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
#include <sys/time.h>

#define XOR_KEY 0x76

static char enc_storage[4096];

static void xor_data(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++)
        buf[i] ^= XOR_KEY;
}

static void get_enc_path(char *out, const char *path) {
    const char *base = strrchr(path, '/');
    if (!base) base = path;
    char dir[4096];
    int dlen = base - path;
    strncpy(dir, path, dlen);
    dir[dlen] = '\0';
    snprintf(out, 4096, "%s%s/%s.enc", enc_storage, dir, base + 1);
}

static void get_dir_path(char *out, const char *path) {
    snprintf(out, 4096, "%s%s", enc_storage, path);
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    char ep[4096], dp[4096];
    get_enc_path(ep, path);
    get_dir_path(dp, path);
    if (lstat(ep, stbuf) == 0) return 0;
    if (lstat(dp, stbuf) == 0) return 0;
    return -ENOENT;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi) {
    (void) offset; (void) fi;
    char dp[4096];
    get_dir_path(dp, path);
    DIR *d = opendir(dp);
    if (!d) return -errno;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        char name[512];
        strncpy(name, de->d_name, sizeof(name));
        if (de->d_type == DT_REG) {
            int len = strlen(name);
            if (len > 4 && strcmp(name + len - 4, ".enc") == 0)
                name[len - 4] = '\0';
        }
        if (filler(buf, name, &st, 0)) break;
    }
    closedir(d);
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char ep[4096];
    get_enc_path(ep, path);
    int res = open(ep, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char ep[4096];
    get_enc_path(ep, path);
    int fd = open(ep, O_RDONLY);
    if (fd == -1) return -errno;
    int res = pread(fd, buf, size, offset);
    close(fd);
    if (res == -1) return -errno;
    xor_data(buf, res);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char ep[4096];
    get_enc_path(ep, path);
    char *tmp = malloc(size);
    if (!tmp) return -ENOMEM;
    memcpy(tmp, buf, size);
    xor_data(tmp, size);
    int fd = open(ep, O_WRONLY);
    if (fd == -1) { free(tmp); return -errno; }
    int res = pwrite(fd, tmp, size, offset);
    close(fd);
    free(tmp);
    if (res == -1) return -errno;
    return res;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char ep[4096];
    get_enc_path(ep, path);
    int fd = open(ep, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

static int xmp_unlink(const char *path) {
    char ep[4096];
    get_enc_path(ep, path);
    if (unlink(ep) == 0) return 0;
    char dp[4096];
    get_dir_path(dp, path);
    if (unlink(dp) == 0) return 0;
    return -errno;
}

static int xmp_mkdir(const char *path, mode_t mode) {
    char dp[4096];
    get_dir_path(dp, path);
    if (mkdir(dp, mode) == -1) return -errno;
    return 0;
}

static int xmp_rmdir(const char *path) {
    char dp[4096];
    get_dir_path(dp, path);
    if (rmdir(dp) == -1) return -errno;
    return 0;
}

static int xmp_truncate(const char *path, off_t size) {
    char ep[4096];
    get_enc_path(ep, path);
    if (truncate(ep, size) == 0) return 0;
    char dp[4096];
    get_dir_path(dp, path);
    if (truncate(dp, size) == 0) return 0;
    return -errno;
}

static int xmp_access(const char *path, int mask) {
    char ep[4096], dp[4096];
    get_enc_path(ep, path);
    get_dir_path(dp, path);
    if (access(ep, mask) == 0) return 0;
    if (access(dp, mask) == 0) return 0;
    return -errno;
}

static int xmp_utimens(const char *path, const struct timespec ts[2]) {
    char ep[4096];
    get_enc_path(ep, path);
    struct timeval tv[2];
    tv[0].tv_sec = ts[0].tv_sec; tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec; tv[1].tv_usec = ts[1].tv_nsec / 1000;
    if (utimes(ep, tv) == 0) return 0;
    char dp[4096];
    get_dir_path(dp, path);
    if (utimes(dp, tv) == 0) return 0;
    return -errno;
}

static struct fuse_operations xmp_oper = {
    .getattr  = xmp_getattr,
    .readdir  = xmp_readdir,
    .open     = xmp_open,
    .read     = xmp_read,
    .write    = xmp_write,
    .create   = xmp_create,
    .unlink   = xmp_unlink,
    .mkdir    = xmp_mkdir,
    .rmdir    = xmp_rmdir,
    .truncate = xmp_truncate,
    .access   = xmp_access,
    .utimens  = xmp_utimens,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <encrypted_storage> <mount_point> [fuse_options]\n", argv[0]);
        return 1;
    }

    // Ambil source dir dari argv[1]
    if (realpath(argv[1], enc_storage) == NULL) {
        perror("realpath");
        return 1;
    }

    // Buat argv baru: [program, mount_point, -o, allow_other, NULL]
    // Ini memastikan allow_other selalu aktif tanpa perlu dipass manual
    char *new_argv[8];
    int new_argc = 0;
    new_argv[new_argc++] = argv[0];       // nama program
    new_argv[new_argc++] = argv[2];       // mount_point
    new_argv[new_argc++] = "-o";
    new_argv[new_argc++] = "allow_other";

    // Tambahkan opsi tambahan jika ada (argv[3] dst)
    for (int i = 3; i < argc && new_argc < 7; i++)
        new_argv[new_argc++] = argv[i];
    new_argv[new_argc] = NULL;

    umask(0);
    return fuse_main(new_argc, new_argv, &xmp_oper, NULL);
}
