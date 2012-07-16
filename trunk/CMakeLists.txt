cmake_minimum_required(VERSION 2.8)
project(active_object)
enable_testing()

option(ENABLE_CXX11 "Whether to compile for C++11" ON)
option(ENABLE_BOOST "Whether to compile for Boost" OFF)

if(ENABLE_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread")
	set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
	set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++0x")
	set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

	add_definitions(-DACTIVE_USE_CXX11=1)
endif()

if(ENABLE_BOOST)
	add_definitions(-DACTIVE_USE_BOOST=1)
	find_package(Boost REQUIRED COMPONENTS thread )
	set(EXTRA_LIBS ${Boost_THREAD_LIBRARY})
endif()

include_directories( include )
add_subdirectory( lib )
add_subdirectory( tests )
add_subdirectory( samples )

install(DIRECTORY include/active DESTINATION include)


