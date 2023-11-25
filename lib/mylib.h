#ifndef MYLIB_H
#define MYLIB_H

#include <sys/epoll.h>
#include <sys/timerfd.h>

int Socket(int domain, int type, int protocol);
int Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int Epoll_create(int size);
int Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int Epoll_wait(int epfd, struct epoll_event *events, 
               int maxevents, int timeout);

ssize_t Rio_readn(int fd, void *usrbuf, size_t n);
ssize_t Rio_writen(int fd, void *usrbuf, size_t n);

int Timerfd_create(int clockid, int flags);
int Timerfd_settime(int fd, int flags,
                    const struct itimerspec *new_value,
                    struct itimerspec *old_value);

int Open(const char *pathname, int flags);
int Close(int fd);

#endif // MYLIB_H