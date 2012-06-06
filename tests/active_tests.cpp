#undef NDEBUG
#include <active_object.hpp>
#include <iostream>
#include <cassert>
#include <vector>

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
	active::run();	// NB: You must do this to clear the default queue if you plan on using this again
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
	active::run();
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
	active::run();
}

struct iface
{
	struct foo {};
	ACTIVE_IFACE(foo);
};

struct test_obj : public active::object, public iface
{
	bool foo_called; // =false;  VC++11 does not support C++11
	test_obj() : foo_called(false) { }
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
	active::run();
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
	active::run();
};

struct ni : public active::object
{
	struct foo {};
	typedef struct bar {int r;} *barp;
	ACTIVE_METHOD(foo);
	ACTIVE_METHOD(barp) const;
	int foo_called; // =0;
	ni() : foo_called(0) {}
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
	active::run();
}

struct msg {};

struct sink_obj : public active::object, public active::sink<msg>
{
	int called; // =0;
	sink_obj() : called(0) {}
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
	active::run();
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
	int foo_value; // =0;
	int bar_value; // =0;
	mmobject() : foo_value(0), bar_value(0) {}
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
	active::run();
}

void test_multiple_instances()
{
	const int N=100;
	counter counters[N];
	for(int i=0; i<N; ++i)
	{
		counters[i](counter::inc());
		counters[i](counter::inc());
	}
	active::run();
	for(int i=0; i<N; ++i)
		assert( counters[i].count == 2 );
}

struct object2;

struct object1 : public active::object
{
	struct foo { int x; };
	ACTIVE_METHOD( foo );
	int foo_value;
	object1() : foo_value(0) { }
	object2 * other;
};

struct object2 : public active::object
{
	struct foo { int x; };
	ACTIVE_METHOD( foo )
	{
		foo_value += foo.x;
		if( foo.x>0 )
		{
			object1::foo f2 = {foo.x-1};
			(*other)(f2);
		}
	}
	int foo_value;
	object2() : foo_value(0) { }
	object1 * other;
};

void object1::ACTIVE_IMPL(foo)
{
	foo_value += foo.x;
	if( foo.x>0 )
	{
		object2::foo f2 = { foo.x-1 };
		(*other)(f2);
	}
}

void test_multiple_separate_instances()
{
	object1 o1[10];
	object2 o2[10];
	for(int i=0; i<10; ++i)
	{
		o1[i].other = &o2[i];
		o2[i].other = &o1[i];
		object1::foo f = { 10 };
		o1[i](f);
	}
	active::run();
	for(int i=0; i<10; ++i)
	{
		assert( o1[i].foo_value == 30 );	// 10+8+6+4+2
		assert( o2[i].foo_value == 25 );	// 9+7+5+3+1
	}
}

void test_pool()
{
	active::pool pool;

	const int N=10, M=10;
	object1 o1[N];
	object2 o2[N];
	for(int i=0; i<N; ++i)
	{
		o1[i].set_scheduler(pool);
		o2[i].set_scheduler(pool);
		o1[i].other = &o2[i];
		o2[i].other = &o1[i];
		object1::foo f = { M };
		o1[i](f);
	}
	pool.run();
	for(int i=0; i<N; ++i)
	{
		assert( o1[i].foo_value == M * (M+2) / 4 );	// 10+8+6+4+2
		assert( o2[i].foo_value == M * M / 4 );	// 9+7+5+3+1
	}
}

void test_thread_pool()
{
	const int N=1000, M=100;
	object1 o1[N];
	object2 o2[N];
	for(int i=0; i<N; ++i)
	{
		o1[i].other = &o2[i];
		o2[i].other = &o1[i];
		object1::foo f = { M };
		o1[i](f);
	}
	active::run(23);
	for(int i=0; i<N; ++i)
	{
		assert( o1[i].foo_value == M * (M+2) / 4 );	// 10+8+6+4+2
		assert( o2[i].foo_value == M * M / 4 );	// 9+7+5+3+1
	}
}

struct except_object : public active::object
{
	bool caught;

	struct ex { };

	void exception_handler()
	{
		try
		{
			throw;
		}
		catch( ex )
		{
			caught=true;

			// If the exception handler fails, we would get std::terminate();
			// just like normal C++.
		}
	}


	struct msg { };

	ACTIVE_METHOD( msg )
	{
		throw ex();
	}

	except_object() : caught(false) { }
};

void test_exceptions()
{
	except_object eo;
	eo(except_object::msg());
	active::run();
	assert( eo.caught );
}

struct my_shared : public active::shared<my_shared>
{
	typedef int msg;
	ACTIVE_METHOD(msg)
	{
		if( msg>0 ) (*this)(msg-1);
	}
};

