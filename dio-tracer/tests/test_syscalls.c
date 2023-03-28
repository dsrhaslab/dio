/**
 * @file renameat.c
 * @author Tânia Esteves (github.com/taniaesteves)
 * @brief This program tests several system calls
 * @version 0.1
 * @date 2022-06-15
 *
 * @copyright Copyright (c) 2022
 *
 * Compile: gcc tests/test_syscalls.c -o tests/test_syscalls -Doff64_t=_off64_t
 */
#define _GNU_SOURCE

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/xattr.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>

#define XATTR_SIZE 10000
#define BUF_SIZE 4096
#define SOCKET_PORT 5000
#define SOCKET_NAME "/tmp/9Lq7BNBnBycd6nxy.socket"

typedef struct args {
    char *name;
    void *value;
    char type;
} ARG;

void print_header(char *syscall, ARG args[], int n_args) {
    char *buffer = malloc(sizeof(char)*1024);
    int size = 0;
    size = sprintf(buffer,"\n----------------------------------\n");
    size += sprintf(buffer+size,"System call: %s\n", syscall);

    if (n_args > 0) {
        size += sprintf(buffer+size,"Args:\n");

        for (int i = 0; i < n_args; i++) {
            switch (args[i].type) {
                case 'c': {
                    size += sprintf(buffer+size,"\t%s: %c\n", args[i].name, *((char*)args[i].value));
                    break;
                }
                case 's': {
                    size += sprintf(buffer+size,"\t%s: %s\n", args[i].name, (char*)args[i].value);
                    break;
                }
                case 'd': {
                    size += sprintf(buffer+size,"\t%s: %d\n", args[i].name, *((int*)args[i].value));
                    break;
                }
                case 'l': {
                    size += sprintf(buffer+size,"\t%s: %ld\n", args[i].name, *((long*)args[i].value));
                    break;
                }
                case 'u': {
                    size += sprintf(buffer+size,"\t%s: %u\n", args[i].name, *((unsigned int*)args[i].value));
                    break;
                }
                default:
                    size += sprintf(buffer+size,"\t%s: *\n", args[i].name);
                    break;
            }
        }
    }

    write(1, buffer, size);
    free(buffer);
}
void print_return_value(int ret, int err) {
    char *buffer = malloc(sizeof(char)*512);
    int size = sprintf(buffer, "Return value: %d\n", ret);

    if (ret < 0) {
        size += sprintf(buffer + size, "Error: %s\n", strerror(err));
    }
    write(1, buffer, size);
    free(buffer);
}

