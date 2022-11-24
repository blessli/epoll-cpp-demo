man手册查看：man epoll<br>
epoll简单测试：
```sh
yum -y install nmap-ncat
nc 127.0.0.1 8000
/usr/bin/g++ -fdiagnostics-color=always -g /home/github/epoll-demo/*.cpp -o /home/github/epoll-demo/epoll_server -l pthread
/usr/bin/g++ -fdiagnostics-color=always -g /home/github/epoll-demo/bench/*.cpp -o /home/github/epoll-demo/bench/epoll_benchmark -l pthread
```