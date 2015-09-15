# Introduction #

Active objects (AOs) are an established concurrent programming technique where objects communicate via messages rather than by direct method calls. This makes active objects thread-safe because they can only process one message at a time, whilst at the same time different AOs can run on different CPU cores and are guaranteed not to interfere with each other.

C++ Active Objects (cppao) is a library to make it simple to implement active objects in C++.

# Getting started #

See the [build instructions](BuildInstructions.md) on how to download and install the cppao library.

# Hello active! #
The first example is
[hello\_active.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/hello_active.cpp)
```
#include <active/object.hpp>
#include <iostream>

class HelloActive : public active::object<HelloActive>
{
public:
        void active_method(const char * sender, const char * msg)
        {
                std::cout << sender << " says " << msg << "!\n";
        }
};

int main()
{
        HelloActive hello;
        hello("Bob", "hello");
        active::run();
}
```

Since this is the first example, let's go through it line by line:

```
#include <active/object.hpp>
```

This includes the header file, you need to do this.

```
#include <iostream>
```

This includes a system header file, which we'll need to output text.

```
class HelloActive : public active::object<HelloActive>
```

This line defines a new class, which inherits from `active::object`. `active::object` is supplied the name of your class so that it knows where to dispatch methods to.

```
        void active_method(const char * sender, const char * msg)
```
Defines a new active method, which in this case takes two arguments.
```
        {
                std::cout << sender << " says " << msg << "!\n";
        }
```
Implements the active method.

```
        HelloActive hello;
```

Create a new instance of the active object of type `HelloActive`.

```
        hello("Bob", "hello");
```
An asynchronous call to the object, which returns immediately but does not run until the scheduler tells it to. Active methods are queued and run one at a time.

Notice that we didn't call `active_method()` directly. That would be bad because it would executed immediately and potentially unsafely.  Ideally `active_method`s are private and have a line like `friend class active::object<HelloActive>;` in the derived class

```
        active::run();
```

Runs all active objects in a thread pool until there are no more messages.

# Ping pong! #

Our next example is almost as simple as the first:
[ping\_pong.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/ping_pong.cpp)
This just shows how active objects can implement several different active methods. Active methods are overloaded using different parameter types.

# Round Robin #

This example [round\_robin.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/round_robin.cpp) shows how multiple objects can interact. Here we have 1000 instances of the same class.

```
#include <active/object.hpp>
#include <cstdio>

/* A slightly more sophisticated example.
 * In this case, each node punts a message to its next node in a loop.
 * To make things interesting, we add lots of messages concurrently.
 */

struct RoundRobin : public active::object<RoundRobin>
{
        RoundRobin * next;

        void active_method( int packet )
        {
                printf( "Received packed %d\n", packet );
                if( packet>0 ) (*next)(packet-1);
        }
};

int main()
{
        // Create 1000 nodes.
        const int Count=1000;
        RoundRobin nodes[Count];

        // Link them together
        for(int i=0; i<Count-1; ++i) nodes[i].next = nodes+i+1;
        nodes[Count-1].next=nodes;

        // Send each node a packet.
        for(int i=0; i<Count; ++i) nodes[i](10);

        active::run();
}
```

# Returning results #

Active method calls are non-blocking asynchronous calls, which cannot return a result (or an exception) to the caller. Instead different mechanisms are required to get the results of active method calls.

## Approach 1: Use a promise ##

One approach is to use a _promise_, which is a fairly common solution to this problem, as follows:

```
    std::promise<int> value;
    add(1,2,value);
    int result = value.get_future().get();
```

This is actually quite a bad idea, because you are fundamentally muddling the synchronous and asynchronous paradigms. The active object you are calling from becomes blocked, the whole program can deadlock, and you are potentially hogging an entire OS thread. There is nothing wrong with futures _per se_, but they are a different paradigm.

To interact better with the cppao scheduler, the function `active::wait()` is provided to work-steal whilst the thread is waiting. This is demonstrated in [future\_lambda.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/future_lambda.cpp).

## Approach 2: Notify another object ##
Specify a callback (either in the active method call, or via some fixed topology) which the active method must call once the computation is complete.  This is demonstrated in [forward\_result.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/forward_result.cpp)

