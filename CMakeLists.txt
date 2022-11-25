set(SRCS
    epoll_server.cpp
    threadpool.cpp
    condition.cpp
)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
include_directories(include)
add_executable(EpollDemo ${SRCS})
target_link_libraries(EpollDemo pthread)


add_subdirectory(bench)
add_subdirectory(reactor)