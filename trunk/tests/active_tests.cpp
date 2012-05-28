#undef NDEBUG
#include <active_object.hpp>
#include <iostream>
#include <cassert>

struct counter : public active::object
{
	int count;
	counter() : count(0) { }
	struct inc {};
	ACTIVE_METHOD(inc) { ++count; }	
};

void test_method_call()
{
	counter t;
	assert( !t.count );
	t(counter::inc());
	assert( !t.count );
	t.run();
	assert( t.count );
}

void test_run_all()
{
	counter t;
	t(counter::inc());
	t(counter::inc());
	t(counter ::inc());
	assert( t.count == 0 );
	t.run();
	assert( t.count == 3 );
}

void test_run_some()
{
	counter t;
	t(counter::inc());
	t(counter::inc());
	t(counter ::inc());
	assert( t.count == 0 );
	t.run_some(2);
	assert( t.count == 2 );
	t.run();
}

struct iface
{
	struct foo {};
	ACTIVE_IFACE(foo);
};

struct test_obj : public active::object, public iface
{
	bool foo_called=false;
	ACTIVE_METHOD(foo)
	{
		foo_called=true;
	}
};

void test_iface()
{
	test_obj obj;
	static_cast<iface&>(obj)(iface::foo());
	assert( !obj.foo_called );
	obj.run();
	assert( obj.foo_called );
}

struct const_obj : public active::object
{
	typedef struct foo{int x;} * foop;
	ACTIVE_METHOD(foop) const
	{
		foop->x=1;
	}
};

void test_const()
{
	const_obj c;
	const_obj::foo f={0};
	c(&f);
	assert( f.x==0 );
	c.run();
	assert( f.x==1 );
};

struct ni : public active::object
{
	struct foo {};
	typedef struct bar {int r;} *barp;
	ACTIVE_METHOD(foo);
	ACTIVE_METHOD(barp) const;
	int foo_called=0;
};

void ni::ACTIVE_IMPL(foo)
{
	++foo_called;
}

void ni::ACTIVE_IMPL(barp) const
{
	++barp->r;
}

void test_not_inline()
{
	ni obj;
	ni::bar msg = { 0 };
	obj(ni::foo());
	obj(&msg);
	obj.run();
	assert( obj.foo_called==1 );
	assert( msg.r==1 );
}

struct msg {};

struct sink_obj : public active::object, public active::sink<msg>
{
	int called=0;
	ACTIVE_METHOD(msg)
	{
		++called;
	}
};

void test_sink()
{
	sink_obj obj;
	static_cast<active::sink<msg>&>(obj)(msg());
	obj.run();
	assert( obj.called==1 );
};

struct mmobject : public active::object
{
	struct foo { int x; };
	struct bar { int x; };
	ACTIVE_METHOD(foo)
	{
		foo_value += foo.x;
	}
	ACTIVE_METHOD(bar)
	{
		bar_value += bar.x;
	}
	int foo_value=0;
	int bar_value=0;
};

void test_multiple_methods()
{
	mmobject obj;
	mmobject::foo foo = { 2 };
	mmobject::bar bar = { 3 };
	obj(foo);
	obj(foo);
	obj(bar);	// TODO: Want chain notation.
	obj(bar);	// TODO: Want chain notation.
	obj.run();
	assert( obj.foo_value == 4 );
	assert( obj.bar_value == 6 );
}

void test_multiple_instances()
{
	counter counters[100];
	for(int i=0; i<100; ++i)
	{
		counters[i](counter::inc());
		counters[i](counter::inc());
	}
	active::run();
	for(int i=0; i<100; ++i)
		assert( counters[i].count == 2 );
}

void test_multiple_separate_instances()
{
}
void test_default_pool()
{
}

void test_pool()
{
}

void test_thread_pool()
{
}

int main()
{
	// Single-object tests
	test_method_call();
	test_run_all();
	test_run_some();
	test_iface();
	test_const();
	test_not_inline();
	test_sink();
	test_multiple_methods();
	
	// Multiple-object tests
	test_multiple_instances();
	test_multiple_separate_instances();
	test_pool();
	test_default_pool();
	test_thread_pool();
	
	// Exceptions
	
	// Shared objects
	
	std::cout << "All tests passed!\n";
	return 0;
}
