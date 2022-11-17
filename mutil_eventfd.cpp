#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>

int main1() {
    int efd = eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    eventfd_write(efd, 2);
    eventfd_write(efd, 3);
    eventfd_write(efd, 4);
    eventfd_t count;
    int read_result = eventfd_read(efd, &count);
    std::cout << "read_result=" << read_result << std::endl;
    std::cout << "count=" << count << std::endl;
    read_result = eventfd_read(efd, &count);
    std::cout << "read_result=" << read_result << std::endl;
    std::cout << "count=" << count << std::endl;
    close(efd);
}

int main() {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC | EFD_SEMAPHORE);
    eventfd_write(efd, 2);
    eventfd_t count;
    int read_result = eventfd_read(efd, &count);
    std::cout << "read_result=" << read_result << std::endl;
    std::cout << "count=" << count << std::endl;
    read_result = eventfd_read(efd, &count);
    std::cout << "read_result=" << read_result << std::endl;
    std::cout << "count=" << count << std::endl;
    read_result = eventfd_read(efd, &count);
    std::cout << "read_result=" << read_result << std::endl;
    std::cout << "count=" << count << std::endl;
    close(efd);
}