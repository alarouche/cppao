add_executable( bench2 bench.cpp )
add_test( bench2 bench2 quick )
target_link_libraries( bench2 cppao ${EXTRA_LIBS} )

