#define FUSE_USE_VERSION 28
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

static char source_dir[2048];

static void get_full_path(char *full_path, const char *path) {
    snprintf(full_path, 2048, "%s%s", source_dir, path);
}

static void build_tujuan(char *out, size_t *out_len) {
    char result[4096] = "Tujuan Mas Amba: ";
    int first = 1;
    for (int i = 1; i <= 7; i++) {
        char filepath[4096];
        sprintf(filepath, "%s/%d.txt", source_dir, i);
        FILE *fp = fopen(filepath, "r");
        if (!fp) continue;
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "KOORD:", 6) == 0) {
                char *value = line + 7;
                value[strcspn(value, "\n")] = '\0';
                if (!first) strcat(result, " ");
                strcat(result, value);
                first = 0;
                break;
            }
        }
        fclose(fp);
    }
    strcat(result, "\n");
    strcpy(out, result);
    *out_len = strlen(result);
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        char content[4096];
        size_t len;
        build_tujuan(content, &len);
        stbuf->st_size = len;
        return 0;
    }
    char full_path[2048];
    get_full_path(full_path, path);
    int res = lstat(full_path, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi) {
    (void) offset; (void) fi;
    char full_path[2048];
    get_full_path(full_path, path);
    DIR *dp = opendir(full_path);
    if (dp == NULL) return -errno;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0)) break;
    }
    closedir(dp);
    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        filler(buf, "tujuan.txt", &st, 0);
    }
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) return 0;
    char full_path[2048];
    get_full_path(full_path, path);
    int res = open(full_path, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    if (strcmp(path, "/tujuan.txt") == 0) {
        char content[4096];
        size_t len;
        build_tujuan(content, &len);
        if (offset >= (off_t)len) return 0;
        size_t to_copy = len - offset;
        if (to_copy > size) to_copy = size;
        memcpy(buf, content + offset, to_copy);
        return to_copy;
    }
    char full_path[2048];
    get_full_path(full_path, path);
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    close(fd);
    return res;
}

static int xmp_access(const char *path, int mask) {
    if (strcmp(path, "/tujuan.txt") == 0) return 0;
    char full_path[2048];
    get_full_path(full_path, path);
    int res = access(full_path, mask);
    if (res == -1) return -errno;
    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .open    = xmp_open,
    .read    = xmp_read,
    .access  = xmp_access,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_dir>\n", argv[0]);
        return 1;
    }
    if (realpath(argv[1], source_dir) == NULL) {
        perror("realpath");
        return 1;
    }
    argv[1] = argv[2];
    argc--;
    umask(0);
    return fuse_main(argc, argv, &xmp_oper, NULL);
}