void test_shared()
{
	// Messages keep the object alive
	std::shared_ptr<my_shared> obj1(new my_shared), obj2(new my_shared);

	(*obj1)(20); (*obj2)(40);
	std::weak_ptr<my_shared> r1(obj1), r2(obj2);
	obj1.reset();
	obj2.reset();

	assert( std::shared_ptr<my_shared>(r1) );
	assert( std::shared_ptr<my_shared>(r2) );

	// The objects are referenced by the message queue at this point
	active::run();

	assert( r1.expired() );
	assert( r2.expired() );
}



struct test_message
{
	int value;
	active::sink<int> * object;
};

struct active_object_test : 
	public active::object,
	public active::sink<int>
{	
	struct test_object
	{
		active::sink<test_message> * subject;
	};
	
	ACTIVE_METHOD( test_object )
	{
		m_expected_value=1;
		struct test_message m = { 1, this };
		(*test_object.subject)(m);
		m.value=2;
		(*test_object.subject)(m);
		m.value=3;
		(*test_object.subject)(m);
	}

	typedef int result;
	
	ACTIVE_METHOD( result )
	{
		assert( result == m_expected_value );
		++m_expected_value;
	}
	
	struct finish {};
	ACTIVE_METHOD(finish)
	{
		assert( m_expected_value == 4 );
	}
	
private:
	int m_expected_value;
};

template<typename Schedule, typename Queue, typename Shared>
struct test_object : 
	public active::object_impl<Schedule, Queue, Shared>, 
	public active::sink<test_message>
{
	typedef Queue queue_type;
	
	ACTIVE_TEMPLATE(test_message)
	{
		(*test_message.object)(test_message.value);
	}
};

void test_object2( active::sink<test_message> & ao)
{
	// !! Make this a much better test.
	active_object_test test;
	active_object_test::test_object m = { &ao };
	test(m);
	active::run();
	test(active_object_test::finish());
	active::run();
}


void test_object_types()
{
	test_object< active::schedule::thread_pool, active::queueing::separate, active::sharing::disabled> obj1;
	test_object< active::schedule::thread_pool, active::queueing::shared, active::sharing::disabled> obj2;
	test_object< active::schedule::thread_pool, active::queueing::direct_call, active::sharing::disabled> obj3;
	test_object< active::schedule::thread_pool, active::queueing::mutexed_call, active::sharing::disabled> obj4;
	
	test_object2(obj1);
	test_object2(obj2);
	test_object2(obj3);
	test_object2(obj4);
	
	typedef active::queueing::steal<active::queueing::shared> steal1;
	typedef active::queueing::steal<active::queueing::separate> steal2;

	test_object2( *std::make_shared<test_object< active::schedule::thread_pool, active::queueing::separate, active::sharing::enabled<> >>() );
	test_object2( *std::make_shared<test_object< active::schedule::thread_pool, active::queueing::shared, active::sharing::enabled<> >>() );
	test_object2( *std::make_shared<test_object< active::schedule::thread_pool, active::queueing::direct_call, active::sharing::enabled<> >>() );
	test_object2( *std::make_shared<test_object< active::schedule::thread_pool, active::queueing::mutexed_call, active::sharing::enabled<> >>() );
	test_object2( *std::make_shared<test_object< active::schedule::thread_pool, steal1, active::sharing::enabled<> >>() );
	test_object2( *std::make_shared<test_object< active::schedule::thread_pool, steal2, active::sharing::enabled<> >>() );
}

struct thread_obj : public active::thread
{
	typedef int add;
	int total;
	
	ACTIVE_METHOD( add )
	{
		total = add;
	}
};

void test_own_thread()
{
	thread_obj obj1, obj2;
	
	obj1(12);
	obj2(13);
	active::run();
	assert( obj1.total = 12 );
	assert( obj2.total == 13 );	
}

// Can we run lots of different objects simultaenously?

typedef int mix_message;
struct mix_config
{
	std::shared_ptr<active::sink<mix_message>> sink,result;
	int id;
};

struct mix_interface : public active::sink<mix_message>, public active::sink<mix_config>
{
};

template<typename Schedule, typename Queueing, typename Sharing>
struct mix_object : public active::object_impl<Schedule,Queueing,Sharing>, public mix_interface
{
	typedef Queueing queue_type;
	int m_id;
	std::weak_ptr<active::sink<mix_message>> m_sink, m_result;
	
