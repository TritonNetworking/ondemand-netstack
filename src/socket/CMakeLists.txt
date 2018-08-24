add_library(dccssocket SHARED dccs_socket.cc)
set_property(TARGET dccssocket PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dccssocket ${CMAKE_DL_LIBS})

