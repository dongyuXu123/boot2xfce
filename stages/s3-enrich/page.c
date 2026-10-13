/* page.c —— S3-M1 两级分页：10+10+12
 *
 * 设计（教学内核）：
 *  - 内核恒等映射（虚=物），所有任务的页目录都含同一份内核映射；
 *  - 用户空间 [USER_BASE, USER_BASE+USER_SPAN) 按任务隔离（M2 起每任务一份目录）；
 *  - 页表/页目录本身从 PMM 拿帧——内核恒等映射下物理地址可直接访问；
 *  - 缺页处理：用户区 !present → 按需分配零页（bss/栈延迟就位的根基）；
 *    M3 再加 COW 写拷贝。
 * 大坑（linux011 血泪）：写完 PTE 后 TLB 可能还缓存旧表项——映射变更后
 * 要么 invlpg 要么重载 CR3。
 */
#include "kernel.h"

#define PG_P    0x001           /* present */
#define PG_RW   0x002           /* R/W */
#define PG_US   0x004           /* user/supervisor */
#define PG_COW  0x200           /* 软件位：copy-on-write 标记（M3） */

#define PD_INDEX(va) (((va) >> 22) & 0x3FF)
#define PT_INDEX(va) (((va) >> 12) & 0x3FF)

uint32_t *active_pd = 0;

/* ---------- 物理帧引用计数（COW 共享页的生命线） ---------- */
#define FRAME_MAX 32768                /* 128MiB/4KiB（QEMU 全部物理内存）*/
static uint8_t frame_ref[FRAME_MAX];     /* 32KiB，.bss 清零即全 0 */
static uint32_t ref_get(uint32_t pa) { return frame_ref[pa / 4096]; }
static void ref_inc(uint32_t pa)     { frame_ref[pa / 4096]++; }
static uint32_t ref_dec(uint32_t pa) { return --frame_ref[pa / 4096]; }
uint32_t task_frame_unref(uint32_t pa) { return ref_dec(pa); }   /* 供 elf.c 用 */        /* 正在用的页目录（load_cr3 顺手记录） */

static uint32_t *boot_pd;       /* 内核启动页目录（所有任务共用其内核部分） */

/* ---------- 页目录生命周期 ---------- */
uint32_t *page_dir_create(void) {
    uint32_t pd_pa = pmm_alloc_frame();
    uint32_t *pd = (uint32_t *)pd_pa;
    memset(pd, 0, 4096);
    /* 内核区整表继承自 boot_pd，但【跳过用户窗口的 PDE1-4】——
     * 那四项在任务里必须是任务自己的用户页表，绝不能带上内核的
     * 恒等映射视图（否则用户态能看见别人的物理内存） */
    for (uint32_t i = 0; i < 1024; i++) {
        if (i >= PD_INDEX(USER_BASE) && i < PD_INDEX(USER_BASE + USER_SPAN)) continue;
        pd[i] = boot_pd[i];
    }
    return pd;
}

void page_dir_destroy(uint32_t *pd) {
    /* 释放用户区页表与用户页。共享帧（COW/代码）只减引用，归零才回收。 */
    for (uint32_t i = PD_INDEX(USER_BASE); i < 1024; i++) {
        if (!(pd[i] & PG_P)) continue;
        uint32_t *pt = (uint32_t *)(pd[i] & ~0xFFF);
        for (uint32_t j = 0; j < 1024; j++)
            if (pt[j] & PG_P) {
                uint32_t pa = pt[j] & ~0xFFF;
                if (ref_dec(pa) == 0) pmm_free_frame(pa);
            }
        pmm_free_frame(pd[i] & ~0xFFF);
    }
    pmm_free_frame((uint32_t)(uintptr_t)pd);
}

/* fork 的地址空间复制：父的可写页转 COW 共享，子映射同一物理帧；
 * 本就只读的页（代码）直接共享。计数同步 +1。 */
void page_copy_space(uint32_t *dst, uint32_t *src) {
    for (uint32_t i = PD_INDEX(USER_BASE); i < 1024; i++) {
        if (!(src[i] & PG_P)) continue;
        uint32_t *spt = (uint32_t *)(src[i] & ~0xFFF);
        /* 子的页表也直接共享同一张（项级 COW 时再改）——共享页表必须
         * 只读，且页目录项不可再被 walk 的 create 路径改写：
         * 教学简化：给子单独复制页表结构，逐项共享帧 */
        uint32_t pt_pa = pmm_alloc_frame_low();
        memset((void *)pt_pa, 0, 4096);
        dst[i] = pt_pa | PG_P | PG_RW | PG_US;
        uint32_t *dpt = (uint32_t *)pt_pa;
        for (uint32_t j = 0; j < 1024; j++) {
            if (!(spt[j] & PG_P)) continue;
            uint32_t pa = spt[j] & ~0xFFF;
            uint32_t attr = spt[j] & 0xFFF;
            if (attr & PG_RW) {                 /* 可写 → 转 COW 共享 */
                spt[j] = pa | (attr & ~PG_RW) | PG_COW;
                dpt[j] = pa | (attr & ~PG_RW) | PG_COW;
            } else {                            /* 只读（代码）直接共享 */
                dpt[j] = spt[j];
            }
            ref_inc(pa);
        }
    }
    if (src == active_pd) reload_cr3();         /* 父的 PTE 改了，TLB 要刷 */
}

