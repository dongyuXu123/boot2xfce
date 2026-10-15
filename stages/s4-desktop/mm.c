/* mm.c —— 物理页帧位图（PMM，来自 Multiboot 内存映射）
 *        + 固定区堆 kmalloc/kfree（首次适应 + 后向合并）
 * S2 只做这两个；分页/虚存/按需调页留给 S3。 */
#include "kernel.h"

/* ---------- 物理页帧管理：1 位 = 1 页(4KiB)，0=空闲 1=已占 ---------- */
#define PMM_LIMIT   0x20000000u            /* 管到 512MiB，够教学用 */
#define PMM_FRAMES  (PMM_LIMIT / 4096)
static uint8_t  pmm_bm[PMM_FRAMES / 8];    /* 16KiB，在内核 .bss 里 */
static uint32_t pmm_total;
static uint32_t pmm_hint;                  /* 分配扫描提示，加速 first-fit */                  /* 分配扫描提示，加速 first-fit */

extern uint8_t kernel_end[];               /* kernel.ld 导出的内核映像末尾 */

static inline void bm_set(uint32_t f)  { pmm_bm[f >> 3] |=  (1u << (f & 7)); }
static inline int  bm_get(uint32_t f)  { return pmm_bm[f >> 3] & (1u << (f & 7)); }

static void reserve_range(uint32_t base, uint32_t end) {
    for (uint32_t a = (base + 4095) & ~4095u; a < end; a += 4096)
        bm_set(a / 4096);
}

void pmm_init(struct mb_info *mb) {
    memset(pmm_bm, 0, sizeof(pmm_bm));
    pmm_total = 0;

    if (mb->flags & (1u << 6)) {       /* GRUB 给了 E820 映射（PC 上通常都有） */
        struct mb_mmap *e   = (struct mb_mmap *)(uintptr_t)mb->mmap_addr;
        struct mb_mmap *end = (struct mb_mmap *)(uintptr_t)(mb->mmap_addr + mb->mmap_length);
        for (; (uintptr_t)e < (uintptr_t)end;
             e = (struct mb_mmap *)(uintptr_t)((uintptr_t)e + e->size + 4)) {
            if (e->type != 1) continue;
            uint32_t base = (uint32_t)e->addr;
            uint32_t lim  = (uint32_t)(e->addr + e->len);
            if (lim > PMM_LIMIT) lim = PMM_LIMIT;
            if (base < 0x100000u) base = 0x100000u;    /* 低 1MiB 给 BIOS/实模式 */
            base = (base + 4095) & ~4095u;
            for (uint32_t a = base; a + 4096 <= lim; a += 4096)
                pmm_total++;
        }
    } else if (mb->flags & 1u) {       /* 兜底：只有 mem_upper，按连续可用算 */
        uint32_t lim = 0x100000u + mb->mem_upper * 1024u;
        if (lim > PMM_LIMIT) lim = PMM_LIMIT;
        pmm_total = (lim - 0x100000u) / 4096u;
    } else {
        print("S2: pmm: no memory info from grub!\n");
        return;
    }

    /* 固定保留区：低 1MiB（BIOS/实模式/IVT——不保留的话 alloc 从帧 0
     * 扫起会把 0x0 当可分配页，此 bug 由课程的 alloc/free 断言揪出）
     * + 内核映像 / 堆 / 用户区（代码+栈） */
    reserve_range(0, 0x100000u);
    reserve_range(0x100000u, (uint32_t)(uintptr_t)kernel_end);
    reserve_range(HEAP_START, HEAP_END);
    reserve_range(USER_BASE, USER_BASE + 0x100000u);
}

uint32_t pmm_total_frames(void) { return pmm_total; }

/* 低区分配：只在 [kernel_end, 4MiB) 内找帧。凡"内核要先清零、再映进
 * 任务地址空间"的帧（页表/按需页/COW 副本/ELF 段）都必须走这里——
 * 4MiB 以下属于 PDE0，被所有页目录继承，内核在任意任务视角都能摸到。
 * （教训：S4 影子缓冲把空闲帧推过 4MiB 后，fork 的页表帧落进用户
 *   PDE1 视角盲区，内核 memset 直接缺页。）
 * 代价：低区只有 ~900KB（约 220 帧），教学规模够用。 */
uint32_t low_hint = 0;
uint32_t pmm_alloc_frame_low(void) {
    if (!low_hint) low_hint = ((uint32_t)(uintptr_t)kernel_end + 4095) / 4096u;
    for (uint32_t f = low_hint; f < 0x200000u / 4096u; f++)
        if (!bm_get(f)) { bm_set(f); low_hint = f + 1; return f * 4096u; }
    /* 低区耗尽：返回 0（调用方须检查）。绝不退回高区帧——高区帧在
     * PDE0-only 任务页目录不可见，页表/PD 落上去会在任务视角 memset
     * 时静默失效，fork 卡死/三重故障（S4 画布×3 后首 fork 即挂实测） */
    return 0;
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t f = pmm_hint; f < pmm_total; f++)
        if (!bm_get(f)) { bm_set(f); pmm_hint = f + 1; return f * 4096u; }
    return 0;                                      /* 没了 */
}

/* 高区分配：从 4MiB 起找空帧，绝不触碰 PDE0 低区。画布等"仅内核经
 * boot_pd 恒等别名访问"的大块专用——低区帧留给页表/PD/用户页 */
uint32_t pmm_alloc_frame_high(void) {
    for (uint32_t f = 1024u; f < pmm_total; f++)
        if (!bm_get(f)) { bm_set(f); return f * 4096u; }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t f = addr / 4096;
    if (f < pmm_total) { pmm_bm[f >> 3] &= (uint8_t)~(1u << (f & 7)); pmm_hint = f; }
}

/* ---------- 堆：块头 12B，首次适应，kfree 向后合并 ---------- */
struct hb {
    uint32_t    size;      /* 不含块头 */
    uint32_t    used;
    struct hb  *next;
};
static struct hb *heap_head;

void heap_init(void) {
    heap_head = (struct hb *)HEAP_START;
    heap_head->size = HEAP_END - HEAP_START - sizeof(struct hb);
    heap_head->used = 0;
    heap_head->next = NULL;
}

void *kmalloc(uint32_t n) {
    n = (n + 3) & ~3u;                 /* 4 字节对齐 */
    for (struct hb *b = heap_head; b; b = b->next) {
        if (b->used || b->size < n) continue;
        if (b->size >= n + sizeof(struct hb) + 8) {   /* 够大就切一块 */
            struct hb *r = (struct hb *)((uint8_t *)b + sizeof(struct hb) + n);
            r->size = b->size - n - sizeof(struct hb);
            r->used = 0;
            r->next = b->next;
            b->size = n;
            b->next = r;
        }
        b->used = 1;
        return (uint8_t *)b + sizeof(struct hb);
    }
    return NULL;
}

void kfree(void *p) {
    if (!p) return;
    struct hb *b = (struct hb *)((uint8_t *)p - sizeof(struct hb));
    b->used = 0;
    for (struct hb *c = b; c->next; ) {            /* 向后合并空闲邻居 */
        if (c->used) { c = c->next; continue; }
        struct hb *n = c->next;
        if (n->used) { c = n; continue; }
        c->size += sizeof(struct hb) + n->size;
        c->next  = n->next;
    }
}
