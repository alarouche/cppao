#IF(APPLE)
#   INCLUDE_DIRECTORIES ( /System/Library/Frameworks )
#   FIND_LIBRARY(COCOA_LIBRARY Cocoa)
#   FIND_LIBRARY(GLUT_LIBRARY GLUT )
#   FIND_LIBRARY(OpenGL_LIBRARY OpenGL )
#   MARK_AS_ADVANCED (COCOA_LIBRARY
#                     GLUT_LIBRARY
#                     OpenGL_LIBRARY)
#   SET(EXTRA_LIBS ${COCOA_LIBRARY} ${GLUT_LIBRARY} ${OpenGL_LIBRARY})
#ENDIF (APPLE)

# SET(EXTRA_LIBS ${OpenGL_LIBRARY})

#Find_Package(GLUT)
#Find_Package(OpenGL)


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
add_executable( exception exception.cpp )
add_executable( object_types object_types.cpp )
add_executable( sieve sieve.cpp )
add_executable( sieve2 sieve2.cpp )
add_executable( mandelbrot mandelbrot.cpp )
add_library( active_socket active_socket.cpp active_socket.hpp )
target_link_libraries( mandelbrot )

link_libraries( active_socket )
add_executable( echo_server echo_server.cpp )
add_executable( echo_client echo_client.cpp )
add_executable( echo echo.cpp )

# Just comment these out if they don't build
# add_executable( shootout shootout.cpp )
# target_link_libraries( shootout boost_system )

target_link_libraries( mandelbrot glut )
