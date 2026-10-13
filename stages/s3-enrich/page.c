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

uint32_t *active_pd = 0;        /* 正在用的页目录（load_cr3 顺手记录） */

static uint32_t *boot_pd;       /* 内核启动页目录（所有任务共用其内核部分） */

/* ---------- 页目录生命周期 ---------- */
uint32_t *page_dir_create(void) {
    uint32_t pd_pa = pmm_alloc_frame();
    uint32_t *pd = (uint32_t *)pd_pa;
    memset(pd, 0, 4096);
    /* 内核区（低 1GiB 的恒等映射）整表继承自 boot_pd：
     * 直接拷 0..USER_BASE 段的所有 PDE——内核映射每任务一份影子 */
    for (uint32_t i = 0; i < PD_INDEX(USER_BASE); i++)
        pd[i] = boot_pd[i];
    return pd;
}

void page_dir_destroy(uint32_t *pd) {
    /* 释放用户区页表与用户页（内核区共享，不动）。M2/M3 接管调用。 */
    for (uint32_t i = PD_INDEX(USER_BASE); i < 1024; i++) {
        if (!(pd[i] & PG_P)) continue;
        uint32_t *pt = (uint32_t *)(pd[i] & ~0xFFF);
        for (uint32_t j = 0; j < 1024; j++)
            if (pt[j] & PG_P)
                pmm_free_frame(pt[j] & ~0xFFF);
        pmm_free_frame(pd[i] & ~0xFFF);
    }
    pmm_free_frame((uint32_t)(uintptr_t)pd);
}

/* ---------- 映射 ---------- */
/* va 处的 PTE 指针，必要时（create=1）现场建页表；返回 0=失败 */
static uint32_t *walk(uint32_t *pd, uint32_t va, int create) {
    uint32_t i = PD_INDEX(va);
    if (!(pd[i] & PG_P)) {
        if (!create) return 0;
        uint32_t pt_pa = pmm_alloc_frame();
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
    /* 低 16MiB：内核 + 堆 + 用户窗口区，恒等映射且放开 U/S
     * （M1 尚无每任务地址空间，语义与 S2 的"无隔离"一致；
     *   M2 起用户窗口改由各任务自己的页目录提供，内核区收回 U/S）
     * [16MiB, 128MiB)：其余物理 RAM 仅供内核恒等访问（supervisor）。
     * 内核要清零任何物理帧（页表/用户页），都得先被映射到。 */
    for (uint32_t pa = 0; pa < 0x1000000u; pa += 4096)
        page_map(boot_pd, pa, pa, 1, 1);
    for (uint32_t pa = 0x1000000u; pa < 0x8000000u; pa += 4096)
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
    if (!(err & 1) && va >= USER_BASE && va < USER_BASE + USER_SPAN) {
        /* 按需零页：bss/栈的延迟就位（M3 在此之前插 COW 判定） */
        uint32_t pa = pmm_alloc_frame();
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
