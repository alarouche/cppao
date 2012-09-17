add_executable( active_tests active_tests.cpp )
add_test( unit_tests active_tests )
target_link_libraries( active_tests cppao ${EXTRA_LIBS} )

add_executable( bench2 bench2.cpp )
add_test( bench2 bench2 quick )
target_link_libraries( bench2 cppao ${EXTRA_LIBS} )

add_executable( test_fifo test_fifo.cpp )
add_test( fifo test_fifo )

add_executable( bench bench.cpp )
add_test( bench bench 20000 )
target_link_libraries( bench cppao ${EXTRA_LIBS} )

add_executable( bench_fib bench_fib.cpp )
target_link_libraries( bench_fib cppao ${EXTRA_LIBS} )
add_test( bench_fib bench_fib 25 )

if( ACTIVE_USE_CXX11 )
    add_executable( test_atomic test_atomic.cpp )
    target_link_libraries( test_atomic cppao ${EXTRA_LIBS} )
    add_test( test_atomic test_atomic )

    add_executable( bench_atomic bench_atomic.cpp )
    target_link_libraries( bench_atomic cppao ${EXTRA_LIBS} )
    add_test( bench_atomic_fifo bench_atomic 1 4 1000000 2 )
    add_test( bench_atomic_lifo bench_atomic 2 4 1000000 2 )

    add_executable( bench_lambda bench_lambda.cpp )
    target_link_libraries( bench_lambda cppao ${EXTRA_LIBS} )
    add_test( bench_lambda bench_lambda 50000 )
endif()
