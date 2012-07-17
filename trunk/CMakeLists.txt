cmake_minimum_required(VERSION 2.8)
project(active_object)
enable_testing()

option(ACTIVE_USE_CXX11 "Whether to compile for C++11" ON)
option(ACTIVE_USE_BOOST "Whether to compile for Boost" OFF)

if(ACTIVE_USE_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread")
	set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
	set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++0x")
	set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
endif()

if(ACTIVE_USE_BOOST)
	find_package(Boost REQUIRED COMPONENTS thread )
	set(EXTRA_LIBS ${Boost_THREAD_LIBRARY})
endif()

configure_file( include/active/config.hpp.in include/active/config.hpp @ONLY )

include_directories( ${CMAKE_CURRENT_BINARY_DIR}/include include )
add_subdirectory( lib )
add_subdirectory( tests )
add_subdirectory( samples )

install(DIRECTORY include/active DESTINATION include)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/active/config.hpp DESTINATION include/active)


