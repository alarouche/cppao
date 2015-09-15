# Header files #
| Header file | Description | Definitions |
|:------------|:------------|:------------|
| `active/advanced.hpp` | Implements a more sophisticated AO type. | [advanced](#active::advanced.md) |
| `active/direct.hpp` | Non-mutexed objects. | [direct](#active::direct.md) |
| `active/fast.hpp` |             | [fast](#active::fast.md) |
| `active/object.hpp` | The main include file, which includes all of the basics. | [basic](#active::basic.md), [default\_scheduler](#active::default_scheduler.md), [object](#active::object.md), [queue\_full](#active::policy::queue_full.md), [priority](#active::priority.md), [run](#active::run.md) |
| `active/promise.hpp` |             | [promise](#active::promise.md), [wait](#active::wait.md) |
| `active/scheduler.hpp` |             | [scheduler](#active::scheduler.md) |
| `active/shared.hpp` | Additional support for shared pointers. | [shared](#active::shared.md) |
| `active/sink.hpp` | Interface classes for AOs. | [handle](#active::handle.md), [sink](#active::sink.md) |
| `active/synchronous.hpp` |             | [synchronous](#active::synchronous.md) |
| `active/thread.hpp` | Running AOs in a dedicated OS thread. | [thread](#active::thread.md) |

# Namespace #
All declarations are in the namespace `active`.

# Classes #

## active::advanced ##
An object type which provides more advanced queue control such as call prioritization, limit queue size, query queue and clear queue.

```
typedef ... advanced;
```

## active::basic ##
An object type which provides the default implementation for active objects. This object is scheduled by the cppao scheduler (`active::scheduler`).

```
typedef ... basic;
```

## active::direct ##
An object type which is unmutexed and always executes in the calling thread. This is actually completely fine provided that the class has no mutable state or is otherwise threadsafe.

```
typedef ... direct;
```

## active::fast ##
An object type which eagerly executes in the calling thread where possible.

```
typedef ... fast;
```

## active::handle ##
Implements a message handler. Use this as a base class for any object which can be called back on a `active::sink<>` interface.

```
template<typename Derived, typename... Args>
struct handle : public sink<Args...>
{
    void send(Args...);
};
```

The `Derived` class must provide a compatible `active_method()`.

## active::object ##
This class is used as a base class for user-defined active objects.

```
template<typename Derived, typename Type=basic>
class object
{
public:
    typedef Derived derived_type;
    typedef ... scheduler_type;
    typedef ... allocator_type;
    typedef ... size_type;
    typedef Type object_type;

    object(
        scheduler_type & scheduler = default_scheduler, 
        const allocator_type & alloc = allocator_type() );

    virtual ~object();

    template<typename...Args>
    derived_type & operator()(Args ... args);

    template<typename...Args>
    const derived_type & operator()(Args ... args) const;

    size_type size() const;

    bool empty() const;

    int get_priority() const;

    scheduler_type & get_scheduler() const;

    void set_scheduler(scheduler_type&);

    allocator_type get_allocator() const;

    virtual void exception_handler();

    void set_capacity(size_type);
   
    size_type get_capacity() const;

    void set_policy(policy::queue_full);

protected:
    void clear();

    template<typename Fn>
    void active_fn(Fn && fn, int priority=0) const;
};
```

Constructor:

| `object(scheduler_type & scheduler, const allocator_type & alloc);` | Constructs and assigns the scheduler and allocator. The scheduler must not be destroyed whilst the object has unprocessed calls. |
|:--------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------|

Methods:

| `derived_type & operator()` | Call an active method. Can throw exceptions (e.g. `std::bad_alloc`) if the active method could not be allocated, the queue is full or some other copy constructor fails. Might be limited to 5 arguments depending on platform. |
|:----------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `const derived_type & operator() const` | Call a const active method.                                                                                                                                                                                                     |
| `void active_fn(Fn&&fn,int priority)` | Enqueues any functor of no arguments.                                                                                                                                                                                           |
| `void clear()`              | Removes all pending calls.                                                                                                                                                                                                      |
| `bool empty() const`        | Returns true iff there are no waiting calls.                                                                                                                                                                                    |
| `allocator_type get_allocator() const` | Retrieve the allocator.                                                                                                                                                                                                         |
| `virtual void exception_handler()` | Called whenever an active method emits an exception. Default implementation prints a warning and continues.                                                                                                                     |
| `scheduler_type & get_scheduler()` | Returns a reference to the scheduler.                                                                                                                                                                                           |
| `void set_scheduler(scheduler_type&)` | Changes the scheduler.                                                                                                                                                                                                          |

Methods implemented only on the `advanced` object type:

| `size_type get_capacity() const` | Returns the maximum available queue size. |
|:---------------------------------|:------------------------------------------|
| `int get_priority() const`       | Retrieves the priority of the next waiting call, or 0 if none exists. |
| `size_type size() const`         | Retrieve the number of waiting calls.     |
| `void set_capacity(size_type)`   | Changes the maximum number of calls in the queue. Does not throw an exception if the number of waiting items exceeds the new capacity. |
| `void set_policy(policy::queue_full)` | Changes the queue policy behaviour.       |

## active::promise ##
Provides a promise mechanism for returning results from AOs. This class implements a `sink`.

```
template<typename T> class promise : public direct, public sink<T>
{
    T get();
};
```

Methods:
| `T get()` | Blocking call to retrieve the value or exception. |
|:----------|:--------------------------------------------------|


## active::run ##
Runs a scheduler in a thread pool. Note that this class is often used as a function.
```
class run
{
public:
    run(int num_threads=std::thread::hardware_concurrency(), scheduler & sched=default_scheduler);
    ~run();
};
```

Methods:

| `run(int num_threads, scheduler & sched)` | Starts the thread pool and starts processing messages. |
|:------------------------------------------|:-------------------------------------------------------|
| `~run()`                                  | Waits for all messages to be processed and stops the thread pool. |

## active::scheduler ##
Schedules the message processing on a collection of active objects.
```
class scheduler
{
public:
    scheduler();
    scheduler(const scheduler&) = delete;
    scheduler& operator=(const scheduler&) = delete;
    void run();
    void run(int num_threads);
    bool run_one();
};
```

Methods:

| `void run()` | Runs the scheduler in a single thread. Can be called concurrently from multiple threads. |
|:-------------|:-----------------------------------------------------------------------------------------|
| `void run(int threads)` | Runs the scheduler using the specified number of threads.                                |
| `bool run_one()` | Runs one AO, returns true iff there is more work available.                              |


## active::shared ##
This implements the base class for shared active objects. Note that normal (`active::object`) can also be put into shared pointers, but then they lack `shared_from_this()` as well as non-destruction guarantees.

```
template<typename Derived, typename Type=basic>
class shared : public object<Derived, ...>
{
public:
    typedef std::shared_ptr<Derived> ptr;
    typedef std::shared_ptr<const Derived> const_ptr;

    ptr shared_from_this();
    const_ptr shared_from_this() const;

    shared(
        scheduler_type & scheduler = default_scheduler, 
        const allocator_type & alloc = allocator_type() );
};
```


## active::sink ##
Pure virtual base class used to pass messages to an active object.

```
template<typename... Args>
struct sink
{
    typedef std::shared_ptr<sink<Args...>> sp;
    typedef std::weak_ptr<sink<Args...>> wp;

    virtual void send(Args...)=0;
};
```

Methods:

| `void send(Args...)` | Enqueues the active method. |
|:---------------------|:----------------------------|

## active::synchronous ##
An object type which is (recursive) mutexed and always executes in the calling thread.
```
typedef ... synchronous;
```

## active::thread ##
An object type which uses an OS thread for scheduling.
```
typedef ... thread;
```


# Global variables #
## active::default\_scheduler ##
```
extern scheduler default_scheduler;
```
The global scheduler, used by objects by default. Global variables are of course to be discouraged, and are only provided for convenience.

# Functions #

## active::priority ##
```
namespace active
{
    template<typename T> int priority(const T&)
    {
        return 0;
    }
}
```
Used to prioritize calls to the `advanced` object type, but is ignored for all other object types.

## active::wait ##
Waits for a promise to be fulfilled. Processes items from the specified scheduler whilst waiting, so as to avoid starving and deadlocking the application.

```
template<typename T>
T wait(std::promise<T> & promise, scheduler & sched = default_scheduler);
```

# Enumerations #
## active::policy::queue\_full ##
| `block` | Block the caller until capacity becomes available. |
|:--------|:---------------------------------------------------|
| `discard` | Discard ANY message which is not highest priority. |
| `fail`  | Throw `std::bad_alloc` to the caller.              |
| `ignore` | Ignore the fact that the queue is full; deliver message anyway. |