/* ---------- 映射 ---------- */
/* va 处的 PTE 指针，必要时（create=1）现场建页表；返回 0=失败 */
static uint32_t *walk(uint32_t *pd, uint32_t va, int create) {
    uint32_t i = PD_INDEX(va);
    if (!(pd[i] & PG_P)) {
        if (!create) return 0;
        uint32_t pt_pa = pmm_alloc_frame_low();
        if (!pt_pa) return 0;
        memset((void *)pt_pa, 0, 4096);
        pd[i] = pt_pa | PG_P | PG_RW | (va >= USER_BASE ? PG_US : 0);
    }
    return &((uint32_t *)(pd[i] & ~0xFFF))[PT_INDEX(va)];
}

void page_map(uint32_t *pd, uint32_t va, uint32_t pa, int user, int rw) {
    uint32_t *pte = walk(pd, va, 1);
    *pte = (pa & ~0xFFF) | PG_P | (rw ? PG_RW : 0) | (user ? PG_US : 0);
    if (pd == current_pd())          /* 改的是正在用的表 → TLB 要刷新 */
        reload_cr3();
}

void page_unmap(uint32_t *pd, uint32_t va) {
    uint32_t *pte = walk(pd, va, 0);
    if (pte && (*pte & PG_P)) {
        *pte = 0;
        if (pd == current_pd()) reload_cr3();
    }
}

uint32_t page_va2pa(uint32_t *pd, uint32_t va) {
    uint32_t *pte = walk(pd, va, 0);
    return (pte && (*pte & PG_P)) ? ((*pte) & ~0xFFF) | (va & 0xFFF) : 0;
}

/* ---------- 启动与缺页 ---------- */
void paging_init(void) {
    boot_pd = (uint32_t *)pmm_alloc_frame();
    memset(boot_pd, 0, 4096);
    /* 全部 128MiB 恒等映射、一律 supervisor——这是"内核视角"的页目录：
     * 用户页（US=1）只存在于各任务自己的页目录里。
     * （曾把低 16MiB 放开 US：page_dir_create 继承内核区后，用户任务
     *   可以透过恒等映射直写内核——隔离形同虚设，S4 收回） */
    for (uint32_t pa = 0; pa < 0x8000000u; pa += 4096)
        page_map(boot_pd, pa, pa, 0, 1);
    load_cr3((uint32_t)boot_pd);
    enable_paging();
}

uint32_t *current_pd(void) {
    return active_pd;
}

/* 内核共享页目录（内核任务与 fork 模板用） */
uint32_t *page_kernel_pd(void) {
    return boot_pd;
}

/* 缺页：err bit0=present bit1=write bit2=user */
void page_fault_handler(uint32_t err, uint32_t va) {
    /* COW 写拷贝：写一个"在场但只读+COW"的用户页 */
    if ((err & 1) && (err & 2) && va >= USER_BASE && va < USER_BASE + USER_SPAN) {
        uint32_t *pte = walk(active_pd, va, 0);
        if (pte && (*pte & PG_P) && (*pte & PG_COW)) {
            uint32_t pa = *pte & ~0xFFF;
            if (ref_get(pa) > 1) {              /* 还有人在共享 → 复制一份 */
                uint32_t npa = pmm_alloc_frame_low();
                if (!npa) goto panic;
                memcpy((void *)npa, (void *)pa, 4096);
                ref_dec(pa);
                *pte = npa | ((*pte) & 0x1FF & ~PG_COW) | PG_RW;
            } else {                            /* 只剩我一个 → 直接转可写 */
                *pte = (*pte & ~PG_COW) | PG_RW;
            }
            reload_cr3();
            return;
        }
    }
    if (!(err & 1) && va >= USER_BASE && va < USER_BASE + USER_SPAN) {
        /* 按需零页：bss/栈的延迟就位（M3 在此之前插 COW 判定） */
        uint32_t pa = pmm_alloc_frame_low();
        if (!pa) goto panic;
        memset((void *)pa, 0, 4096);
        page_map(active_pd, va & ~0xFFFu, pa, 1, 1);
        return;
    }
panic:
    cli();
    print("\nS3: PAGE FAULT err=");
    print_hex(err);
    print(" va=");
    print_hex(va);
    print("\n");
    for (;;) hlt();
}
