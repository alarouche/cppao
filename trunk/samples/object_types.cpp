
/* Sample to demonstrate the various object types.
 * As you can see, they all work in pretty much the same way,
 * but have their own foibles and personalities.
 */

#include <active/object.hpp>
#include <active/thread.hpp>
#include <active/shared.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>
#include <active/fast.hpp>
#include <cstdio>	// std::cout etc is not threadsafe

struct standard_object : public active::object<standard_object>
{
	void active_method(const char * greeting) { printf("Standard object says %s\n", greeting); }
};

struct shared_object : public active::shared<shared_object>
{
	void active_method(const char * greeting) { printf("Shared object says %s\n", greeting); }
};

struct fast_object : public active::object<fast_object, active::fast>
{
	void active_method(const char * greeting) { printf("Fast object says %s\n", greeting); }
};

struct thread_object : public active::object<thread_object, active::thread>
{
	void active_method(const char * greeting) { printf("Thread object says %s\n", greeting); }
};

struct shared_thread : public active::shared<shared_thread,active::thread>
{
	void active_method(const char * greeting) { printf("Shared thread object says %s\n", greeting); }
};

struct direct : public active::object<direct,active::direct>
{
	void active_method(const char * greeting) { printf("Direct object says %s\n", greeting); }
};

struct mutexed : public active::object<mutexed,active::synchronous>
{
	void active_method(const char * greeting) { printf("Synchronous object says %s\n", greeting); }
};


int main()
{
	standard_object obj1;
	obj1("Hello");

	shared_object::ptr obj2(new shared_object);
	(*obj2)("Hello");
	obj2.reset();	// Object is not destroyed until all messages processed.

	fast_object obj3;
	obj3("Hello");

	thread_object obj4;
	obj4("Hello");

	shared_thread::ptr obj5(new shared_thread);
	(*obj5)("Hello");
	obj5.reset();	// Object is not destroyed until all messages processed.

	direct obj6;
	obj6("Hello");

	mutexed obj7;
	obj7("Hello");

	printf("Now running object pool...\n");
	active::run(4);
}
