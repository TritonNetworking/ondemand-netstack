// glibc system call wrapper
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

// Function pointers to the original glibc functions.
static ssize_t (*real_send)(int sockfd, const void *buf, size_t len, int flags) = NULL;
static ssize_t (*real_recv)(int sockfd, void *buf, size_t len, int flags) = NULL;

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    printf("send: sockfd = %d, buf = %p, len = %zu, flags = %d.\n",
        sockfd, buf, len, flags);
    if (real_send == NULL) {
        *(void **)(&real_send) = dlsym(RTLD_NEXT, "send");
    }

    return real_send(sockfd, buf, len, flags);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    printf("recv: sockfd = %d, buf = %p, len = %zu, flags = %d.\n",
        sockfd, buf, len, flags);
    if (real_recv == NULL) {
        *(void **)(&real_recv) = dlsym(RTLD_NEXT, "recv");
    }

    return real_recv(sockfd, buf, len, flags);
}

