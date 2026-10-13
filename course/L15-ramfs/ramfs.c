/* ramfs.c —— 最小内存盘：固定文件表，每文件 512B 内容
 * 提供 create/write（合一）/read/list；目录与动态容量是 S3 的事。 */
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

void ramfs_init(void) {
    memset(files, 0, sizeof(files));
}

static int name_eq(const char *a, const char *b) {   /* 16 字符内 strcmp */
    for (int i = 0; i < 16; i++) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

static struct rfile *lookup(const char *name) {
    for (int i = 0; i < MAX_FILES; i++)
        if (files[i].used && name_eq(files[i].name, name))
            return &files[i];
    return NULL;
}

int ramfs_write(const char *name, const void *buf, uint32_t len) {
    if (len > FILE_MAX) len = FILE_MAX;
    struct rfile *f = lookup(name);
    if (!f) {                                   /* 不存在则创建 */
        for (int i = 0; i < MAX_FILES; i++) {
            if (files[i].used) continue;
            f = &files[i];
            memset(f, 0, sizeof(*f));
            for (int j = 0; j < 15 && name[j]; j++) f->name[j] = name[j];
            f->used = 1;
            break;
        }
        if (!f) return -1;
    }
    memcpy(f->data, buf, len);
    f->size = len;
    return (int)len;
}

int ramfs_read(const char *name, void *buf, uint32_t max) {
    struct rfile *f = lookup(name);
    if (!f) return -1;
    uint32_t n = f->size < max ? f->size : max;
    memcpy(buf, f->data, n);
    return (int)n;
}

void ramfs_list(void) {
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
