add_executable(simple_client simple_client.c)
add_executable(simple_server simple_server.c)

add_library(socketwrapper SHARED socket_wrapper.c)
set_property(TARGET socketwrapper PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(socketwrapper ${CMAKE_DL_LIBS})

