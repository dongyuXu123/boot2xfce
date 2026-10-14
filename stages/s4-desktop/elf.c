/* elf.c —— S3-M4：ELF 装载器 + execve 支持
 *
 * 32 位 ELF 只读装载：校验魔数 → 遍历 PT_LOAD → 每段按页分配物理帧
 * （先清零，bss 天生为 0）→ 映进目标页目录 → 经"恒等别名"拷贝文件字节。
 *
 * 恒等别名技巧：内核自己的页目录把低 128MiB 恒等映射（虚=物），所以往
 * (void*)物理地址 写 = 写进那张用户页——不必在内核里临时挂用户页表。
 *
 * execve 原位换像：syscall 现场里就攥着 int 0x80 的 iret 帧（regs*），
 * 把 eip/esp 改写成新程序的入口和新栈，iretd 一执行就落到新程序——
 * 任务没换、pid 没换、内核栈没换，只有地址空间和用户现场换了。
 */
#include "kernel.h"

#define PG_P    0x001
#define PG_RW   0x002
#define PG_US   0x004
#define PG_COW  0x200
#define PD_INDEX(va) (((va) >> 22) & 0x3FF)
#define PT_INDEX(va) (((va) >> 12) & 0x3FF)

struct elf_ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version, e_entry, e_phoff, e_shoff, e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum;
} __attribute__((packed));

struct elf_phdr {
    uint32_t p_type, p_offset, p_vaddr, p_paddr;
    uint32_t p_filesz, p_memsz, p_flags, p_align;
} __attribute__((packed));

#define PT_LOAD 1

/* 把 img 指向的 ELF 镜像装进 pd；成功返回 0 并给出入口地址 */
static int elf_load_pd(uint32_t *pd, const uint8_t *img, uint32_t size, uint32_t *entry) {
    if (size < sizeof(struct elf_ehdr)) return -1;
    const struct elf_ehdr *eh = (const struct elf_ehdr *)img;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') return -1;
    if (eh->e_ident[0] != 0x7F) { print("[dbg] bad magic\n"); return -1; }
    if (eh->e_phentsize != sizeof(struct elf_phdr)) { print("[dbg] bad phentsize\n"); return -1; }

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const struct elf_phdr *ph = (const struct elf_phdr *)
            (img + eh->e_phoff + (uint32_t)i * eh->e_phentsize);
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_memsz == 0) continue;
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_vaddr < USER_BASE || ph->p_vaddr + ph->p_memsz > USER_BASE + USER_SPAN) {
            return -1;
        }

        /* 按页分配（清零：bss 就位）并映进 pd */
        for (uint32_t va = ph->p_vaddr & ~0xFFFu;
             va < ph->p_vaddr + ph->p_memsz; va += 4096) {
            if (page_va2pa(pd, va)) continue;            /* 已有页不重复盖 */
            uint32_t pa = pmm_alloc_frame_low();
            if (!pa) return -1;
            memset((void *)pa, 0, 4096);
            page_map(pd, va, pa, 1, 1);
        }
        /* 文件字节经恒等别名写入（虚=物，直接写物理页） */
        for (uint32_t off = 0; off < ph->p_filesz; off += 4096) {
            uint32_t va  = ph->p_vaddr + off;
            uint32_t pa  = page_va2pa(pd, va & ~0xFFFu);
            uint32_t skip = va & 0xFFF;
            uint32_t len  = 4096 - skip;
            if (off + len > ph->p_filesz) len = ph->p_filesz - off;
            memcpy((void *)(pa + skip), img + ph->p_offset + off, len);
        }
    }
    *entry = eh->e_entry;
    return 0;
}

/* 清掉页目录里的用户区（内核共享区不动）——execve 换像前调用 */
void page_dir_clear_user(uint32_t *pd) {
    for (uint32_t i = PD_INDEX(USER_BASE); i < 1024; i++) {
        if (!(pd[i] & PG_P)) continue;
        uint32_t *pt = (uint32_t *)(pd[i] & ~0xFFF);
        for (uint32_t j = 0; j < 1024; j++)
            if (pt[j] & PG_P) {
                uint32_t pa = pt[j] & ~0xFFF;
                extern uint32_t task_frame_unref(uint32_t pa);
                if (task_frame_unref(pa) == 0) pmm_free_frame(pa);
            }
        pmm_free_frame(pd[i] & ~0xFFF);
        pd[i] = 0;
    }
    if (pd == active_pd) reload_cr3();
}

/* 给任务装载 name 指定的程序（ramfs 里的 ELF）。
 * fresh 任务（还没跑）与 execve（已在跑）共用；成功返回 0。 */
int task_exec_image(task_t *t, const char *name) {
    static uint8_t img[16384];
    int n = ramfs_read(name, img, sizeof(img));
    if (n <= 0) return -1;

    page_dir_clear_user(t->pd);

    uint32_t entry = 0;
    if (elf_load_pd(t->pd, img, (uint32_t)n, &entry)) return -1;

    /* 用户栈：栈顶往下 4 页 */
    for (uint32_t va = USER_TOP - 0x4000; va < USER_TOP; va += 4096) {
        uint32_t pa = pmm_alloc_frame_low();
        if (!pa) return -1;
        memset((void *)pa, 0, 4096);
        page_map(t->pd, va, pa, 1, 1);
    }
    t->ueip = entry;
    t->uesp = USER_TOP;

    /* 改写"首次运行"预制 iret 帧（execve 路径由 syscall 改 int80 现场） */
    struct context *ctx = (struct context *)(uintptr_t)
                          (t->kstack_top - sizeof(struct context));
    uint32_t *frame = (uint32_t *)(uintptr_t)((uint8_t *)ctx - 20);
    frame[0] = entry;
    frame[3] = USER_TOP;

    if (t->pd == active_pd) reload_cr3();
    return 0;
}
