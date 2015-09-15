# Downloading #

C++ Active Objects (cppao) is a small library, which is distributed in source code form from the [download page](http://code.google.com/p/cppao/downloads/list).

# Supported platforms #

Cppao is regularly validated on the following platforms:

  * Visual Studio 11
  * Linux, g++ 4.6
  * Xcode 4.2

Cppao should work on any platform where [Boost](http://www.boost.org) and [Cmake](http://www.cmake.org) are available.

# Prerequisites #

Windows:

  * Visual Studio 11 (Beta)

or

  * CMake
  * Boost
  * Visual Studio

GNU/Linux/Cygwin/Macports:

  * g++ 4.6 or higher
  * CMake

OSX:

  * XCode
  * CMake
  * Boost

# Platform-specific instructions #

## Visual Studio 11 ##
A solution file has been provided for Visual Studio 11. Just open this to compile the library, tests and the samples. You can hack in this solution if you just want to experiment.

Copy the generated lib file, the include directory **and config.hpp** to a more permanent location, and link your project to the library in the usual way.

## Older versions of Visual Studio ##

You can use CMake to generate solution files for older versions of Visual Studio. Download the CMake Windows installer from http://www.cmake.org, and also install Boost from http://www.boost.org.

When you run the CMake generator specify ACTIVE\_USE\_BOOST=1, ACTIVE\_USE\_CXX11=0 and BOOST\_ROOT.

## GNU/Linux ##
Use CMake to generate a Makefile. You must install the `cmake` package.

Three build types are available:

  * C++11 without Boost (default): cmake
  * C++11 and Boost: cmake -DACTIVE\_USE\_BOOST=1
  * C++03 and Boost: cmake -DACTIVE\_USE\_BOOST=1 -DACTIVE\_USE\_CXX11=0

```
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release
$ make all test
...
$ sudo make install
$ samples/hello_active
Hello, world!
$ 
```

Note that in order to run the Mandelbrot sample, you need various OpenGL packages: freeglut3-dev libxmu-dev libxi-dev libgl1-mesa-swx11

If you `make install`, then you can link against the library in future projects using `-lcppao`.

## Xcode ##
First you need to install Boost, and follow the instructions to install it to `/usr/local`, something like:
```
$ ./bootstrap.sh
$ ./b2
$ sudo ./b2 install
```

Use cmake to create an xproj file, then open that file in Xcode.
```
$ mkdir build
$ cd build
$ cmake -GXcode -DACTIVE_USE_CXX11=0 -DACTIVE_USE_BOOST=1 ..
```

Note: C++11 is supported by clang however there is an internal compiler error so is better avoided.

## Macports ##
The following works for me; you may need to hack this around a bit to get the Mandelbrot sample to work.
```
$ sudo port install freeglut
$ mkdir build
$ cd build
$ cmake -DCMAKE_CXX_COMPILER=g++-mp-4.7 -DCMAKE_CXX_FLAGS="-std=c++11 -pthread -D_GLIBCXX_USE_NANOSLEEP -L/opt/local/lib" ..
```