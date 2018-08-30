cmake_minimum_required(VERSION 3.5)
project(FARCE CXX)

FIND_PACKAGE( Boost COMPONENTS program_options REQUIRED )

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )
LINK_DIRECTORIES( ${Boost_LIBRARY_DIR} )

file(GLOB memshare_SRC
    "*.h"
    "*.cpp"
)

ADD_LIBRARY(memshare ${memshare_SRC})
target_link_libraries(memshare LINK_PUBLIC ${Boost_LIBRARIES})
