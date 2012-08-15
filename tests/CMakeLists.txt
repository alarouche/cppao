add_executable( active_tests active_tests.cpp )
add_test( active_tests active_tests )
target_link_libraries( active_tests cppao ${EXTRA_LIBS} )

add_executable( test_fifo test_fifo.cpp )
add_test( test_fifo test_fifo )
