/* L12 桩 —— 物理页帧位图 PMM（堆是给定成果，L13 才轮到你写它）
 * 1 位 = 1 页(4KiB)：0=空闲 1=已占。输入是 GRUB 的 E820 mmap。
 * 验收：连发两页必须不同且 4K 对齐，释放后重发必须拿回同一页。 */
#include "kernel.h"

#define PMM_LIMIT   0x20000000u            /* 管到 512MiB，够教学用 */
#define PMM_FRAMES  (PMM_LIMIT / 4096)
static uint8_t  pmm_bm[PMM_FRAMES / 8];    /* 16KiB，在内核 .bss 里 */
static uint32_t pmm_total;
static uint32_t pmm_hint;                  /* 分配扫描提示，加速 first-fit */

extern uint8_t kernel_end[];               /* kernel.ld 导出的内核映像末尾 */

static inline void bm_set(uint32_t f)  { pmm_bm[f >> 3] |=  (1u << (f & 7)); }
static inline int  bm_get(uint32_t f)  { return pmm_bm[f >> 3] & (1u << (f & 7)); }

static void reserve_range(uint32_t base, uint32_t end) {
    for (uint32_t a = (base + 4095) & ~4095u; a < end; a += 4096)
        bm_set(a / 4096);
}

void pmm_init(struct mb_info *mb) {
    /* TODO(你来写·L12):
     * 1) 位图清零、计数清零
     * 2) 遍历 mmap（信息结构 flags bit6；type==1 才可用）
     *    base 截到 >=1MiB、按 4K 向上取整；lim 截到 PMM_LIMIT
     *    逐页数出 pmm_total
     * 3) 保留三块固定区：内核镜像[1M,kernel_end]、堆[HEAP_START,HEAP_END)、
     *    用户区[USER_BASE, USER_BASE+1M)
     * 大坑：mmap 记录步进 = e->size + 4（L07 你应该已经疼过了） */
    (void)mb;
}

uint32_t pmm_total_frames(void) {
    return 0;                    /* TODO: 替换我 */
}

uint32_t pmm_alloc_frame(void) {
    /* TODO(你来写·L12): 从 pmm_hint 起找第一个 0 位，置 1 返回页地址；没了返回 0 */
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    /* TODO(你来写·L12): 清位，并把 pmm_hint 拉回来加速复用 */
    (void)addr;
}

/* ---------- 堆（给定成果：首次适应 + 分裂 + 后向合并，L13 课重写） ---------- */
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
    n = (n + 3) & ~3u;
    for (struct hb *b = heap_head; b; b = b->next) {
        if (b->used || b->size < n) continue;
        if (b->size >= n + sizeof(struct hb) + 8) {
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
    for (struct hb *c = b; c->next; ) {
        if (c->used) { c = c->next; continue; }
        struct hb *n = c->next;
        if (n->used) { c = n; continue; }
        c->size += sizeof(struct hb) + n->size;
        c->next  = n->next;
    }
}
