set(SRCS
    reactor_main.cpp
    myreactor.cpp
)

add_executable(reactor ${SRCS})
target_link_libraries(reactor pthread)