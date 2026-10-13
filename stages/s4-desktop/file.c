/* file.c —— S3-M5：VFS 分发层 + 每任务 fd 表
 *
 * 设计：vnode = {ops, data}，具体文件系统（现在只有 ramfs，S4+ 有 tty）
 * 给出 read/write 回调；fd 表是任务结构体里的数组，open 占槽 close 释放。
 *
 * 用户指针铁律的落地处：syscall 层先用 uptr_ok 校验范围，再经内核缓冲
 * 与 vnode 往来——"只翻一层"，用户永远不能让内核替它读写内核地址。
 */
#include "kernel.h"

int uptr_ok(uint32_t va, uint32_t n) {
    return va >= USER_BASE && va + n <= USER_BASE + USER_SPAN;
}

/* /dev/tty 在 M6 接入：open 到这个名字时由 tty 层给 vnode */
extern struct vnode *tty_lookup_vnode(void);

static struct vnode *vfs_lookup(const char *kname) {
    if (kname[0] == '/' && kname[1] == 'd' && kname[2] == 'e' && kname[3] == 'v')
        return tty_lookup_vnode();
    return ramfs_lookup_vnode(kname);
}

int fd_open(task_t *t, const char *kname) {
    struct vnode *v = vfs_lookup(kname);
    if (!v) return -1;
    for (int i = 0; i < FD_MAX; i++) {
        if (t->fds[i].used) continue;
        t->fds[i].vn = v;
        t->fds[i].off = 0;
        t->fds[i].used = 1;
        return i;
    }
    return -1;
}

int fd_read(task_t *t, int fd, void *kbuf, uint32_t n) {
    if (fd < 0 || fd >= FD_MAX || !t->fds[fd].used) return -1;
    struct file *f = &t->fds[fd];
    int r = f->vn->ops->read(f->vn, kbuf, n, f->off);
    if (r > 0) f->off += (uint32_t)r;
    return r;
}

int fd_write(task_t *t, int fd, const void *kbuf, uint32_t n) {
    if (fd < 0 || fd >= FD_MAX || !t->fds[fd].used) return -1;
    struct file *f = &t->fds[fd];
    int r = f->vn->ops->write(f->vn, kbuf, n, f->off);
    if (r > 0) f->off += (uint32_t)r;
    return r;
}

int fd_close(task_t *t, int fd) {
    if (fd < 0 || fd >= FD_MAX || !t->fds[fd].used) return -1;
    t->fds[fd].used = 0;
    return 0;
}

int fd_lseek(task_t *t, int fd, int32_t off, int whence) {
    if (fd < 0 || fd >= FD_MAX || !t->fds[fd].used) return -1;
    struct file *f = &t->fds[fd];
    uint32_t base = (whence == 1) ? f->off : 0;      /* 0=SEEK_SET 1=SEEK_CUR */
    f->off = base + (uint32_t)off;
    return (int)f->off;
}
