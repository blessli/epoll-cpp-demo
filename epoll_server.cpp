#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include "threadpool.h"
#define MAXLEN 1024
#define SERV_PORT 18000
#define MAX_OPEN_FD 1024
#define THREADPOOL_MAX_NUM 1
static int efd;
struct fds
{
    int epollfd;
    int socketfd;
};

void *worker(void *args)
{
    int socketfd = *(int *)args;
    int epollfd = efd;
    printf("start new thread to receive data on fd: %d\n", socketfd);
    char buf[MAXLEN];
    int bytes = read(socketfd, buf, MAXLEN);
    if (bytes == 0)
    {
        int ret = epoll_ctl(efd, EPOLL_CTL_DEL, socketfd, NULL);
        assert(ret != -1);
        close(socketfd);
        printf("客户端关闭连接，client closed\n");
    }
    else
    {
        for (int j = 0; j < bytes; ++j)
        {
            buf[j] = toupper(buf[j]);
        }
        buf[bytes] = '\0';
        // 向客户端发送数据
        write(socketfd, buf, bytes);
        printf("转成大写字母，并向客户端发送数据: %s\n", buf);
    }
}

int main(int argc, char *argv[])
{
    threadpool_t pool;
    // 初始化线程池，最多3个线程
    threadpool_init(&pool, THREADPOOL_MAX_NUM);
    int listenfd, connfd, ret;
    char buf[MAXLEN];
    struct sockaddr_in cliaddr, servaddr;
    socklen_t clilen = sizeof(cliaddr);
    struct epoll_event tep, ep[MAX_OPEN_FD];
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    assert(!ret);
    ret = listen(listenfd, 20);
    assert(!ret);
    // 创建一个epoll fd
    efd = epoll_create(MAX_OPEN_FD);
    assert(efd != -1);
    tep.events = EPOLLIN;
    tep.data.fd = listenfd;
    // 把监听socket 先添加到efd中
    ret = epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &tep);
    while (1)
    {
        // 返回已就绪的epoll_event,-1表示阻塞,没有就绪的epoll_event,将一直等待
        size_t nready = epoll_wait(efd, ep, MAX_OPEN_FD, -1);
        assert(nready != -1);
        for (int i = 0; i < nready; ++i)
        {
            int socketfd = ep[i].data.fd;
            // 如果是新的连接,需要把新的socket添加到efd中
            if (ep[i].data.fd == listenfd)
            {
                connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
                assert(connfd != -1);
                tep.events = EPOLLIN;
                tep.data.fd = connfd;
                ret = epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &tep);
                assert(ret != -1);
                printf("如果是新的连接,需要把新的socket添加到efd中\n");
            }
            else if (ep[i].events & EPOLLIN) // 客户端有数据发送的事件发生
            {
                int *p = (int *)malloc(sizeof(int));
                *p = socketfd;
                threadpool_add_task(&pool, worker, p);
            }
            else
            {
                printf("error occurred~~~\n");
            }
        }
    }
    threadpool_destroy(&pool);
    return 0;
}