/* L16 桩 —— int 0x80 系统调用分发器
 * 约定：eax=调用号，ebx/ecx/edx=参数，eax=返回值。
 *   1 write(fd=1, buf, len)  2 getpid()  3 exit(code)
 *   4 uptime()               5 ls()
 * 桩把 eax 统一置 -1：没有任何标记会被打印，test 必红。 */
#include "kernel.h"

void syscall_dispatch(struct regs *r) {
    switch (r->eax) {
    case 1: {                                   /* write */
        /* TODO(你来写·L16):
         * 1) n = min(r->edx, 255)
         * 2) memcpy 到内核 kbuf，补 '\0'
         * 3) print(kbuf)；r->eax = n
         * （无分页=同一地址空间，用户指针直用；S3 开分页后这里必须换
         *   "只翻一层"的拷贝语义，防用户传内核指针） */
        static char kbuf[256];
        r->eax = (uint32_t)-1;
        break;
    }
    case 2:                                     /* getpid */
        /* TODO: r->eax = current->pid */
        r->eax = (uint32_t)-1;
        break;
    case 3:                                     /* exit */
        /* TODO: current->state = T_DEAD（想想为什么这里不能 kfree 自己的栈），
         * 打印 "S2: [user] exit, pid=<pid>\n"，r->eax=0 */
        r->eax = (uint32_t)-1;
        break;
    case 4:                                     /* uptime */
        /* TODO: r->eax = ticks */
        r->eax = (uint32_t)-1;
        break;
    case 5:                                     /* ls */
        /* TODO: ramfs_list(); r->eax = 0 */
        r->eax = (uint32_t)-1;
        break;
    default:
        r->eax = (uint32_t)-1;
        break;
    }
}
