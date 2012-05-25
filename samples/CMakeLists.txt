link_libraries( active_object )
include_directories( ../lib )

add_executable( hello_active hello_active.cpp )
add_executable( ping_pong ping_pong.cpp )
add_executable( round_robin round_robin.cpp )
add_executable( return_result return_result.cpp )
add_executable( forward_result forward_result.cpp )
add_executable( forward_result_iface forward_result_iface.cpp )
add_executable( forward_result_sink forward_result_sink.cpp )
add_executable( future future.cpp )
add_executable( life life.cpp life.hpp )
add_executable( bench bench.cpp )
add_library( active_socket active_socket.cpp active_socket.hpp )

link_libraries( active_socket )
add_executable( echo_server echo_server.cpp )
add_executable( echo_client echo_client.cpp )
add_executable( echo echo.cpp )

# Just comment these out if they don't build
# add_executable( shootout shootout.cpp )
# target_link_libraries( shootout boost_system )
