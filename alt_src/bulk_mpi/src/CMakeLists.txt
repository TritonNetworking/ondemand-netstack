cmake_minimum_required(VERSION 3.5)
PROJECT(BULK_MPI CXX)

SET(CMAKE_C_COMPILER /usr/mpi/gcc/openmpi-3.1.1rc1/bin/mpicc)
SET(CMAKE_CXX_COMPILER /usr/mpi/gcc/openmpi-3.1.1rc1/bin/mpicxx)

FILE(GLOB bulk_mpi_SRC
    "*.h"
    "*.cpp"
)

ADD_EXECUTABLE(bulk_mpi ${bulk_mpi_SRC})
TARGET_LINK_LIBRARIES(bulk_mpi yaml-cpp)
