#include <stdio.h>
#include <assert.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <string>
#define MAX_OPEN_FD 1000
#define CURRENCY 2
#define SERV_PORT 18000
#define MAXLEN 1024
static int efd;
static struct sockaddr_in servaddr;
struct epoll_event tep;
int setnonblock(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}
void new_conn()
{
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(socketfd != -1);
    setnonblock(socketfd);
    tep.events = EPOLLOUT|EPOLLIN;
    tep.data.fd = socketfd;
    int ret = epoll_ctl(efd, EPOLL_CTL_ADD, socketfd, &tep);
    assert(ret != -1);
    ret = connect(socketfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    fprintf(stderr,"Connect Error:%s\a\n",strerror(errno));
    // assert(ret != -1);
}

int main(int argc, char *argv[])
{
    char buf[MAXLEN];
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    struct epoll_event ep[MAX_OPEN_FD];
    // 创建一个epoll fd
    efd = epoll_create(MAX_OPEN_FD);
    assert(efd != -1);
    for (int i = 0; i < CURRENCY; i++)
    {
        new_conn();
    }
    while (1)
    {
        size_t nready = epoll_wait(efd, ep, MAX_OPEN_FD, -1);
        assert(nready != -1);
        for (int i = 0; i < nready; i++)
        {
            int connfd = ep[i].data.fd;
            if (ep[i].events & EPOLLOUT)
            {
                tep.events = EPOLLIN;
                tep.data.fd = connfd;
                int ret = epoll_ctl(efd, EPOLL_CTL_MOD, connfd, &tep);
                assert(ret != -1);
                boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
                std::string uuid_string = boost::uuids::to_string(a_uuid);
                strcpy(buf, uuid_string.c_str());
                printf("write %s\n", buf);
                write(connfd, buf, sizeof(uuid_string));
            }
            else if (ep[i].events & EPOLLIN)
            {
                printf("can read\n");
                int bytes = read(connfd, buf, MAXLEN);
                printf("return %s\n", buf);
                close(connfd);
                new_conn();
            }
        }
    }
}