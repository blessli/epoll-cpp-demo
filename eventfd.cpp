#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>

int main() {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    eventfd_write(efd, 2);
    eventfd_t count;
    eventfd_read(efd, &count);
    std::cout << "count=" << count << std::endl;
    close(efd);
}