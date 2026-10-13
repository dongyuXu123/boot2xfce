/* ramfs.c —— 内存文件系统：真目录树
 * 节点=文件或目录；根"/"；路径支持绝对/相对与 . ..
 * write 打开不存在文件时自动补建中间目录（等价 mkdir -p）。
 */
#include "kernel.h"

#define MAX_NODES   32
#define FILE_MAX    16384
#define MAX_KIDS    6

struct rnode {
    char     name[16];
    int      used, is_dir;
    struct rnode *parent, *kids[MAX_KIDS];
    int      nk;
    uint32_t size;
    uint8_t  data[FILE_MAX];
};
static struct rnode nodes[MAX_NODES];
static struct rnode *root;
static struct rnode *fs_cwd;           /* 内核侧当前目录（任务 cwd 见 task.c） */

void ramfs_init(void) {
    memset(nodes, 0, sizeof(nodes));
    root = &nodes[0];
    root->used = 1; root->is_dir = 1; root->name[0] = '/';
    fs_cwd = root;
}

static struct rnode *alloc_node(void) {
    for (int i = 0; i < MAX_NODES; i++) if (!nodes[i].used) return &nodes[i];
    return 0;
}
static int name_eq(const char *a, const char *b) {
    for (int i = 0; i < 16; i++) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}
static struct rnode *find_kid(struct rnode *d, const char *nm) {
    for (int i = 0; i < d->nk; i++)
        if (d->kids[i]->used && name_eq(d->kids[i]->name, nm)) return d->kids[i];
    return 0;
}
static int next_comp(const char **pp, char *out) {   /* 取下一段；0=结束 */
    const char *p = *pp;
    while (*p == '/') p++;
    if (!*p) { *pp = p; return 0; }
    int ci = 0;
    while (*p && *p != '/') { if (ci < 15) out[ci++] = *p; p++; }
    out[ci] = 0;
    *pp = p;
    return 1;
}

/* 沿路径走（绝对/相对；支持 . ..），返回到达的节点；找不到返回 0 */
static struct rnode *resolve(const char *path) {
    struct rnode *cur = (*path == '/') ? root : fs_cwd;
    char comp[16];
    while (next_comp(&path, comp)) {
        if (!cur->is_dir) return 0;
        if (comp[0] == '.' && !comp[1]) continue;
        if (comp[0] == '.' && comp[1] == '.' && !comp[2]) {
            if (cur->parent) cur = cur->parent;
            continue;
        }
        cur = find_kid(cur, comp);
        if (!cur) return 0;
    }
    return cur;
}

/* 走到"末段"的父目录（中间目录自动补建），末段名拷入 tail。 */
static struct rnode *walk_parents(const char *path, char *tail) {
    struct rnode *cur = (*path == '/') ? root : fs_cwd;
    char comp[16];
    for (;;) {
        if (!next_comp(&path, comp)) return 0;
        if (!*path && 0) { }
        /* 看 comp 后面还有没有内容 */
        const char *q = path;
        while (*q == '/') q++;
        if (!*q) {                                   /* comp 即末段 */
            for (int i = 0; i < 16; i++) tail[i] = comp[i];
            return cur;
        }
        if (!cur->is_dir) return 0;
        if (comp[0] == '.' && !comp[1]) continue;
        struct rnode *k = find_kid(cur, comp);
        if (!k) {                                    /* 中间目录自动补建 */
            struct rnode *n = alloc_node();
            if (!n) return 0;
            memset(n, 0, sizeof(*n));
            n->used = 1; n->is_dir = 1; n->parent = cur;
            for (int i = 0; i < 15 && comp[i]; i++) n->name[i] = comp[i];
            if (cur->nk < MAX_KIDS) cur->kids[cur->nk++] = n;
            k = n;
        }
        if (!k->is_dir) return 0;
        cur = k;
    }
}

struct rnode *ramfs_lookup(const char *path) { return resolve(path); }