	ACTIVE_TEMPLATE( mix_message )
	{
		std::shared_ptr<active::sink< ::mix_message>> sink(m_sink), result(m_result);
		if( mix_message>0 && sink ) (*sink)(mix_message-1);
		else if( mix_message<=0 && result) (*result)(m_id);
	}
	
	ACTIVE_TEMPLATE( mix_config )
	{
		m_id = mix_config.id;
		m_sink = mix_config.sink;
		m_result = mix_config.result;
	}
};

std::shared_ptr<mix_interface> mix_factory(int n)
{
	switch( n%22 )
	{
	case 0: return std::make_shared< mix_object<active::schedule::thread_pool, active::queueing::shared, active::sharing::disabled> >();
	case 1: return std::make_shared< mix_object<active::schedule::thread_pool, active::queueing::shared, active::sharing::enabled<>> >();
	case 2: return std::make_shared< mix_object<active::schedule::thread_pool, active::queueing::separate, active::sharing::disabled> >();
	case 3: return std::make_shared< mix_object<active::schedule::thread_pool, active::queueing::separate, active::sharing::enabled<>> >();
	case 4: return std::make_shared< mix_object<active::schedule::thread_pool, active::queueing::steal<active::queueing::shared>, active::sharing::disabled> >();
	case 5: return std::make_shared< mix_object<active::schedule::thread_pool, active::queueing::steal<active::queueing::shared>, active::sharing::enabled<>> >();
	case 6: return std::make_shared< mix_object<active::schedule::thread_pool, active::queueing::steal<active::queueing::separate>, active::sharing::disabled> >();
	case 7: return std::make_shared< mix_object<active::schedule::thread_pool, active::queueing::steal<active::queueing::separate>, active::sharing::enabled<>> >();
	case 8: return std::make_shared< mix_object<active::schedule::own_thread, active::queueing::shared, active::sharing::disabled> >();
	case 9: return std::make_shared< mix_object<active::schedule::own_thread, active::queueing::shared, active::sharing::enabled<>> >();
	case 10: return std::make_shared< mix_object<active::schedule::own_thread, active::queueing::separate, active::sharing::disabled> >();
	case 11: return std::make_shared< mix_object<active::schedule::own_thread, active::queueing::separate, active::sharing::enabled<>> >();
	case 12: return std::make_shared< mix_object<active::schedule::own_thread, active::queueing::steal<active::queueing::shared>, active::sharing::disabled> >();
	case 13: return std::make_shared< mix_object<active::schedule::own_thread, active::queueing::steal<active::queueing::shared>, active::sharing::enabled<>> >();
	case 14: return std::make_shared< mix_object<active::schedule::own_thread, active::queueing::steal<active::queueing::separate>, active::sharing::disabled> >();
	case 15: return std::make_shared< mix_object<active::schedule::own_thread, active::queueing::steal<active::queueing::separate>, active::sharing::enabled<>> >();
	case 16: return std::make_shared< mix_object<active::schedule::none, active::queueing::direct_call, active::sharing::disabled> >();
	case 17: return std::make_shared< mix_object<active::schedule::none, active::queueing::direct_call, active::sharing::enabled<>> >();
	case 18: return std::make_shared< mix_object<active::schedule::none, active::queueing::mutexed_call, active::sharing::disabled> >();
	case 19: return std::make_shared< mix_object<active::schedule::none, active::queueing::mutexed_call, active::sharing::enabled<>> >();
	case 20: return std::make_shared< mix_object<active::schedule::none, active::queueing::try_lock, active::sharing::disabled> >();
	case 21: return std::make_shared< mix_object<active::schedule::none, active::queueing::try_lock, active::sharing::enabled<>> >();
	}
}

template<typename T>
struct result_holder : public active::object, public active::sink<T>
{
	typedef T msg;
	T result;
	ACTIVE_METHOD(msg) { result = msg; }
};

void test_object_mix()
{
	const int N=313, M=1024;
	auto result = std::make_shared<result_holder<int>>();
	std::vector<std::shared_ptr<mix_interface>> vec(N);
	
	int n=0;
	for(auto &i : vec)
		i = mix_factory(n++);

	for(int n=0; n<N; ++n)
	{
		mix_config msg = { n<N-1 ? vec[n+1] : vec[0], result, n };
		static_cast<active::sink<mix_config>&>((*vec[n]))(msg);
	}
	
	static_cast<active::sink<mix_message>&>(*vec[0])(M); // 0000);
	
	active::run(13);
	
	assert( result->result == M%N );
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
	test_thread_pool();
	
	// Exceptions
	test_exceptions();
	
	// Object types
	test_shared();	
	test_object_types();
	test_own_thread();
	test_object_mix();
	
	std::cout << "All tests passed!\n";
	return 0;
}
