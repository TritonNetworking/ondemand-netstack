cmake_minimum_required(VERSION 3.5)
project(BULK_MPI CXX)

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )
LINK_DIRECTORIES( ${Boost_LIBRARY_DIR} )

file(GLOB sha256_SRC
    "*.h"
    "*.cpp"
)

ADD_LIBRARY(sha256 ${sha256_SRC})
target_link_libraries(sha256 LINK_PUBLIC)