int ramfs_mkdir(const char *path) {
    char tail[16];
    struct rnode *d = walk_parents(path, tail);
    if (!d || !d->is_dir || !tail[0]) return -1;
    if (find_kid(d, tail)) return -1;                /* 已存在 */
    struct rnode *n = alloc_node();
    if (!n) return -1;
    memset(n, 0, sizeof(*n));
    n->used = 1; n->is_dir = 1; n->parent = d;
    for (int i = 0; i < 15 && tail[i]; i++) n->name[i] = tail[i];
    if (d->nk < MAX_KIDS) d->kids[d->nk++] = n;
    return 0;
}

int ramfs_rm(const char *path) {
    struct rnode *n = resolve(path);
    if (!n || n == root) return -1;
    if (n->is_dir && n->nk) return -1;               /* 目录非空 */
    struct rnode *par = n->parent;
    for (int i = 0; i < par->nk; i++)
        if (par->kids[i] == n) {
            for (int j = i; j < par->nk - 1; j++) par->kids[j] = par->kids[j + 1];
            par->nk--;
            break;
        }
    memset(n, 0, sizeof(*n));
    return 0;
}

int ramfs_chdir(const char *path) {
    struct rnode *n = resolve(path);
    if (!n || !n->is_dir) return -1;
    fs_cwd = n;
    return 0;
}

int ramfs_read(const char *path, void *buf, uint32_t max) {
    struct rnode *n = resolve(path);
    if (!n || n->is_dir) return -1;
    uint32_t nn = n->size < max ? n->size : max;
    memcpy(buf, n->data, nn);
    return (int)nn;
}

int ramfs_write(const char *path, const void *buf, uint32_t len) {
    char tail[16];
    struct rnode *d = walk_parents(path, tail);
    if (!d || !tail[0]) return -1;
    struct rnode *f = find_kid(d, tail);
    if (!f) {
        f = alloc_node();
        if (!f) return -1;
        memset(f, 0, sizeof(*f));
        f->used = 1; f->parent = d;
        for (int i = 0; i < 15 && tail[i]; i++) f->name[i] = tail[i];
        if (d->nk < MAX_KIDS) d->kids[d->nk++] = f;
    }
    if (f->is_dir) return -1;
    if (len > FILE_MAX) len = FILE_MAX;
    memcpy(f->data, buf, len);
    f->size = len;
    return (int)len;
}

void ramfs_list(const char *path) {
    struct rnode *d = resolve(path && path[0] ? path : ".");
    if (!d || !d->is_dir) { print("S3: ls: not a dir\n"); return; }
    print("S3: ls ");
    if (d == root) print("/"); else print(d->name);
    print(":");
    for (int i = 0; i < d->nk; i++) {
        print(" ");
        print(d->kids[i]->name);
        if (d->kids[i]->is_dir) print("/");
        else { print("("); print_dec(d->kids[i]->size); print("B)"); }
    }
    print("\n");
}

int ramfs_getcwd(char *buf, int max) {
    struct rnode *n = fs_cwd;
    char rev[256]; int rl = 0;
    while (n && n != root) {
        int l = 0; while (n->name[l]) l++;
        rev[rl++] = '/';
        for (int j = l; j > 0; j--) rev[rl++] = n->name[j - 1];
        n = n->parent;
    }
    int oi = 0;
    buf[oi++] = '/';
    for (int j = rl - 1; j >= 0; j--) buf[oi++] = rev[j];
    buf[oi] = 0;
    return oi;
}

/* ---------- VFS 挂接：文件节点包装成 vnode ---------- */
static int rv_read(struct vnode *v, void *buf, uint32_t len, uint32_t off) {
    struct rnode *n = v->data;
    if (off >= n->size) return 0;
    if (off + len > n->size) len = n->size - off;
    memcpy(buf, n->data + off, len);
    return (int)len;
}
static int rv_write(struct vnode *v, const void *buf, uint32_t len, uint32_t off) {
    struct rnode *n = v->data;
    if (off + len > FILE_MAX) len = FILE_MAX - off;
    memcpy(n->data + off, buf, len);
    if (off + len > n->size) n->size = off + len;
    return (int)len;
}
static const struct file_ops ramfs_ops = { rv_read, rv_write };
static struct vnode rv_node;

struct vnode *ramfs_lookup_vnode(const char *path) {
    struct rnode *n = resolve(path);
    if (!n || n->is_dir) return 0;
    rv_node.ops = &ramfs_ops;
    rv_node.data = n;
    return &rv_node;
}
