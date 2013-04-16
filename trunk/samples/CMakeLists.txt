
# Only needed for Mandelbrot
Find_Package(GLUT)
Find_Package(OpenGL)

link_libraries( cppao ${EXTRA_LIBS} )

add_executable( hello_active hello_active.cpp )
add_test( hello_active hello_active )

add_executable( ping_pong ping_pong.cpp )
add_test( ping_pong ping_pong )

add_executable( round_robin round_robin.cpp )
add_test( round_robin round_robin )

add_executable( forward_result forward_result.cpp )
add_test( forward_result forward_result )

add_executable( forward_result_sink forward_result_sink.cpp )
add_test( forward_result_sink forward_result_sink )

add_executable( future future.cpp )
add_test( future future )

add_executable( life life.cpp life.hpp )
add_test( life life )

add_executable( exception exception.cpp )
add_test( exception exception )

add_executable( object_types object_types.cpp )
add_test( object_types object_types )

add_executable( sieve sieve.cpp )
add_test( bench_sieve sieve 20000 )

add_executable( sieve2 sieve2.cpp )
add_test( sieve2 sieve2 1000 )

add_executable( fib fib.cpp )
add_test( fib fib 20 )

add_executable( queue_control queue_control.cpp )
add_test( queue_control queue_control )

add_executable( fifo fifo.cpp )
add_test( fifo fifo 1000000 )

add_executable( fifo2 fifo2.cpp )
add_test( fifo2 fifo2 1000000 )

add_library( active_socket active_socket.cpp active_socket.hpp )
add_executable( philosophers philosophers.cpp )

# Sometimes takes too long to run
# add_test( philosophers philosophers 2 )

IF( OPENGL_FOUND AND GLUT_FOUND AND NOT APPLE )
	add_executable( mandelbrot mandelbrot.cpp mandelbrot.hpp )
		target_link_libraries( mandelbrot ${GL_LIBRARIES} ${GLUT_LIBRARIES} )
ELSE()
	message( "Skipping Mandelbrot sample because you don't appear to have OpenGL and GLUT installed" )
ENDIF()

link_libraries( active_socket )
add_executable( echo_server echo_server.cpp )
add_executable( echo_client echo_client.cpp )
add_executable( echo echo.cpp )

add_executable( santa santa.cpp )

if( ACTIVE_USE_CXX11 )
	add_executable( hello_active_lambda hello_active_lambda.cpp )
	add_test( hello_active_lambda hello_active_lambda )

	add_executable( ping_pong_lambda ping_pong_lambda.cpp )
	add_test( ping_pong_lambda ping_pong_lambda )

	add_executable( round_robin_lambda round_robin_lambda.cpp )
	add_test( round_robin_lambda round_robin_lambda)

	add_executable( forward_result_lambda forward_result_lambda.cpp )
	add_test( forward_result_lambda forward_result_lambda )

	add_executable( forward_result_iface_lambda forward_result_iface_lambda.cpp )
	add_test(forward_result_iface_lambda forward_result_iface_lambda)

	add_executable( forward_result_sink_lambda forward_result_sink_lambda.cpp )
	add_test( forward_result_sink_lambda forward_result_sink_lambda )

	add_executable( future_lambda future_lambda.cpp )
	add_test( future_lambda future_lambda )

	add_executable( sieve_lambda sieve_lambda.cpp )
	add_test( sieve_lambda sieve_lambda 1000 )

	add_executable( sieve2_lambda sieve2_lambda.cpp )
	add_test( sieve2_lambda sieve2_lambda 1000 )

	add_executable( fib_lambda fib_lambda.cpp )
	add_test( fib_lambda fib_lambda 25 )

	add_executable( forward_result_iface forward_result_iface.cpp )
	add_test( forward_result_iface forward_result_iface )

	add_executable( queue_control_lambda queue_control_lambda.cpp )
	add_test( queue_control_lambda queue_control_lambda )
endif()