## Approach 3: Use a message sink ##
The class `active::sink<>` provides a pure virtual base class which is used to decouple the sender from the recipient. It is illustrated in [forward\_result\_sink.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/forward_result_sink.cpp) as follows:

```
#include <active/object.hpp>
#include <active/sink.hpp>
#include <iostream>

class ComplexComputation : public active::object<ComplexComputation>
{
public:
        void active_method(int a, int b, active::sink<int> * handler)
        {
                handler->send(a+b);
        }
};

class ComputationHandler :
        public active::object<ComputationHandler>,
        public active::handle<ComputationHandler,int>
{
public:
        void active_method(int result)
        {
                std::cout << "Result of computation = " << result << std::endl;
        }
};


int main()
{
        ComputationHandler handler;
        ComplexComputation cc;
        cc(1,2,&handler);
        active::run();
}
```

Notice that we must `#include <active/sink.hpp>`, pass an interface of type `active::sink<>` to the computation, and implement the callback by inheriting from `active::handle<>`. `active::sink/handle` are actually variadic, and are just a shorthand for writing a pure virtual base class.

## Approach 4: Use `active::promise` ##

We can combine `active::sink<>` and `std::promise` using the class `active::promise<>`. This is better because the call is compatible with either a promise or a call-back.

This is demonstrated in [future.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/future.cpp).

# Lambda methods #

In C++11, one has the opportunity to implement active methods as lambdas. Code for this looks as follows ([hello\_active\_lambda.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/hello_active_lambda.cpp)):

```
#include <active/object.hpp>
#include <iostream>

class HelloActive : public active::object<HelloActive>
{
public: 
        void greet(const char * msg)
        {
                active_fn([=]{ std::cout << msg << std::endl; });
        }
};

int main()
{
        HelloActive hello;
        hello.greet("Hello, world!");
        active::run();
}
```
In this case, the active method (which can now have a sensible name) enqueues the real implementation using a call to `active_fn()`.

Note that one could also achieve something similar in C++03 using `boost::bind()`.

It is a matter of personal taste as to which approach is preferred.

# Const `active_method`s #
`active_method()` respects `const`, which means that you can overload `active_method()` with a const version, and you can also control which overload gets called depending on whether you invoke `operator()` or `operator() const`.

# Exception handling #

In the same way that return values can't be delivered to the point of calling, so exceptions can't be propagated to the caller either. So where to deliver the exception?

Instead they are delivered to the `exception_handler()` method (which is not an active method), which can rethrow, catch and handle the exception. If `exception_handler()` emits an exception, then the application terminates.

You can also deliver exceptions via `std::promise`.

Of course, if an active method wants to do something different, then it must implement `try...catch` itself.

# Using shared pointers #

The examples so far have relied on raw C-style pointers, mainly for simplicity. This has the obvious drawback that you might end up with dangling pointers, or that objects are destroyed whilst there are still messages on their queue.

You could also use references but the same problem arises.

To solve this, the class `active::shared<>` can be used as a base class for your AO, which will guarantee that the object will only be destroyed after all messages have been processed.

The sample [object\_types.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/object_types.cpp) demonstrates this.

```
#include <active/shared.hpp>

struct shared_object : public active::shared<shared_object>
{
        void active_method(const char * greeting) { std::cout << "Shared object says " << greeting << std::endl; }
};
```

# Object types #
There are other object types which are available by deriving from a different base class as shown in the following table:

| Object type | Header file | Base class | Shared base class | Description | Use case |
|:------------|:------------|:-----------|:------------------|:------------|:---------|
| `advanced`  | `#include <active/advanced.hpp>` | `active::object<T,active::advanced>` | `active::shared<T,active::advanced>` | More feature-rich active object type which supports message prioritization, limit queue size and query queue size. | When these features are needed. |
| `basic`     | `#include <active/object.hpp>` | `active::object<T>` | `active::shared<T>` | Default active object. | Default use. |
| `direct`    | `#include <active/direct.hpp>` | `active::object<T,active::direct>` | `active::shared<T,active::direct>` | Non-mutexed, zero-overhead object. | Simple connecting objects. Objects with no mutable state. |
| `fast`      | `#include <active/fast.hpp>` | `active::object<T,active::fast>` | `active::shared<T,active::fast>` | Runs the method in the calling thread if available. | Lightweight processing. |
| `synchronous` | `#include <active/synchronous.hpp>` | `active::object<T,active::synchronous>` | `active::shared<T,active::synchronous>` | Execute call in calling thread. | Lightweight processing. |
| `thread`    | `#include <active/thread.hpp>` | `active::object<T,active::thread>` | `active::shared<T,active::thread>` | Dedicate an OS thread to this AO. | Blocking calls, IO, fair scheduling. |

