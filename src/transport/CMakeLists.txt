add_library(socketwrapper SHARED socket_wrapper.c)
set_property(TARGET socketwrapper PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(socketwrapper ${CMAKE_DL_LIBS})

add_library(dccssocket SHARED dccs_socket.cc)
set_property(TARGET dccssocket PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dccssocket ${CMAKE_DL_LIBS})

find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(mpi_daemon mpi_daemon.cc)
target_link_libraries(mpi_daemon ${MPI_CXX_LIBRARIES} Threads::Threads)

