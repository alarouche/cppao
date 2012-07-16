add_executable( active_tests active_tests.cpp )
add_test( active_tests active_tests )
target_link_libraries( active_tests cppao ${EXTRA_LIBS} )
