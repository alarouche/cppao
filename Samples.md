Note: Many of these samples have a variation implemented in terms of C++11 lambdas. (These are not available in C++03...) This is to compare and contrast the two approaches.

# Hello world! #
http://code.google.com/p/cppao/source/browse/trunk/samples/hello_active.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/hello_active_lambda.cpp

A simple active method call with two parameters.  Demonstrates:

  * Defining an active method using `active_method()`.
  * Defining an active method using lambdas.
  * Instantiating an active object.
  * Calling an active method.
  * Multiple parameters to active methods.
  * Running the message loop, `active::run()`.

# Ping pong #
http://code.google.com/p/cppao/source/browse/trunk/samples/ping_pong.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/ping_pong_lambda.cpp

A message is sent backwards and forwards between two active methods in the same AO. Demonstrates:

  * Defining an AO with more than one active method.
  * Overloading `active_method` calls.

# Round robin #
http://code.google.com/p/cppao/source/browse/trunk/samples/round_robin.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/round_robin_lambda.cpp

A message is sent to each of 1000 nodes, which then pass the message onwards to the next node in the loop, with a time-to-live of 10 hops for each message. Demonstrates:

  * Multiple instances of the same class
  * Fixed topology/connections between AOs.
  * Running multiple objects concurrently using `active::run()`.

# Forward results #
http://code.google.com/p/cppao/source/browse/trunk/samples/forward_result.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/forward_result_lambda.cpp

Call an active method, which passes the result to another active object.

Demonstrates:

  * A way to interconnect objects.

# Callbacks #
http://code.google.com/p/cppao/source/browse/trunk/samples/forward_result_iface.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/forward_result_iface_lambda.cpp

Call an active method, which calls a function on completion.

Demonstrates:

  * Using `std::function` and lambdas as return mechanism.

# Message sinks #
http://code.google.com/p/cppao/source/browse/trunk/samples/forward_result_sink.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/forward_result_sink_lambda.cpp

An active method performs a calculation, then passes the result to another active object via a message sink.

Demonstrates:

  * `active::sink<>`,
  * How to pass results between objects in a more abstract way.

# Promises promises #
http://code.google.com/p/cppao/source/browse/trunk/samples/future.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/future_lambda.cpp

An active method is called, which returns its results in a promise. The main thread waits for the promise to be fulfilled.

N.B. This is close to the "traditional" solution to how AOs return results.

Demonstrates:

  * Returning results in a promise.
  * Instantiating `active::run` to run the scheduler in a scope.
  * `active::wait()` to provide a non-blocking wait.

# Queue control #
http://code.google.com/p/cppao/source/browse/trunk/samples/queue_control.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/queue_control_lambda.cpp

Sends a high priority active method to an AO to clear its pending work queue.

Demonstrates:
  * The `active::advanced` object type.
  * Prioritising messages.
  * Clearing the work queue.

# Exceptions #
http://code.google.com/p/cppao/source/browse/trunk/samples/exception.cpp

An active method throws an exception which calls an exception handler.

Demonstrates:

  * How to catch exceptions,
  * `exception_handler()`

# Prime number sieve #
http://code.google.com/p/cppao/source/browse/trunk/samples/sieve.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/sieve2.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/sieve_lambda.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/sieve2_lambda.cpp

Outputs the list of prime numbers. Creates a list of AOs which are used to filter a sequence of integers.

The second program improves upon the first by handling longer lists (>1000000), since there is a "bug" in `std::shared_ptr<>` in that it cannot represent very long linked lists without the destructor running out of stack space.

Demonstrates:

  * `active::shared<>`,
  * Dynamic creation of AOs.

# FIFO #
http://code.google.com/p/cppao/source/browse/trunk/samples/fifo.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/fifo2.cpp

A producer generates a sequence of 10000000 integers which get sent to a consumer. The queue size is limited to 100000. In the first example, a FIFO is constructed using asynchronous callbacks. In the second example, an `advanced` object type is used which blocks the sender once the size limit is reached.

Demonstrates:

  * Synchronous vs. asynchronous design,
  * Using `advanced` object type,
  * A benchmark.

# Game of life #
http://code.google.com/p/cppao/source/browse/trunk/samples/life.hpp

http://code.google.com/p/cppao/source/browse/trunk/samples/life.cpp

Implements Conway's "Game of Life" in a console.  Each cell is its own AO, in addition to a controller and a display AO. All communication between AOs is via active methods. This is an exercise in synchronisation (all cells must update synchronously) and message flows.

Demonstrates:

  * A more complex example.
  * Multiple classes with multiple active methods.
  * Using an explicit `active::scheduler`

# Object types #
http://code.google.com/p/cppao/source/browse/trunk/samples/object_types.cpp

Creates and runs different AO implementations. Demonstrates:

  * How to use different object types,
  * Different characteristics of different objects,
  * How to use shared objects for additional safety.

# Fibonacci #
http://code.google.com/p/cppao/source/browse/trunk/samples/fib.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/fib_lambda.cpp

Implements a deliberately naive recursive Fibonacci number calculator (F(n)=F(n-1)+F(n-2)), where each number spawns two active objects - resulting in an exponential increase in the number of AOs.

Demonstrates:

  * Dynamic creation of AOs.
  * A benchmark.
  * Fire-and-forget AOs, which are destroyed automatically.
  * `active::shared`
  * The fact that for trivial calculations, the `direct` object type can be best.

# Active sockets #
http://code.google.com/p/cppao/source/browse/trunk/samples/active_socket.hpp

http://code.google.com/p/cppao/source/browse/trunk/samples/active_socket.cpp

Wrapping a synchronous object inside an AO. Many POSIX/Winsock functions are provided, but as messages.

Demonstrates:

  * Use of `std::shared_ptr` to contain objects,
  * `active::shared<>`,
  * Composite objects: `socket` contains a `reader` and a `writer`; `select` contains a `select_loop`,
  * Network programming.

# Echo #
http://code.google.com/p/cppao/source/browse/trunk/samples/echo.cpp

A simple command line utility which copies text from input to output using the socket AO, as well as a special "pipe" AO which copies data between two file descriptors.

Demonstrates:

  * Tests the socket object.

# Echo client/server #
http://code.google.com/p/cppao/source/browse/trunk/samples/echo_server.cpp

http://code.google.com/p/cppao/source/browse/trunk/samples/echo_client.cpp

Building on the socket and pipe objects, we construct a TCP/IP client/server. The echo\_server simply echos data which is sent to it, and the echo\_client forwards input to the server, reads the response then outputs it. The echo\_client can bounce data between the client and the server a specified number of times.

Demonstrates:

  * Asynchronous IO,
  * Writing network applications,
  * Concurrent socket operations,
  * Multiple message sinks on the same object.

# Mandelbrot #
http://code.google.com/p/cppao/source/browse/trunk/samples/mandelbrot.hpp

http://code.google.com/p/cppao/source/browse/trunk/samples/mandelbrot.cpp

An interactive viewer of the Mandelbrot fractal. The screen is divided into 256 segments which are computed in independent active objects.

Demonstrates:
  * Dividing large problems.
  * Array processing.
  * Passing large arrays using move semantics.
  * Integrating with a GUI.
  * Running concurrently with a GUI message loop.
  * Using `active::run` as a scoped object not a function.