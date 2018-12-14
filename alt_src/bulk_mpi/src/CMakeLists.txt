cmake_minimum_required(VERSION 3.5)
PROJECT(BULK_MPI CXX)

find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

set(HEADERS
    bulk_app.cpp
    control_host.cpp
    dummy_host.cpp
    endhost.cpp
    fake_data.cpp
    fake_data.h
    globals.h
    pkt_crafter.h
    run_funcs.h
    stats.h
    transport.h
    utils.h
)

set(BULK_MPI_SRC
    mpi_transport.h
    bulk_mpi.h
)

set(BULK_RDMA_SRC
    rdma_transport.h
    bulk_rdma.h
)

ADD_EXECUTABLE(bulk_mpi ${HEADERS} ${BULK_MPI_SRC})
TARGET_LINK_LIBRARIES(bulk_mpi yaml-cpp sha256 ${MPI_CXX_LIBRARIES})

ADD_EXECUTABLE(bulk_rdma ${HEADERS} ${BULK_RDMA_SRC})
TARGET_LINK_LIBRARIES(bulk_rdma yaml-cpp sha256 ${MPI_CXX_LIBRARIES})
