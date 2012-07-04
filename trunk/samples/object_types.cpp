
/* Sample to demonstrate the various object types.
 * As you can see, they all work in pretty much the same way,
 * but have their own foibles and personalities.
 */

#include <active/object.hpp>
#include <iostream>

typedef const char * greeting;

struct standard_object : public active::object
{
	ACTIVE_METHOD(greeting) { std::cout << "Standard object says " << greeting << std::endl; }
};

struct shared_object : public active::shared<shared_object>
{
	ACTIVE_METHOD(greeting) { std::cout << "Shared object says " << greeting << std::endl; }
};

struct fast_object : public active::fast
{
	ACTIVE_METHOD(greeting) { std::cout << "Fast object says " << greeting << std::endl; }
};

struct thread_object : public active::thread
{
	ACTIVE_METHOD(greeting) { std::cout << "Thread object says " << greeting << std::endl; }
};

struct shared_thread : public active::shared_thread<shared_thread>
{
	ACTIVE_METHOD(greeting) { std::cout << "Shared thread object says " << greeting << std::endl; }
};

struct direct : public active::direct
{
	ACTIVE_METHOD(greeting) { std::cout << "Direct object says " << greeting << std::endl; }
};

struct mutexed: public active::synchronous
{
	ACTIVE_METHOD(greeting) { std::cout << "Synchronous object says " << greeting << std::endl; }
};


int main()
{
	standard_object obj1;
	obj1("Hello");

	auto obj2 = std::make_shared<shared_object>();
	(*obj2)("Hello");
	obj2.reset();	// Object is not destroyed until all messages processed.

	fast_object obj3;
	obj3("Hello");

	thread_object obj4;
	obj4("Hello");

	auto obj5 = std::make_shared<shared_thread>();
	(*obj5)("Hello");
	obj5.reset();	// Object is not destroyed until all messages processed.

	direct obj6;
	obj6("Hello");

	mutexed obj7;
	obj7("Hello");

	std::cout << "Now running object pool...\n";
	active::run(4);
}
