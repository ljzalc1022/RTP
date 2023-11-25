#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>

#include "mylib.h"
#include "rio.h"

int Socket(int domain, int type, int protocol)
{
    int re = socket(domain, type, protocol);
    if (re == -1) perror("socket");
    return re;
}

int Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int re = bind(sockfd, addr, addrlen);
    if (re == -1) perror("bind");
    return re;
}

int Epoll_create(int size)
{
    int re = epoll_create(size);
    if (re == -1) perror("epoll_create");
    return re;
}
int Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
    int re = epoll_ctl(epfd, op, fd, event);
    if (re == -1) perror("epoll_ctl");
    return re;
}
int Epoll_wait(int epfd, struct epoll_event *events, 
               int maxevents, int timeout)
{
    int re = epoll_wait(epfd, events, maxevents, timeout);
    if (re == -1) perror("epoll_wait");
    return re;
}

ssize_t Rio_readn(int fd, void *usrbuf, size_t n)
{
    int re = rio_readn(fd, usrbuf, n);
    if (re == -1) perror("rio_readn");
    return re;
}
ssize_t Rio_writen(int fd, void *usrbuf, size_t n)
{
    int re = rio_writen(fd, usrbuf, n);
    if (re == -1) perror("rio_writen");
    return re;
}

int Timerfd_create(int clockid, int flags)
{
    int re = timerfd_create(clockid, flags);
    if (re == -1) perror("timerfd_create");
    return re;
}
int Timerfd_settime(int fd, int flags,
                    const struct itimerspec *new_value,
                    struct itimerspec *old_value)
{
    int re = timerfd_settime(fd, flags, new_value, old_value);
    if (re == -1) perror("timerfd_settime");
    return re;
}

int Open(const char *pathname, int flags)
{
    int re = open(pathname, flags);
    if (re == -1) perror("open");
    return re;
}
int Close(int fd)
{
    int re = close(fd);
    if (re == -1) perror("close");
    return re;
}