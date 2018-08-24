find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(mpi_daemon mpi_daemon.cc)
target_link_libraries(mpi_daemon ${MPI_CXX_LIBRARIES} Threads::Threads)

