cmake_minimum_required(VERSION 3.12)
project(MyProject)

set(CMAKE_CXX_STANDARD 11)

add_executable(select src/select.cpp)
target_link_libraries(select PRIVATE pthread)

add_executable(poll src/poll.cpp)
target_link_libraries(poll PRIVATE pthread)

add_executable(epoll src/epoll.cpp)
target_link_libraries(epoll PRIVATE pthread)

add_executable(myTest test/gtest.cpp)
target_link_libraries(myTest PRIVATE gtest pthread)