int test_rename() {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];
    args[0].name = "oldname";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    args[1].name = "newname";
    args[1].value = "output.txt";
    args[1].type = 's';

    print_header("rename", args, n_args);
	ret =  rename(args[0].value, args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_renameat() {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    int olddirfd = AT_FDCWD;
    args[0].name = "olddirfd";
    args[0].value = &olddirfd;
    args[0].type = 'd';

    args[1].name = "oldname";
    args[1].value = "output.txt";
    args[1].type = 's';

    int newdirfd = AT_FDCWD;
    args[2].name = "newdirfd";
    args[2].value = &newdirfd;
    args[2].type = 'd';

    args[3].name = "newname";
    args[3].value = "/tmp/inputs/inputA.txt";
    args[3].type = 's';

    print_header("renameat", args, n_args);
	ret = renameat(*(int*)args[0].value, (char*)args[1].value, *(int*)args[2].value, (char*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_renameat2() {
    int ret, err;
    int n_args = 5;

    ARG args[n_args];

    int olddirfd = AT_FDCWD;
    args[0].name = "olddirfd";
    args[0].value = &olddirfd;
    args[0].type = 'd';

    args[1].name = "oldname";
    args[1].value = "/tmp/inputs/inputB.txt";
    args[1].type = 's';

    int newdirfd = AT_FDCWD;
    args[2].name = "newdirfd";
    args[2].value = &newdirfd;
    args[2].type = 'd';

    args[3].name = "newname";
    args[3].value = "/tmp/inputs/outputB.txt";
    args[3].type = 's';

    unsigned int flags = RENAME_EXCHANGE|RENAME_WHITEOUT;
    args[4].name = "flags";
    args[4].value = &flags;
    args[4].type = 'u';

    print_header("renameat", args, n_args);
    ret = renameat2(*(int*)args[0].value, (char*)args[1].value, *(int*)args[2].value, (char*)args[3].value, *(unsigned int*)args[4].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_truncate() {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "path";
    args[0].value = "/tmp/inputs/inputB.txt";
    args[0].type = 's';

    long length = 512;
    args[1].name = "length";
    args[1].value = &length;
    args[1].type = 'l';

    print_header("truncate", args, n_args);
    ret = truncate((char*)args[0].value, *((long*)args[1].value));
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_ftruncate(int fd) {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    long length = 512;
    args[1].name = "length";
    args[1].value = &length;
    args[1].type = 'l';

    print_header("ftruncate", args, n_args);
    ret = ftruncate(*(int*)args[0].value, *((long*)args[1].value));
    err = errno;
    print_return_value(ret, err);

    return ret;
}


int test_unlink() {
    int ret, err;
    int n_args = 1;

    ARG args[n_args];

    args[0].name = "path";
    args[0].value = "/tmp/inputs/inputB.txt";
    args[0].type = 's';

    print_header("unlink", args, n_args);
    ret = unlink((char*)args[0].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_unlinkat() {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    int dirfd = AT_FDCWD;
    args[0].name = "dirfd";
    args[0].value = &dirfd;
    args[0].type = 'd';

    args[1].name = "path";
    args[1].value = "/tmp/inputs/inputB.txt";
    args[1].type = 's';

    int flags = 0;
    args[2].name = "flags";
    args[2].value = &flags;
    args[2].type = 'd';

    print_header("unlinkat", args, n_args);
    ret = unlinkat(*(int*)args[0].value, (char*)args[1].value, *(int*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_stat() {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "path";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    struct stat statbuf;
    args[1].name = "statbuf";
    args[1].value = &statbuf;
    args[1].type = '0';

    print_header("stat", args, n_args);
    ret = stat((char*)args[0].value, (struct stat *)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_lstat() {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "path";
    args[0].value = "/tmp/inputs/inputB.txt";
    args[0].type = 's';

    struct stat statbuf;
    args[1].name = "statbuf";
    args[1].value = &statbuf;
    args[1].type = '0';

    print_header("lstat", args, n_args);
    ret = lstat((char*)args[0].value, (struct stat *)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_fstat(int fd) {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    struct stat statbuf;
    args[1].name = "statbuf";
    args[1].value = &statbuf;
    args[1].type = '0';

    print_header("fstat", args, n_args);
    ret = fstat(*(int*)args[0].value, (struct stat *)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_fstatat() {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    int dirfd = AT_FDCWD;
    args[0].name = "dirfd";
    args[0].value = &dirfd;
    args[0].type = 'd';

    args[1].name = "path";
    args[1].value = "/tmp/inputs/inputB.txt";
    args[1].type = 's';

    struct stat statbuf;
    args[2].name = "statbuf";
    args[2].value = &statbuf;
    args[2].type = '0';

    int flags = AT_SYMLINK_NOFOLLOW|AT_EMPTY_PATH;
    args[3].name = "flags";
    args[3].value = &flags;
    args[3].type = 'd';

    print_header("fstatat", args, n_args);
    ret = fstatat(*(int*)args[0].value, (char*)args[1].value, (struct stat *)args[2].value, *(int*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_fstatfs(int fd) {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    struct statfs buf;
    args[1].name = "buf";
    args[1].value = &buf;
    args[1].type = '0';

    print_header("fstatfs", args, n_args);
    ret = fstatfs(*(int*)args[0].value, (struct statfs *)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_readlink() {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/readlink.symlink";
    args[0].type = 's';

    char buf[BUF_SIZE];
    args[1].name = "buf";
    args[1].value = &buf;
    args[1].type = 's';

    size_t size = BUF_SIZE;
    args[2].name = "bufsiz";
    args[2].value = &size;
    args[2].type = 'l';

    print_header("readlink", args, n_args);
    ret = readlink((char*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_readlinkat() {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    int dirfd = AT_FDCWD;
    args[0].name = "dirfd";
    args[0].value = &dirfd;
    args[0].type = 'd';

    args[1].name = "pathname";
    args[1].value = "/tmp/inputs/readlink.symlink";
    args[1].type = 's';

    char buf[BUF_SIZE];
    args[2].name = "buf";
    args[2].value = &buf;
    args[2].type = 's';

    size_t size = BUF_SIZE;
    args[3].name = "bufsiz";
    args[3].value = &size;
    args[3].type = 'l';

    print_header("readlinkat", args, n_args);
    ret = readlinkat(*(int*)args[0].value, (char*)args[1].value, (char*)args[2].value, *(size_t*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_listxattr() {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "path";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    char list[XATTR_SIZE];
    args[1].name = "list";
    args[1].value = &list;
    args[1].type = 's';

    size_t size = XATTR_SIZE;
    args[2].name = "size";
    args[2].value = &size;
    args[2].type = 'l';

    print_header("listxattr", args, n_args);
    ret = listxattr((char*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_llistxattr() {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "path";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    char list[XATTR_SIZE];
    args[1].name = "list";
    args[1].value = &list;
    args[1].type = 's';

    size_t size = XATTR_SIZE;
    args[2].name = "size";
    args[2].value = &size;
    args[2].type = 'l';

    print_header("llistxattr", args, n_args);
    ret = llistxattr((char*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_flistxattr(int fd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    char list[XATTR_SIZE];
    args[1].name = "list";
    args[1].value = &list;
    args[1].type = 's';

    size_t size = XATTR_SIZE;
    args[2].name = "size";
    args[2].value = &size;
    args[2].type = 'l';

    print_header("flistxattr", args, n_args);
    ret = flistxattr(*(int*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_setxattr() {
    int ret, err;
    int n_args = 5;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';

    args[2].name = "value";
    args[2].value = "x attribute value";
    args[2].type = 's';

    size_t size = strlen((char*)args[2].value);
    args[3].name = "size";
    args[3].value = &size;
    args[3].type = 'd';

    int flags = 0;
    args[4].name = "flags";
    args[4].value = &flags;
    args[4].type = 'd';

    print_header("setxattr", args, n_args);

    ret = setxattr((char*)args[0].value, (char*)args[1].value, (char*)args[2].value, *(size_t*)args[3].value, *(int*)args[4].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_lsetxattr() {
    int ret, err;
    int n_args = 5;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';

    args[2].name = "value";
    args[2].value = "Y attribute value";
    args[2].type = 's';

    size_t size = strlen((char*)args[2].value);
    args[3].name = "size";
    args[3].value = &size;
    args[3].type = 'd';

    int flags = XATTR_REPLACE;
    args[4].name = "flags";
    args[4].value = &flags;
    args[4].type = 'd';

    print_header("lsetxattr", args, n_args);

    ret = lsetxattr((char*)args[0].value, (char*)args[1].value, (char*)args[2].value, *(size_t*)args[3].value, *(int*)args[4].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_fsetxattr(int fd) {
    int ret, err;
    int n_args = 5;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';

    args[2].name = "value";
    args[2].value = "Y attribute value";
    args[2].type = 's';

    size_t size = strlen((char*)args[2].value);
    args[3].name = "size";
    args[3].value = &size;
    args[3].type = 'd';

    int flags = 0;
    args[4].name = "flags";
    args[4].value = &flags;
    args[4].type = 'd';

    print_header("fsetxattr", args, n_args);

    ret = fsetxattr(*(int*)args[0].value, (char*)args[1].value, (char*)args[2].value, *(size_t*)args[3].value, *(int*)args[4].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_getxattr() {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';

    char value[1024];
    args[2].name = "value";
    args[2].value = &value;
    args[2].type = 's';

    size_t size = 1024;
    args[3].name = "size";
    args[3].value = &size;
    args[3].type = 'd';

    print_header("getxattr", args, n_args);

    ret = getxattr((char*)args[0].value, (char*)args[1].value, (char*)args[2].value, *(size_t*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    printf("value is: %s\n", value);
    return ret;
}
int test_lgetxattr() {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';

    char value[1024];
    args[2].name = "value";
    args[2].value = &value;
    args[2].type = 's';

    size_t size = 1024;
    args[3].name = "size";
    args[3].value = &size;
    args[3].type = 'd';

    print_header("lgetxattr", args, n_args);

    ret = lgetxattr((char*)args[0].value, (char*)args[1].value, (char*)args[2].value, *(size_t*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    printf("value is: %s\n", value);
    return ret;
}
int test_fgetxattr(int fd) {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';

    char value[1024];
    args[2].name = "value";
    args[2].value = &value;
    args[2].type = 's';

    size_t size = 1024;
    args[3].name = "size";
    args[3].value = &size;
    args[3].type = 'd';

    print_header("fgetxattr", args, n_args);

    ret = fgetxattr(*(int*)args[0].value, (char*)args[1].value, (char*)args[2].value, *(size_t*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    printf("value is: %s\n", value);
    return ret;
}

int test_removexattr() {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';

    print_header("removexattr", args, n_args);

    ret = removexattr((char*)args[0].value, (char*)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_lremovexattr() {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';

    print_header("lremovexattr", args, n_args);

    ret = lremovexattr((char*)args[0].value, (char*)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_fremovexattr(int fd) {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    args[1].name = "name";
    args[1].value = "user.custom_attrib";
    args[1].type = 's';


    print_header("fremovexattr", args, n_args);

    ret = fremovexattr(*(int*)args[0].value, (char*)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_open() {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/inputA.txt";
    args[0].type = 's';

    int flags = O_RDONLY;
    args[1].name = "flags";
    args[1].value = &flags;
    args[1].type = 'd';

    print_header("open", args, n_args);

    ret = open((char*)args[0].value, *(int*)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_openat() {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    int dirfd = AT_FDCWD;
    args[0].name = "dirfd";
    args[0].value = &dirfd;
    args[0].type = 'd';

    args[1].name = "pathname";
    args[1].value = "/tmp/inputs/inputD.txt";
    args[1].type = 's';

    int flags = O_CREAT|O_RDWR|O_APPEND;
    args[2].name = "flags";
    args[2].value = &flags;
    args[2].type = 'd';

    mode_t mode = 0666;
    args[3].name = "mode";
    args[3].value = &mode;
    args[3].type = 'u';

    print_header("openat", args, n_args);

    ret = openat(*(int*)args[0].value, (char*)args[1].value, *(int*)args[2].value, *(mode_t*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_creat() {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/inputs/inputC.txt";
    args[0].type = 's';

    mode_t mode = 0630;
    args[1].name = "mode";
    args[1].value = &mode;
    args[1].type = 'u';

    print_header("creat", args, n_args);

    ret = creat((char*)args[0].value, *(mode_t*)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_readahead(int fd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    off64_t offset = 512;
    args[1].name = "offset";
    args[1].value = &offset;
    args[1].type = 'l';

    size_t count = 512;
    args[2].name = "count";
    args[2].value = &count;
    args[2].type = 'd';

    print_header("readahead", args, n_args);

    ret = readahead(*(int*)args[0].value, *(off64_t*)args[1].value, *(size_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_close(int fd) {
    int ret, err;
    int n_args = 1;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    print_header("close", args, n_args);

    ret = close(*(int*)args[0].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_fsync(int fd) {
    int ret, err;
    int n_args = 1;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    print_header("fsync", args, n_args);

    ret = fsync(*(int*)args[0].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_fdatasync(int fd) {
    int ret, err;
    int n_args = 1;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    print_header("fdatasync", args, n_args);

    ret = fdatasync(*(int*)args[0].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_read(int fd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    char buf[BUF_SIZE];
    args[1].name = "buf";
    args[1].value = &buf;
    args[1].type = 's';

    size_t count = BUF_SIZE;
    args[2].name = "count";
    args[2].value = &count;
    args[2].type = 'd';

    print_header("read", args, n_args);

    ret = read(*(int*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_pread(int fd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    char buf[512];
    args[1].name = "buf";
    args[1].value = &buf;
    args[1].type = 's';

    size_t count = 512;
    args[2].name = "count";
    args[2].value = &count;
    args[2].type = 'd';

    off_t offset = 1024;
    args[3].name = "offset";
    args[3].value = &offset;
    args[3].type = 'l';

    print_header("pread", args, n_args);

    ret = pread(*(int*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value, *(off_t*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_readv(int fd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    int iovcnt = 3;
    struct iovec iov[iovcnt];

    for (int i = 0; i < iovcnt; i++) {
        iov[i].iov_base = (char*) malloc(sizeof(char)*512);
        iov[i].iov_len = 512;
    }

    args[1].name = "iov";
    args[1].value = &iov;
    args[1].type = 'd';

    args[2].name = "iovcnt";
    args[2].value = &iovcnt;
    args[2].type = 'd';

    print_header("readv", args, n_args);

    ret = readv(*(int*)args[0].value, (struct iovec*)args[1].value, *(int*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    for (int i = 0; i < iovcnt; i++) printf ("%d: %s\n", i, (char *) iov[i].iov_base);

    for (int i = 0; i < iovcnt; i++) {
        free(iov[i].iov_base);
    }

    return ret;
}

int test_write(int fd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    args[1].name = "buf";
    args[1].value = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Phasellus tristique eros vitae enim consequat consequat. Quisque ex purus, pharetra ultrices ligula id, congue ultrices tortor. Praesent velit ligula, scelerisque pharetra vehicula et, tempor eget odio. Etiam maximus neque a justo elementum, ac accumsan tellus vehicula. Vivamus vitae orci eget velit malesuada varius sit amet vitae nibh. Nunc sed semper odio. Ut tempor auctor aliquet. Nulla facilisi. Nunc et felis nec arcu interdum elementum eget cras.";
    args[1].type = 's';

    size_t count = strlen((char*)args[1].value);
    args[2].name = "count";
    args[2].value = &count;
    args[2].type = 'd';

    print_header("write", args, n_args);

    ret = write(*(int*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_pwrite(int fd) {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    args[1].name = "buf";
    args[1].value = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Phasellus tristique eros vitae enim consequat consequat. Quisque ex purus, pharetra ultrices ligula id, congue ultrices tortor. Praesent velit ligula, scelerisque pharetra vehicula et, tempor eget odio. Etiam maximus neque a justo elementum, ac accumsan tellus vehicula. Vivamus vitae orci eget velit malesuada varius sit amet vitae nibh. Nunc sed semper odio. Ut tempor auctor aliquet. Nulla facilisi. Nunc et felis nec arcu interdum elementum eget cras.";
    args[1].type = 's';

    size_t count = strlen((char*)args[1].value);
    args[2].name = "count";
    args[2].value = &count;
    args[2].type = 'd';

    off_t offset = 1024;
    args[3].name = "offset";
    args[3].value = &offset;
    args[3].type = 'l';


    print_header("pwrite", args, n_args);

    ret = pwrite(*(int*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value, *(off_t*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_writev(int fd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "fd";
    args[0].value = &fd;
    args[0].type = 'd';

    int iovcnt = 3;
    struct iovec iov[iovcnt];

    char *buf[] = {
                "The term buccaneer comes from the word boucan.\n",
                "A boucan is a wooden frame used for cooking m.\n",
                "Buccaneer is the West Indies name for a pirat.\n" };
    for (int i = 0; i < iovcnt; i++) {
            iov[i].iov_base = buf[i];
            iov[i].iov_len = strlen(buf[i]) + 1;
    }

    args[1].name = "iov";
    args[1].value = &iov;
    args[1].type = 'd';

    args[2].name = "iovcnt";
    args[2].value = &iovcnt;
    args[2].type = 'd';

    print_header("writev", args, n_args);

    ret = writev(*(int*)args[0].value, (struct iovec*)args[1].value, *(int*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    for (int i = 0; i < iovcnt; i++) printf ("%d: %s\n", i, (char *) iov[i].iov_base);

    return ret;
}

int test_socket() {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    int domain = AF_INET;
    args[0].name = "domain";
    args[0].value = &domain;
    args[0].type = 'd';

    int type = SOCK_STREAM;
    args[1].name = "type";
    args[1].value = &type;
    args[1].type = 'd';

    int protocol = 0;
    args[2].name = "protocol";
    args[2].value = &protocol;
    args[2].type = 'd';

    print_header("socket", args, n_args);

    ret = socket(*(int*)args[0].value, *(int*)args[1].value, *(int*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}
int test_socketUnix() {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    int domain = AF_UNIX;
    args[0].name = "domain";
    args[0].value = &domain;
    args[0].type = 'd';

    int type = SOCK_SEQPACKET;
    args[1].name = "type";
    args[1].value = &type;
    args[1].type = 'd';

    int protocol = 0;
    args[2].name = "protocol";
    args[2].value = &protocol;
    args[2].type = 'd';

    print_header("socket", args, n_args);

    ret = socket(*(int*)args[0].value, *(int*)args[1].value, *(int*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_socketpair(int sv[2]) {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    int domain = PF_LOCAL;
    args[0].name = "domain";
    args[0].value = &domain;
    args[0].type = 'd';

    int type = SOCK_STREAM;
    args[1].name = "type";
    args[1].value = &type;
    args[1].type = 'd';

    int protocol = 0;
    args[2].name = "protocol";
    args[2].value = &protocol;
    args[2].type = 'd';

    args[3].name = "sv";
    args[3].value = &sv;
    args[3].type = 'p';

    print_header("socketpair", args, n_args);

    ret = socketpair(*(int*)args[0].value, *(int*)args[1].value, *(int*)args[2].value, sv);
    err = errno;
    print_return_value(ret, err);

    printf("sv[0]=%d, sv[1]=%d\n", sv[0], sv[1]);

    return ret;
}

int test_bind(int sockfd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SOCKET_PORT);
    args[1].name = "address";
    args[1].value = &address;
    args[1].type = 'p';

    int addrlen = sizeof(address);
    args[2].name = "addrlen";
    args[2].value = &addrlen;
    args[2].type = 'd';

    print_header("bind", args, n_args);

    ret = bind(*(int*)args[0].value, (struct sockaddr*)args[1].value, *(int*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_listen(int sockfd) {
    int ret, err;
    int n_args = 2;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    int backlog = 1;
    args[1].name = "backlog";
    args[1].value = &backlog;
    args[1].type = 'd';

    print_header("listen", args, n_args);

    ret = listen(*(int*)args[0].value, *(int*)args[1].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_accept(int sockfd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SOCKET_PORT);
    args[1].name = "address";
    args[1].value = &address;
    args[1].type = 'p';

    socklen_t addrlen = sizeof(address);
    args[2].name = "addrlen";
    args[2].value = &addrlen;
    args[2].type = 'u';

    print_header("accept", args, n_args);

    ret = accept(*(int*)args[0].value, (struct sockaddr*)args[1].value, (socklen_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_accept4(int sockfd) {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SOCKET_PORT);
    args[1].name = "address";
    args[1].value = &address;
    args[1].type = 'p';

    socklen_t addrlen = sizeof(address);
    args[2].name = "addrlen";
    args[2].value = &addrlen;
    args[2].type = 'u';

    int flags = SOCK_NONBLOCK;
    args[3].name = "flags";
    args[3].value = &flags;
    args[3].type = 'd';

    print_header("accept4", args, n_args);

    ret = accept4(*(int*)args[0].value, (struct sockaddr*)args[1].value, (socklen_t*)args[2].value, *(int*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_connect(int sockfd) {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    struct sockaddr_un address;
    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_NAME, sizeof(address.sun_path)-1);
    args[1].name = "address";
    args[1].value = &address;
    args[1].type = 'p';

    socklen_t addrlen = sizeof(address);
    args[2].name = "addrlen";
    args[2].value = &addrlen;
    args[2].type = 'u';

    print_header("connect", args, n_args);

    ret = connect(*(int*)args[0].value, (struct sockaddr*)args[1].value, *(socklen_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_setsockopt(int sockfd) {
    int ret, err;
    int n_args = 5;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    int level = SOL_SOCKET;
    args[1].name = "level";
    args[1].value = &level;
    args[1].type = 'd';

    int optname = SO_REUSEPORT;
    args[2].name = "optname";
    args[2].value = &optname;
    args[2].type = 'u';

    int optval = 1;
    args[3].name = "optval";
    args[3].value = &optval;
    args[3].type = 'u';

    socklen_t optlen = sizeof(int);
    args[4].name = "optlen";
    args[4].value = &optlen;
    args[4].type = 'u';


    print_header("setsockopt", args, n_args);

    ret = setsockopt(*(int*)args[0].value, *(int*)args[1].value, *(int*)args[2].value, (int*)args[3].value, *(socklen_t*)args[4].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_getsockopt(int sockfd) {
    int ret, err;
    int n_args = 5;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    int level = SOL_SOCKET;
    args[1].name = "level";
    args[1].value = &level;
    args[1].type = 'd';

    int optname = SO_REUSEPORT;
    args[2].name = "optname";
    args[2].value = &optname;
    args[2].type = 'd';

    int optval = 0;
    args[3].name = "optval";
    args[3].value = &optval;
    args[3].type = 'd';

    socklen_t optlen = sizeof(int);
    args[4].name = "optlen";
    args[4].value = &optlen;
    args[4].type = 'u';


    print_header("getsockopt", args, n_args);

    ret = getsockopt(*(int*)args[0].value, *(int*)args[1].value, *(int*)args[2].value, (int*)args[3].value, (socklen_t*)args[4].value);
    err = errno;
    print_return_value(ret, err);

    printf("Optval: %d, optlen: %d\n", optval, optlen);

    return ret;
}

int test_mknod() {
    int ret, err;
    int n_args = 3;

    ARG args[n_args];

    args[0].name = "pathname";
    args[0].value = "/tmp/abc-path.txt";
    args[0].type = 's';

    mode_t mode = S_IFREG | 0666;
    args[1].name = "mode";
    args[1].value = &mode;
    args[1].type = 'd';

    dev_t dev = 0;
    args[2].name = "dev";
    args[2].value = &dev;
    args[2].type = 'd';


    print_header("mknod", args, n_args);

    ret = mknod((char*)args[0].value, *(mode_t*)args[1].value, *(dev_t*)args[2].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_mknodat() {
    int ret, err;
    int n_args = 4;

    ARG args[n_args];

    int dirfd = AT_FDCWD;
    args[0].name = "dirfd";
    args[0].value = &dirfd;
    args[0].type = 'd';

    args[1].name = "pathname";
    args[1].value = "/tmp/def-path.txt";
    args[1].type = 's';

    mode_t mode = S_IFIFO|0444;
    args[2].name = "mode";
    args[2].value = &mode;
    args[2].type = 'd';

    dev_t dev = 0;
    args[3].name = "dev";
    args[3].value = &dev;
    args[3].type = 'd';


    print_header("mknodat", args, n_args);

    ret = mknodat(*(int*)args[0].value, (char*)args[1].value, *(mode_t*)args[2].value, *(dev_t*)args[3].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int test_recvfrom(int sockfd, struct sockaddr *src_addr, socklen_t *addrlen) {
    int ret, err;
    int n_args = 6;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    char buf[BUF_SIZE];
    args[1].name = "buf";
    args[1].value = &buf;
    args[1].type = 's';

    size_t len = strlen(buf);
    args[2].name = "len";
    args[2].value = &len;
    args[2].type = 'u';

    int flags = 0;
    args[3].name = "flags";
    args[3].value = &flags;
    args[3].type = 'd';

    args[4].name = "src_addr";
    args[4].value = src_addr;
    args[4].type = 'p';

    *addrlen = sizeof(struct sockaddr_in);
    printf("ADDRLEN: %d\n", *addrlen);
    args[5].name = "addrlen";
    args[5].value = addrlen;
    args[5].type = 'u';

    print_header("recvfrom", args, n_args);

    ret = recvfrom(*(int*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value, *(int*)args[3].value, (struct sockaddr*)args[4].value, (socklen_t*)args[5].value);
    err = errno;
    print_return_value(ret, err);

    printf("buf: %.*s\n", ret,buf);

    return ret;
}


int test_sendto(int sockfd, struct sockaddr *dest_addr, socklen_t addrlen) {
    int ret, err;
    int n_args = 6;

    ARG args[n_args];

    args[0].name = "sockfd";
    args[0].value = &sockfd;
    args[0].type = 'd';

    char *buf = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
    args[1].name = "buf";
    args[1].value = buf;
    args[1].type = 's';

    size_t len = strlen(buf);
    args[2].name = "len";
    args[2].value = &len;
    args[2].type = 'u';

    int flags = MSG_DONTWAIT;
    args[3].name = "flags";
    args[3].value = &flags;
    args[3].type = 'd';

    args[4].name = "dest_addr";
    args[4].value = dest_addr;
    args[4].type = 'p';

    args[5].name = "addrlen";
    args[5].value = &addrlen;
    args[5].type = 'u';

    print_header("sendto", args, n_args);

    ret = sendto(*(int*)args[0].value, (char*)args[1].value, *(size_t*)args[2].value, *(int*)args[3].value, (struct sockaddr*)args[4].value, *(socklen_t*)args[5].value);
    err = errno;
    print_return_value(ret, err);

    return ret;
}

int main () {

    int pid = getpid();
    printf("Pid: %d. Press Enter to start...\n", pid);
    getchar();

    test_rename();
    test_fstatat();
    test_truncate();
    test_unlink();
    test_stat();
    test_renameat();
    test_lstat();
    test_unlinkat();
    test_listxattr();
    test_renameat2();
    test_llistxattr();
    test_readlink();
    test_readlinkat();
    test_setxattr();
    test_getxattr();
    test_removexattr();
    test_lsetxattr();
    test_lgetxattr();
    test_lremovexattr();
    int fd = test_open();
    test_readahead(fd);
    test_read(fd);
    test_ftruncate(fd);
    test_fstat(fd);
    test_creat();
    test_fsetxattr(fd);
    test_fgetxattr(fd);
    test_fremovexattr(fd);
    int fd2 = test_openat();
    test_writev(fd2);
    int off = lseek(fd2, 0, SEEK_CUR);
    printf("offset is: %d\n", off);
    test_write(fd2);
    test_fdatasync(fd2);
    test_pwrite(fd2);
    test_fsync(fd);
    test_pread(fd2);
    test_readv(fd2);
    test_flistxattr(fd);
    test_close(fd);
    test_fstatfs(fd2);
    test_close(-1);
    test_close(112);

    int sv[2];
    test_socketpair(sv);
    close(sv[0]);

    int sockfd = test_socket();
    test_bind(sockfd);
    test_setsockopt(sockfd);
    test_listen(sockfd);
    int newsockfd = test_accept(sockfd);
    test_getsockopt(sockfd);

    struct sockaddr from = {0};
    socklen_t addrlen = 0;
    test_recvfrom(newsockfd, &from, &addrlen);
    from.sa_family = AF_INET;
    addrlen = 3;
    test_sendto(newsockfd, &from, addrlen);
    // test_accept4(sockfd);

    close(newsockfd);
    close(sockfd);

    sockfd = test_socketUnix();
    test_connect(sockfd);

    test_mknod();
    test_mknodat();

	return 0;
}