cmake_minimum_required(VERSION 3.5)
project(FARCE CXX)

ADD_EXECUTABLE(memshare_fork memshare_fork.cpp)

TARGET_LINK_LIBRARIES(memshare_fork memshare)
