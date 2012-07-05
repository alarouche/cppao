
# Only needed for Mandelbrot
Find_Package(GLUT)
Find_Package(OpenGL)

link_libraries( cppao )

add_executable( hello_active hello_active.cpp )
add_executable( ping_pong ping_pong.cpp )
add_executable( round_robin round_robin.cpp )
add_executable( forward_result forward_result.cpp )
add_executable( forward_result_iface forward_result_iface.cpp )
add_executable( forward_result_sink forward_result_sink.cpp )
add_executable( future future.cpp )
add_executable( life life.cpp life.hpp )
add_executable( bench bench.cpp )
add_executable( exception exception.cpp )
add_executable( object_types object_types.cpp )
add_executable( sieve sieve.cpp )
add_executable( sieve2 sieve2.cpp )
add_executable( queue_control queue_control.cpp )
add_library( active_socket active_socket.cpp active_socket.hpp )

IF( OPENGL_FOUND AND GLUT_FOUND )
	add_executable( mandelbrot mandelbrot.cpp mandelbrot.hpp )
        target_link_libraries( mandelbrot ${GL_LIBRARIES} ${GLUT_LIBRARIES} )
ELSE()
	message( "Skipping Mandelbrot sample because you don't appear to have OpenGL and GLUT installed" )
ENDIF()

link_libraries( active_socket )
add_executable( echo_server echo_server.cpp )
add_executable( echo_client echo_client.cpp )
add_executable( echo echo.cpp )

