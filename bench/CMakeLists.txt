set(SRCS
    epoll_benchmark.cpp
)

add_executable(bench ${SRCS})
target_link_libraries(bench pthread)