add_library(libsocketwrapper SHARED socket_wrapper.c)
set_property(TARGET libsocketwrapper PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(libsocketwrapper ${CMAKE_DL_LIBS})

