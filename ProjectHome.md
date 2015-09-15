# News #

18th September 2012 - version 0.4 released! This version includes many performance improvements, improvements to the testing suite, and variadic message handlers.

# Purpose #
Cppao is a C++ library to make it simpler to deliver robust high-performance multi-threaded programs using active objects.

The main benefit of Cppao is its simple syntax. Advanced features are provided such as

  * High performance using a bespoke scheduler,
  * Scalability to millions of concurrent objects,
  * A variety of object types for performance,
  * Actors,
  * Message queue size limits,
  * Message prioritization,
  * Multiple resource pools.

# What are active objects and why do I need them? #

Active objects are an established high-level technique whereby each object runs in its own logical thread, and method calls are executed asynchronously. The program is naturally multi-threaded and better able to exploit the hardware.

This is a much simpler and more robust approach to multi-threading, since each active object is thread-safe. Low-level multi-threading facilities (`std::mutex`, `std::thread` etc.) are harder to use and so programmers avoid them.

http://en.wikipedia.org/wiki/Active_object

_Actors_ are similar to active objects, but are limited to passing messages between objects instead of dispatching method calls.

http://en.wikipedia.org/wiki/Actor_model

# Example #
```
#include <active/object.hpp>
#include <iostream>

class HelloActive : public active::object<HelloActive>
{
public:
        void active_method(const char * sender, const char * message)
        {
                std::cout << sender << " says " << message << std::endl;
        }
};

int main()
{
        HelloActive hello;
        hello("Bob", "hello");
        active::run();
}
```

The basic concept is simple: each active object is derived from the class `active::object` and uses `active_method()` to define new active methods (which should not be called directly!). Messages are sent using `operator()`. The scheduler `active::run()` runs until all messages are processed.

The next example displays a list of prime numbers. This program really does use all CPU cores, and scales to millions of active objects.
```
class Prime : public active::shared<Prime>
{
public:
        Prime(int p) : prime(p) { std::cout << p << "\n"; }

        void active_method(int filter)
        {
                if(filter % prime)
                {
                        if(next)
                                (*next)(filter);
                        else
                                next.reset( new Prime(filter) );
                }
        }

private:
        ptr next;
        const int prime;
};

class Source : public active::object<Source>
{
public:
        Source(int m) : max(m) { }

        void active_method(int number)
        {
                if(head)
                        (*head)(number);
                else
                        head.reset( new Prime(number) );

                if( number < max )
                        (*this)(number+1);
        }
private:
        Prime::ptr head;
        const int max;
};

int main(int argc, char**argv)
{
        int max = argc<2 ? 100000 : atoi(argv[1]);
        Source source(max);
        source(2);
        active::run();
}
```

For more examples, see the [Samples](Samples.md). See the [Tutorial](Tutorial.md) to get started!