The sample [object\_types.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/object_types.cpp) shows the different implementations.

Object types can be mixed freely.

# Blocking vs. non-blocking design #

When designing concurrent applications, there is a fundamental choice between synchronous (blocking) calls, and asynchronous (non-blocking) calls. The message-queue approach of actors lends itself more naturally to the non-blocking calls, where calls are essentially "fire and forget" with some guarantees about delivery at some point in the future.

Blocking calls are possible in Cppao. For example, calls to the `active::synchronous` and `active::advanced` object types can block. Other function calls (such as IO functions) can also block, and there is nothing preventing the use of mutexes, conditions or other synchronised objects in conjunction with Cppao. It is possible, even recommended, to use futures to return results to the caller.

Blocking calls should be used with caution however, because they fundamentally tie up an entire OS thread, and if `active::run()` was called with the default number of threads, then this will be limited to the number of CPU cores, so holding up a CPU core is potentially bad for performance. Blocking calls will also prevent all other use of the active object, and the application can deadlock.

One solution to the thread-pool problem is to use the function or method `wait()`, which performs work in the caller thread whilst waiting. Another solution is to increase the size of the thread pool, e.g.
```
    active::run(32);		// Run with 32 threads.
```
and a third solution is to ensure that objects which block are either in the main thread or have the `active::thread` object type, i.e.
```
    class MyBlockingObject : public active::object<MyBlockingObject, active::thread>
    ...	
```
See [future.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/future.cpp) for an example of using `wait()`. The examples [fifo.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/fifo.cpp) and [fifo2.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/fifo2.cpp) show different implementations of a FIFO, one without blocking calls, and another using blocking calls to the `advanced` object type to manage the queue size. The blocking design is quite a bit clearer, but I like the purity of a purely asynchronous design, even if it sometimes induces a headache.

# Advanced queue control #
The `advanced` object type offers some more facilities than the basic type. These include:

  * Limit the queue size, using `set_capacity()` and `get_capacity()`,
  * Query current queue size,
  * Control the behaviour when the queue gets full using `set_queue_policy()`:
    * `active::policy::ignore` to deliver the message anyway (the default),
    * `active::policy::fail` to throw std::bad\_alloc to the caller,
    * `active::policy::block` to block the caller until space is available in the queue,
    * `active::policy::discard` to discard excess messages.
  * Prioritize calls, so calls are handled in priority order as well as order of receiving.
  * Peek at the next active method using `get_priority()`.

See [queue\_control.cpp](http://code.google.com/p/cppao/source/browse/trunk/samples/queue_control.cpp) for an example of some of these.

# Staying safe #
Active objects guarantee that your program will be thread-safe, if you follow these guidelines:

  * Communicate only via active methods:
    * Make all mutable class-members private.
    * Make all non-active methods private.
  * Never call `active_method()` directly. Ideally make `active_method()` private and friend your base class.
  * Ensure arguments are thread-safe:
    * Restrict arguments value-types and pointers to AOs.
    * Use move semantics (`std::move()` etc.) when passing large arrays between objects.
    * Avoid (shared) pointers to anything else, unless they are `unique()`.
    * Avoid weak pointers in arguments, as they aren't needed.
  * Ensure all pointers and references are valid: use smart pointers.
  * Avoid blocking calls (e.g. network, `std::future<>::get()`), unless using `active::thread`. Use `active::wait()` when waiting on a promise.

The beauty of cppao is that deadlock cannot occur - what happens is that your program runs out of messages and exits. (Blocking calls, blocking message queues and futures all reintroduce the possibility of deadlock.)

# Where next? #
Now you have the tools, it is time to apply them! More examples can be found in the [Samples](Samples.md), and more details are in the [Reference](Reference.md).