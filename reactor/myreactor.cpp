#include "myreactor.h"
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  //for htonl() and htons()
#include <fcntl.h>
#include <sys/epoll.h>
#include <list>
#include <errno.h>
#include <time.h>
#include <sstream>
#include <iomanip> //for std::setw()/setfill()
#include <unistd.h>

#define min(a, b) ((a <= b) ? (a) : (b))

CMyReactor::CMyReactor()
{
    //m_listenfd = 0;
    //m_epollfd = 0;
    //m_bStop = false;
}

CMyReactor::~CMyReactor()
{

}

bool CMyReactor::init(const char* ip, short nport)
{
    //创建监听socket，并将监听socket挂载到 epoll 上
    if (!create_server_listener(ip, nport))
    {
        std::cout << "Unable to bind: " << ip << ":" << nport << "." << std::endl;
        return false;
    }

    //打印当前线程id
    std::cout << "main thread id = " << std::this_thread::get_id() << std::endl;

    //启动接收新连接的线程
    m_acceptthread.reset(new std::thread(CMyReactor::accept_thread_proc, this));

    //启动工作线程（收发数据）
    for (auto& t : m_workerthreads)
    {
        t.reset(new std::thread(CMyReactor::worker_thread_proc, this));
    }


    return true;
}
//释放资源
bool CMyReactor::uninit()
{
    m_bStop = true;
    m_acceptcond.notify_one();
    m_workercond.notify_all();

    m_acceptthread->join();
    for (auto& t : m_workerthreads)
    {
        t->join();
    }

    ::epoll_ctl(m_epollfd, EPOLL_CTL_DEL, m_listenfd, NULL);

    //TODO: 是否需要先调用shutdown()一下？
    ::shutdown(m_listenfd, SHUT_RDWR);
    ::close(m_listenfd);
    ::close(m_epollfd);

    return true;
}

