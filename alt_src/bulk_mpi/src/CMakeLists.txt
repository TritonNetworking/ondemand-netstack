cmake_minimum_required(VERSION 3.5)
PROJECT(BULK_MPI CXX)

find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

FILE(GLOB bulk_mpi_SRC
    "*.h"
    "*.cpp"
)

ADD_EXECUTABLE(bulk_mpi ${bulk_mpi_SRC})
TARGET_LINK_LIBRARIES(bulk_mpi yaml-cpp sha256)
