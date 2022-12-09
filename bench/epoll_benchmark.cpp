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
#include <ctime>
#include <thread>
#include <iostream>
#define MAX_OPEN_FD 1000
#define CURRENCY 2
#define SERV_PORT 18000
#define MAXLEN 1024
#define TOTAL_REQ 1e6
static int efd, total_req, total_read;
static struct sockaddr_in servaddr;
struct epoll_event tep;
void setnonblock(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}
void new_conn()
{
    if (--total_req < 0)
        return;
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(socketfd != -1);
    setnonblock(socketfd);
    tep.events = EPOLLOUT | EPOLLIN;
    tep.data.fd = socketfd;
    int ret = epoll_ctl(efd, EPOLL_CTL_ADD, socketfd, &tep);
    assert(ret != -1);
    ret = connect(socketfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    // fprintf(stderr,"Connect Error:%s\a\n",strerror(errno));
    // assert(ret != -1);
}

int main(int argc, char *argv[])
{
    total_req = total_read = TOTAL_REQ;
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
    clock_t start = clock();
    time_t start_time, end_time;
    time(&start_time);
    while (1)
    {
        int nready = epoll_wait(efd, ep, MAX_OPEN_FD, -1);
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
                // printf("write %s\n", buf);
                write(connfd, buf, sizeof(buf));// 写缓冲区满，会阻塞
            }
            else if (ep[i].events & EPOLLIN)
            {
                int bytes = read(connfd, buf, MAXLEN);
                buf[bytes] = 0;
                // printf("return %s\n", buf);
                close(connfd);
                new_conn();
                // fprintf(stderr, "total_read %d\n", total_read);
                if (--total_read <= 0)
                {
                    break;
                }
            }
        }
        if (--total_read <= 0)
        {
            break;
        }
    }
    clock_t end = clock();
    /**
     * 10000  4.68s
     * 100000 46.68s 单线程
     * 100000 49.02s reactor模式1:2
     * 100000 46.66s reactor模式1:4 cpu60%
     * 100000 46.98s reactor模式1:6
    */
   	time(&end_time);
	double cost_time = difftime(end_time, start_time);
	std::cout << "压测花费了 " << cost_time << " seconds" << std::endl;
    return 0;
}