bool CMyReactor::close_client(int clientfd)
{
    if (::epoll_ctl(m_epollfd, EPOLL_CTL_DEL, clientfd, NULL) == -1)
    {
        std::cout << "close client socket failed as call epoll_ctl failed" << std::endl;
        //return false;
    }


    ::close(clientfd);

    return true;
}
//主 loop
void* CMyReactor::main_loop(void* p)
{
    std::cout << "main thread id = " << std::this_thread::get_id() << std::endl;

    CMyReactor* pReatcor = static_cast<CMyReactor*>(p);

    //在一个 while 循环中 不断根据 epoll_wait的返回值去处理相应的事件
    while (!pReatcor->m_bStop)
    {
        struct epoll_event ev[1024];

        int n = ::epoll_wait(pReatcor->m_epollfd, ev, 1024, 10);

        if (n == 0)
            continue;
        else if (n < 0)
        {


        }

        int m = min(n, 1024);
        for (int i = 0; i < m; ++i)
        {
            //判断返回的如果是接受链接事件，则通知接收连接线程接收新连接
            if (ev[i].data.fd == pReatcor->m_listenfd)
                pReatcor->m_acceptcond.notify_one();
            //如果是收发数据事件，则通知普通工作线程接收数据
            else
            {
                //使用大括号将锁的粒度变细，将临界区变小
                {
                    //m_listClients队列是共享资源，需要加锁
                    std::unique_lock<std::mutex> guard(pReatcor->m_workermutex);

                    pReatcor->m_listClients.push_back(ev[i].data.fd);
                }

                //通知消费者 m_workercond 消费
                pReatcor->m_workercond.notify_one();
                //std::cout << "signal" << std::endl;
            }// end if

        }// end for-loop
    }// end while

    std::cout << "main loop exit ..." << std::endl;

    return NULL;
}
void CMyReactor::accept_thread_proc(CMyReactor* pReatcor)
{
    std::cout << "accept thread, thread id = " << std::this_thread::get_id() << std::endl;

    //接受链接线程在一个死循环中不断接受客户端的连接
    while (true)
    {
        int newfd;
        struct sockaddr_in clientaddr;
        socklen_t addrlen;
        {
            std::unique_lock<std::mutex> guard(pReatcor->m_acceptmutex);

            //如果没有连接，进行将阻塞在这里等待。当m_acceptcond被唤醒时，
            //说明有新连接到来，那么调用 accept 接受连接
            pReatcor->m_acceptcond.wait(guard);
            if (pReatcor->m_bStop)
                break;

            //std::cout << "run loop in accept_thread_proc" << std::endl;

            newfd = ::accept(pReatcor->m_listenfd, (struct sockaddr *)&clientaddr, &addrlen);
        }
        if (newfd == -1)
            continue;

        std::cout << "new client connected: " << ::inet_ntoa(clientaddr.sin_addr) << ":" << ::ntohs(clientaddr.sin_port) << std::endl;

        //将新socket设置为non-blocking
        int oldflag = ::fcntl(newfd, F_GETFL, 0);
        int newflag = oldflag | O_NONBLOCK;
        if (::fcntl(newfd, F_SETFL, newflag) == -1)
        {
            std::cout << "fcntl error, oldflag =" << oldflag << ", newflag = " << newflag << std::endl;
            continue;
        }

        struct epoll_event e;
        memset(&e, 0, sizeof(e));
        e.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
        e.data.fd = newfd;

        //将 accept 的连接fd ，继续加入 epoll 中监听他的 读写事件
        if (::epoll_ctl(pReatcor->m_epollfd, EPOLL_CTL_ADD, newfd, &e) == -1)
        {
            std::cout << "epoll_ctl error, fd =" << newfd << std::endl;
        }
    }

    std::cout << "accept thread exit ..." << std::endl;
}
void CMyReactor::worker_thread_proc(CMyReactor* pReatcor)
{
    std::cout << "new worker thread, thread id = " << std::this_thread::get_id() << std::endl;

    while (true)
    {
        int clientfd;
        {
            std::unique_lock<std::mutex> guard(pReatcor->m_workermutex);

            //注意此处应使用while， 避免虚假唤醒
            while (pReatcor->m_listClients.empty())
            {
                if (pReatcor->m_bStop)
                {
                    std::cout << "worker thread exit ..." << std::endl;
                    return;
                }

                pReatcor->m_workercond.wait(guard);
            }

            clientfd = pReatcor->m_listClients.front();
            pReatcor->m_listClients.pop_front();
        }

        //gdb调试时不能实时刷新标准输出，用这个函数刷新标准输出，使信息在屏幕上实时显示出来
        std::cout << std::endl;

        std::string strclientmsg;
        char buff[256];
        bool bError = false;
        while (true)
        {
            memset(buff, 0, sizeof(buff));
            int nRecv = ::recv(clientfd, buff, 256, 0);
            if (nRecv == -1)
            {
                if (errno == EWOULDBLOCK)
                    break;
                else
                {
                    std::cout << "recv error, client disconnected, fd = " << clientfd << std::endl;
                    pReatcor->close_client(clientfd);
                    bError = true;
                    break;
                }

            }
            //对端关闭了socket，这端也关闭。
            else if (nRecv == 0)
            {
                std::cout << "peer closed, client disconnected, fd = " << clientfd << std::endl;
                pReatcor->close_client(clientfd);
                bError = true;
                break;
            }

            strclientmsg += buff;
        }

        //出错了，就不要再继续往下执行了
        if (bError)
            continue;

        std::cout << "client msg: " << strclientmsg;

        //将消息加上时间标签后发回
        time_t now = time(NULL);
        struct tm* nowstr = localtime(&now);
        std::ostringstream ostimestr;
        ostimestr << "[" << nowstr->tm_year + 1900 << "-"
            << std::setw(2) << std::setfill('0') << nowstr->tm_mon + 1 << "-"
            << std::setw(2) << std::setfill('0') << nowstr->tm_mday << " "
            << std::setw(2) << std::setfill('0') << nowstr->tm_hour << ":"
            << std::setw(2) << std::setfill('0') << nowstr->tm_min << ":"
            << std::setw(2) << std::setfill('0') << nowstr->tm_sec << "]server reply: ";

        strclientmsg.insert(0, ostimestr.str());
        //对于数据的发送：LT不注册可写事件，有数据直接发送，调用send和write的时候可能会阻塞，但是没有关系
        //睡一会继续发，一直尝试，到数据发送出去
        while (true)
        {
            int nSent = ::send(clientfd, strclientmsg.c_str(), strclientmsg.length(), 0);
            if (nSent == -1)
            {
                if (errno == EWOULDBLOCK)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                else
                {
                    std::cout << "send error, fd = " << clientfd << std::endl;
                    pReatcor->close_client(clientfd);
                    break;
                }

            }

            std::cout << "send: " << strclientmsg;
            strclientmsg.erase(0, nSent);

            if (strclientmsg.empty())
                break;
        }
    }
}
//根据 ip、port 创建监听socket，和 epollfd， 并将监听socket 挂载到 epollfd 上
bool CMyReactor::create_server_listener(const char* ip, short port)
{
    m_listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (m_listenfd == -1)
        return false;

    int on = 1;
    //设置监听 socket 的地址和端口的可重用
    ::setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    ::setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on));

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);

    //绑定
    if (::bind(m_listenfd, (sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        return false;

    //监听
    if (::listen(m_listenfd, 50) == -1)
        return false;

    //创建 epollfd
    m_epollfd = ::epoll_create(1);
    if (m_epollfd == -1)
        return false;

    struct epoll_event e;
    memset(&e, 0, sizeof(e));
    e.events = EPOLLIN | EPOLLRDHUP;
    e.data.fd = m_listenfd;

    //将m_listenfd（监听socketfd）挂载到 epollfd 上面，让epoll_wait 进行监听
    if (::epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_listenfd, &e) == -1)
        return false;

    return true;
}