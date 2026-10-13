/* L15 桩 —— 内存盘 ramfs：固定文件表，每文件 512B
 * name_eq/lookup/ramfs_list 给定；你写 init/write/read。
 * 验收：写 "hello,ramfs!\n"(13B) 读回必须逐字节一致。 */
#include "kernel.h"

#define MAX_FILES   8
#define FILE_MAX    512

struct rfile {
    char     name[16];
    int      used;
    uint32_t size;
    uint8_t  data[FILE_MAX];
};
static struct rfile files[MAX_FILES];

static int name_eq(const char *a, const char *b) {   /* 16 字符内 strcmp（给定） */
    for (int i = 0; i < 16; i++) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

static struct rfile *lookup(const char *name) {      /* 给定 */
    for (int i = 0; i < MAX_FILES; i++)
        if (files[i].used && name_eq(files[i].name, name))
            return &files[i];
    return NULL;
}

void ramfs_init(void) {
    /* TODO(你来写·L15): 清空文件表（想想为什么 memset 整个数组就够） */
}

int ramfs_write(const char *name, const void *buf, uint32_t len) {
    /* TODO(你来写·L15):
     * 1) len 超过 FILE_MAX 就截断
     * 2) lookup 已有文件；没有就找空槽创建（名字逐字符拷，注意留 \0）
     * 3) 表满返回 -1；拷数据、记 size，返回写入长度 */
    (void)name; (void)buf; (void)len;
    return -1;
}

int ramfs_read(const char *name, void *buf, uint32_t max) {
    /* TODO(你来写·L15): 找不到返回 -1；拷 min(size,max)，返回实际长度 */
    (void)name; (void)buf; (void)max;
    return -1;
}

void ramfs_list(void) {                              /* 给定 */
    print("S2: ramfs ls:");
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) continue;
        print(" ");
        print(files[i].name);
        print("(");
        print_dec(files[i].size);
        print("B)");
    }
    print("\n");
}
