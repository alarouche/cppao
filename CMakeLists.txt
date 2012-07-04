cmake_minimum_required(VERSION 2.8)
project(active_object)
enable_testing()

set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++0x")
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

include_directories( include )
add_subdirectory( lib )
add_subdirectory( tests )
add_subdirectory( samples )


