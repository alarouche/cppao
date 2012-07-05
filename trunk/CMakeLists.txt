cmake_minimum_required(VERSION 2.8)
project(active_object)
enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread")
set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++0x")
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

include_directories( include )
add_subdirectory( lib )
add_subdirectory( tests )
add_subdirectory( samples )

install(DIRECTORY include/active DESTINATION include)


