#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <ctype.h>
#define MAXLEN 1024
#define SERV_PORT 18000
#define MAX_OPEN_FD 1024

int main2(int argc, char *argv[])
{
    int listenfd, connfd, efd, ret;
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
    while(1)
    {
        // 返回已就绪的epoll_event,-1表示阻塞,没有就绪的epoll_event,将一直等待
        size_t nready = epoll_wait(efd, ep, MAX_OPEN_FD, -1);
        assert(nready != -1);
        for (int i = 0; i < nready; ++i)
        {
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
            // 否则,读取数据
            else
            {
                connfd = ep[i].data.fd;
                int bytes = read(connfd, buf, MAXLEN);
                // 客户端关闭连接
                if (bytes == 0)
                {
                    ret = epoll_ctl(efd, EPOLL_CTL_DEL, connfd, NULL);
                    assert(ret != -1);
                    close(connfd);
                    printf("客户端关闭连接，client[%d] closed\n", i);
                }
                else
                {
                    for (int j = 0; j < bytes; ++j)
                    {
                        buf[j] = toupper(buf[j]);
                    }
                    buf[bytes]='\0';
                    // 向客户端发送数据
                    write(connfd, buf, bytes);
                    printf("转成大写字母，并向客户端发送数据: %s\n",buf);
                }
            }
        }
    }
    return 0;
}