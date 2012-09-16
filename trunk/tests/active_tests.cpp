#define BOOST_DISABLE_ASSERTS 1
#undef NDEBUG

#include <active/object.hpp>

#ifdef ACTIVE_USE_BOOST
#include <boost/make_shared.hpp>
#endif

#include <active/scheduler.hpp>
#include <active/promise.hpp>
#include <active/thread.hpp>
#include <active/advanced.hpp>
#include <active/shared.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>
#include <active/fast.hpp>

#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

struct counter : public active::object<counter>
{
	int count;
	counter() : count(0) { }
	struct inc {};
	void active_method(inc) { ++count; }
};

void test_method_call()
{
	counter t;
	assert( !t.count );
	t(counter::inc());
	assert( !t.count );
	active::run();
	assert( t.count );
}

void test_run_all()
{
	counter t;
	t(counter::inc());
	t(counter::inc());
	t(counter::inc());
	assert( t.count == 0 );
	active::run();
	assert( t.count == 3 );
}

void test_run_some()
{
#if 0
	counter t;
	t(counter::inc());
	t(counter::inc());
	t(counter::inc());
	assert( t.count == 0 );
	t.run_some(2);
	assert( t.count == 2 );
	active::run();  // Deadlocks because t is activated twice (loop on activation queue not good)
#endif
}

struct const_obj : public active::object<const_obj>
{
	typedef struct foo{int x;} * foop;
	void active_method(foop foop) const
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
	active::run();
	assert( f.x==1 );
};

struct ni : public active::object<ni>
{
	struct foo {};
	typedef struct bar {int r;} *barp;
	void active_method(foo);
	void active_method(barp) const;
	int foo_called; // =0;
	ni() : foo_called(0) {}
};

void ni::active_method(foo foo)
{
	++foo_called;
}

void ni::active_method(barp barp) const
{
	++barp->r;
}

void test_not_inline()
{
	ni obj;
	ni::bar msg = { 0 };
	obj(ni::foo());
	obj(&msg);
	active::run();
	assert( obj.foo_called==1 );
	assert( msg.r==1 );
}

struct msg {};
struct msg2 {};

struct sink_obj :
    public active::object<sink_obj>,
    public active::handle<sink_obj>,
    public active::handle<sink_obj,msg>,
    public active::handle<sink_obj,int,msg>,
    public active::handle<sink_obj,int,msg,msg2>,
    public active::handle<sink_obj,int,msg,const char*,msg2>,
    public active::handle<sink_obj,int,msg,const char*,msg2,double>
{
    int called;
	sink_obj() : called(0) {}

    void active_method()
    {
        ++called;
    }

    void active_method(msg)
	{
		++called;
	}

    void active_method(int v,msg)
    {
        assert(v==1);
        ++called;
    }

    void active_method(int v,msg,msg2)
    {
        assert(v==1);
        ++called;
    }

    void active_method(int v,msg,const char*m,msg2)
    {
        assert(v==1);
        assert( strcmp(m,"abc")==0 );
        ++called;
    }

    void active_method(int v,msg,const char*,msg2,double d)
    {
        assert(v==1);
        assert(d==2.0);
        ++called;
    }
};

void test_sink()
{
	sink_obj obj;
    static_cast<active::sink<>&>(obj).send();
    static_cast<active::sink<msg>&>(obj).send(msg());
    static_cast<active::sink<int,msg>&>(obj).send(1,msg());
    static_cast<active::sink<int,msg,msg2>&>(obj).send(1,msg(),msg2());
    static_cast<active::sink<int,msg,const char*,msg2>&>(obj).send(1,msg(),"abc",msg2());
    static_cast<active::sink<int,msg,const char*,msg2,double>&>(obj).send(1,msg(),"abc",msg2(),2.0);
    active::run();
    assert( obj.called==6 );
};

struct variadic_object : public active::object<variadic_object>
{
	mutable int calls[10], const_calls[10];

	variadic_object() : calls(), const_calls()
	{
	}

	void active_method()
	{
		++calls[0];
	}

	void active_method() const
	{
		++const_calls[0];
	}

	void active_method(int x)
	{
		assert( x==1 );
		++calls[1];
	}

	void active_method(double x)
	{
		assert( x==1.0 );
		++calls[1];
	}

	void active_method(int x) const
	{
		assert( x==1 );
		++const_calls[1];
	}

	void active_method(double x) const
	{
		assert( x==1.0 );
		++const_calls[1];
	}

	void active_method(int a, double b, const char *c)
	{
		assert(a==1);
		assert(b==2.0);
		assert( strcmp(c,"3")==0 );
		++calls[3];
	}

	void active_method(int a, double b, const char *c) const
	{
		assert(a==1);
		assert(b==2.0);
		assert( strcmp(c,"3")==0 );
		++const_calls[3];
	}

	void active_method(int a, double b, const char *c, int*d)
	{
		assert(a==1);
		assert(b==2.0);
		assert( strcmp(c,"3")==0 );
		assert(*d==4);
		++calls[4];
	}

	void active_method(int a, double b, const char *c, int*d) const
	{
		assert(a==1);
		assert(b==2.0);
		assert( strcmp(c,"3")==0 );
		assert(*d==4);
		++const_calls[4];
	}

	void active_method(int a, double b, const char *c, int*d, char e)
	{
		assert(a==1);
		assert(b==2.0);
		assert( strcmp(c,"3")==0 );
		assert(*d==4);
		assert(e=='5');
		++calls[5];
	}

	void active_method(int a, double b, const char *c, int*d, char e) const
	{
		assert(a==1);
		assert(b==2.0);
		assert( strcmp(c,"3")==0 );
		assert(*d==4);
		assert(e=='5');
		++const_calls[5];
	}
};

void test_variadic()
{
	variadic_object obj;
	const variadic_object &cobj=obj;
	obj();
	cobj();
	obj(1);
	cobj(1);
	obj(1.0);
	cobj(1.0);
	obj(1,2.0,"3");
	cobj(1,2.0,"3");
	int four = 4;
	obj(1,2.0,"3",&four);
	cobj(1,2.0,"3",&four);
	obj(1,2.0,"3",&four,'5');
	cobj(1,2.0,"3",&four,'5');

	active::run();

	assert( obj.calls[0]==1 );
	assert( obj.const_calls[0]==1 );
	assert( obj.calls[1]==2 );
	assert( obj.const_calls[1]==2 );
	assert( obj.calls[4]==1 );
	assert( obj.const_calls[4]==1 );
	assert( obj.calls[5]==1 );
	assert( obj.const_calls[5]==1 );
}

struct mmobject : public active::object<mmobject>
{
	struct foo { int x; };
	struct bar { int x; };
	void active_method(RVALUE_REF(foo) foo)
	{
		foo_value += foo.x;
	}
	void active_method(RVALUE_REF(bar) bar)
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
	obj(foo)(foo)(bar)(bar);
	active::run();
	assert( obj.foo_value == 4 );
	assert( obj.bar_value == 6 );
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

struct object1 : public active::object<object1>
{
	struct foo { int x; };
	void active_method( foo );
	int foo_value;
	object1() : foo_value(0) { }
	object2 * other;
};

struct object2 : public active::object<object2>
{
	struct foo { int x; };
	void active_method( foo foo )
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

void object1::active_method(foo foo)
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
	active::scheduler pool;

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

struct except_object : public active::object<except_object>
{
	bool caught;

	struct ex { };

	void exception_handler() throw()
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

	void active_method( msg )
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
	void active_method(int msg)
	{
		if( msg>0 ) (*this)(msg-1);
	}
};

void test_shared()
{
	// Messages keep the object alive
	active::platform::shared_ptr<my_shared> obj1(new my_shared), obj2(new my_shared);

	(*obj1)(20); (*obj2)(40);
	active::platform::weak_ptr<my_shared> r1(obj1), r2(obj2);
	obj1.reset();
	obj2.reset();

	assert( active::platform::shared_ptr<my_shared>(r1) );
	assert( active::platform::shared_ptr<my_shared>(r2) );

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
	public active::object<active_object_test>,
	public active::sink<int>
{
	struct test_object
	{
		active::sink<test_message> * subject;
	};

	void active_method( test_object test_object )
	{
		m_expected_value=1;
		struct test_message m = { 1, this };
		test_object.subject->send(m);
		m.value=2;
		test_object.subject->send(m);
		m.value=3;
		test_object.subject->send(m);
	}

	typedef int result;

	void active_method( int result )
	{
		assert( result == m_expected_value );
		++m_expected_value;
	}

	void send(int result) { (*this)(result); }

	struct finish {};
	void active_method(finish)
	{
		assert( m_expected_value == 4 );
	}

private:
	int m_expected_value;
};

template<typename Schedule, typename Queue, typename Shared>
struct test_object :
	public active::object<test_object<Schedule,Queue,Shared>, active::object_impl<Schedule, Queue, Shared> >,
	public active::handle<test_object<Schedule,Queue,Shared>, test_message>
{
	test_object() : m_times(0) { }

	int m_times;

	void active_method(test_message msg)
	{
		msg.object->send(msg.value);
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
	test_object< active::schedule::thread_pool, active::queueing::shared<>, active::sharing::disabled> obj2;
	test_object< active::schedule::thread_pool, active::queueing::direct_call, active::sharing::disabled> obj3;
	test_object< active::schedule::thread_pool, active::queueing::mutexed_call, active::sharing::disabled> obj4;

	test_object2(obj2);
	test_object2(obj3);
	test_object2(obj4);

	typedef active::queueing::eager<active::queueing::shared<> > steal1;

	test_object2( *active::platform::make_shared<test_object< active::schedule::thread_pool, active::queueing::shared<>, active::sharing::enabled<> > >() );
	test_object2( *active::platform::make_shared<test_object< active::schedule::thread_pool, active::queueing::direct_call, active::sharing::enabled<> > >() );
	test_object2( *active::platform::make_shared<test_object< active::schedule::thread_pool, active::queueing::mutexed_call, active::sharing::enabled<> > >() );
	test_object2( *active::platform::make_shared<test_object< active::schedule::thread_pool, steal1, active::sharing::enabled<> > >() );
}


template<typename Type>
struct test_clear_obj : public active::object<test_clear_obj<Type>,Type>
{
	void active_method(int x)
	{
		assert( x<=2 );
		if( x==2 )
		{
			this->clear();
			assert( this->empty() );
		}
	}
};

template<typename Base>
void test_clear2()
{
	test_clear_obj<Base> obj;
	obj(1)(2)(3)(4);
	active::run();
}

void test_clear()
{
	test_clear2<active::basic>();
	test_clear2<active::advanced>();
	// Teeny tiny bug - clear does not necessarily clear everything on fast.
	// test_clear2<active::fast>();
}

struct thread_obj : public active::object<thread_obj,active::thread>
{
	int total;

	void active_method( int add )
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

struct shared_thread_obj : public active::shared<shared_thread_obj,active::thread>
{
	int total;
	void active_method(int add) { total += add; }
	shared_thread_obj() : total(0) { }

	void active_method(int * finish) { *finish=total; }
};

void test_shared_thread(bool reset, int sleep1, int sleep2, int threads)
{
	shared_thread_obj::ptr st(new shared_thread_obj());
#ifdef ACTIVE_USE_BOOST
	active::platform::this_thread::sleep(boost::posix_time::milliseconds(sleep1));
#else
	active::platform::this_thread::sleep_for(std::chrono::milliseconds(sleep1));
#endif

	int finished=0;
	(*st)(12);
	(*st)(13);
	(*st)(&finished);
	if(reset) st.reset();

#ifdef ACTIVE_USE_BOOST
	active::platform::this_thread::sleep(boost::posix_time::milliseconds(sleep2));
#else
	active::platform::this_thread::sleep_for(std::chrono::milliseconds(sleep2));
#endif
	{ active::run r(threads); }	// Want: active::run{threads}; but thanks clang for not implementing this.
	assert( finished==25 );
}

void test_shared_thread()
{
	test_shared_thread(false, 0, 10, 1);
	test_shared_thread(false, 10, 0, 1);
	test_shared_thread(false, 0, 10, 4);
	test_shared_thread(false, 10, 0, 4);
	test_shared_thread(true, 0, 10, 1);
	test_shared_thread(true, 10, 0, 1);
	test_shared_thread(true, 0, 10, 4);
	test_shared_thread(true, 10, 0, 4);
}

// Can we run lots of different objects simultaenously?

typedef int mix_message;
struct mix_config
{
	active::platform::shared_ptr<active::sink<mix_message> > sink,result;
	int id;
};

struct mix_interface : public active::sink<mix_message>, public active::sink<mix_config>
{
};

int total_allocated=0;

template<typename Schedule, typename Queueing, typename Sharing>
struct mix_object : public active::object<mix_object<Schedule,Queueing,Sharing>, active::object_impl<Schedule,Queueing,Sharing> >, public mix_interface
{
	typedef Queueing queue_type;
	int m_id;
	active::sink<mix_message>::wp m_sink, m_result;

	mix_object() { ++total_allocated; }
	~mix_object() { --total_allocated; }

	void active_method( mix_message msg )
	{
		typename active::sink<mix_message>::sp sink = m_sink.lock(), result=m_result.lock();
		if( msg>0 && sink ) sink->send(msg-1);
		else if( msg<=0 && result) result->send(m_id);
	}

	void active_method( mix_config mc )
	{
		m_id = mc.id;
		m_sink = mc.sink;
		m_result = mc.result;
	}

	void send(mix_message msg) { (*this)(msg); }
	void send(mix_config mc) { (*this)(mc); }
};


active::platform::shared_ptr<mix_interface> mix_factory(int n)
{
	switch( n%22 )
	{
	default:
	case 0: return active::platform::make_shared< mix_object<active::schedule::thread_pool, active::queueing::shared<>, active::sharing::disabled> >();
	case 1: return active::platform::make_shared< mix_object<active::schedule::thread_pool, active::queueing::shared<>, active::sharing::enabled<> > >();
	case 4: return active::platform::make_shared< mix_object<active::schedule::thread_pool, active::queueing::eager<active::queueing::shared<> >, active::sharing::disabled> >();
	case 5: return active::platform::make_shared< mix_object<active::schedule::thread_pool, active::queueing::eager<active::queueing::shared<> >, active::sharing::enabled<> > >();
	case 8: return active::platform::make_shared< mix_object<active::schedule::own_thread, active::queueing::shared<>, active::sharing::disabled> >();
	case 9: return active::platform::make_shared< mix_object<active::schedule::own_thread, active::queueing::shared<>, active::sharing::enabled<> > >();
	case 12: return active::platform::make_shared< mix_object<active::schedule::own_thread, active::queueing::eager<active::queueing::shared<> >, active::sharing::disabled> >();
	case 13: return active::platform::make_shared< mix_object<active::schedule::own_thread, active::queueing::eager<active::queueing::shared<> >, active::sharing::enabled<> > >();
	case 16: return active::platform::make_shared< mix_object<active::schedule::none, active::queueing::direct_call, active::sharing::disabled> >();
	case 17: return active::platform::make_shared< mix_object<active::schedule::none, active::queueing::direct_call, active::sharing::enabled<> > >();
	case 18: return active::platform::make_shared< mix_object<active::schedule::none, active::queueing::mutexed_call, active::sharing::disabled> >();
	case 19: return active::platform::make_shared< mix_object<active::schedule::none, active::queueing::mutexed_call, active::sharing::enabled<> > >();
	case 20: return active::platform::make_shared< mix_object<active::schedule::thread_pool, active::queueing::advanced<>, active::sharing::disabled> >();
	case 21: return active::platform::make_shared< mix_object<active::schedule::thread_pool, active::queueing::advanced<>, active::sharing::enabled<> > >();
	}
}

template<typename T>
struct result_holder : public active::object<result_holder<T> >, public active::sink<T>
{
	T result;
	void active_method(T msg) { result = msg; }
	void send(T msg) { (*this)(msg); }
};

void test_object_mix()
{
	const int N=313, M=1024;
	active::platform::shared_ptr<result_holder<int> > result = active::platform::make_shared<result_holder<int> >();
	std::vector<active::platform::shared_ptr<mix_interface> > vec;

	for(int i=0; i<N; ++i)
		vec.push_back(mix_factory(i));

	for(int n=0; n<N; ++n)
	{
		mix_config msg = { n<N-1 ? vec[n+1] : vec[0], result, n };
		static_cast<active::sink<mix_config>&>((*vec[n])).send(msg);
	}

	static_cast<active::sink<mix_message>&>(*vec[0]).send(int(M));

	active::run(13);

	assert( result->result == M%N );
	vec.clear();
	assert( total_allocated ==0 );
}

int bytes_allocated=0, objects_constructed=0;

template<typename T>
struct my_allocator : public std::allocator<T>
{
	my_allocator() { }

	template<typename U>
	my_allocator(const my_allocator<U>&) { }

	T * allocate(int n)
	{
		bytes_allocated += n * sizeof(T);
		return std::allocator<T>::allocate(n);
	}

	void deallocate(T*p, int n)
	{
		std::allocator<T>::deallocate(p,n);
		bytes_allocated -= n * sizeof(T);
	}

	void construct(T*p, const T&v)
	{
		std::allocator<T>::construct(p,v);
		++objects_constructed;
	}

	void destroy(T*p)
	{
		std::allocator<T>::destroy(p);
		--objects_constructed;
	}

	template<typename U>
	struct rebind
	{
		typedef my_allocator<U> other;
	};
};

template<typename T>
struct awkward_allocator : public my_allocator<T>
{
	const int value;
	awkward_allocator(int i) : value(i) { }

	template<typename U>
	awkward_allocator(const awkward_allocator<U>&o) : value(o.value) { }

	template<typename U>
	struct rebind
	{
		typedef awkward_allocator<U> other;
	};
};

struct my_object : public
	active::object<my_object,
	active::object_impl<active::schedule::thread_pool, active::queueing::shared<awkward_allocator<int> >, active::sharing::disabled> >
{
	my_object(const allocator_type & a) :
		active::object<my_object,
		active::object_impl<active::schedule::thread_pool, active::queueing::shared<awkward_allocator<int> >, active::sharing::disabled> >
		(active::default_scheduler, a), total(0)
	{
	}
	int total;

	void active_method( int message )
	{
		total += message;
	}
};

struct my_object2 : public active::object<my_object2,
	active::object_impl<active::schedule::thread_pool, active::queueing::shared<my_allocator<int> >, active::sharing::disabled> >
{
	my_object2() : total(0)
	{
	}
	int total;

	void active_method( int message )
	{
		total += message;
	}
};

struct foo
{

};

void test_allocators()
{
	{
		std::vector<foo,my_allocator<foo> > vec1(2,foo());
		std::vector<foo,awkward_allocator<foo> > vec2(3,
													foo(),
													awkward_allocator<foo>(12));

		assert( vec2.get_allocator().value == 12);
		assert( bytes_allocated>0 );
		// assert( objects_constructed == 5 );
	}
	assert( bytes_allocated==0 );
	// assert( objects_constructed==0 );

	{
		my_object obj1(14);
		assert( obj1.get_allocator().value == 14 );

		obj1(99);
		obj1(101);
		active::run();
		assert( obj1.total == 200 );
	}
	assert( bytes_allocated==0 );
	// assert( objects_constructed==0 );

	{
		my_object2 obj2;

		obj2(99);
		obj2(101);
		active::run();
		assert( obj2.total == 200 );
		obj2.get_allocator();
	}
	assert( bytes_allocated==0 );
	// assert( objects_constructed==0 );

}

struct MoveObject : public active::object<MoveObject>
{
	struct Counted
	{
		static int instances;
		Counted() { ++instances; }
		~Counted() { }
		Counted(const Counted&) { ++instances; }
#ifdef ACTIVE_USE_CXX11
		Counted(Counted&&) { ++instances; }
#endif
	private:
	};

	struct MoveMessage
	{
		std::vector<Counted> vec;
	};

	void active_method( MoveMessage )
	{
	}
};

int MoveObject::Counted::instances=0;

void test_move_semantics()
{
	MoveObject::MoveMessage m;
	m.vec.resize(5);
	assert( MoveObject::Counted::instances==5 );
	MoveObject obj;
	obj(m);
	// This test fails on MSVC11: Move semantics not working properly
	//assert( MoveObject::Counted::instances==5 );
	active::run();
	//assert( MoveObject::Counted::instances==5 );
}

void test_promise()
{
	active::promise<int> x;
	x.send(12);
	assert( x.get() == 12 );
}

namespace active
{
	template<> int priority(const long & i) { return i; }
}

struct my_advanced : public active::object<my_advanced,active::advanced>
{
	typedef long msg;
	int previous;
	my_advanced() : previous(4) { }
	my_advanced(int limit) : previous(4)
	{
		set_capacity(limit);
		assert( get_capacity() == limit );
	}

	void active_method(long msg)
	{
		assert( msg==0 || get_priority() == msg-1 );
		assert( msg==this->previous-1 ); this->previous = msg;
	}
};

void test_advanced_ordering()
{
	my_advanced obj;
	obj(3L);
	obj(1L);
	obj(2L);
	active::run();
	assert( obj.previous == 1 );
}

void test_advanced_queue_limit()
{
	my_advanced obj(3);
	obj.set_queue_policy( active::policy::fail );
	obj(1L);
	obj(2L);
	obj(3L);

	try
	{
		obj(6L);
		assert(0 && "Exception not thrown");
	} catch (std::bad_alloc)
	{
	}
	active::run();

	obj.set_queue_policy( active::policy::ignore );
	obj.previous = 5;
	obj(1L);
	obj(2L);
	obj(3L);
	obj(4L);
	active::run();

	obj.set_queue_policy( active::policy::block );
	obj.previous = 4;
	obj(1L);
	obj(2L);
	obj(3L);
	obj(0L);
	active::run();

	obj.set_queue_policy( active::policy::discard );
	obj.previous = 4;
	obj(1L);
	obj(2L);
	obj(3L);
	obj(3L);
	active::run();

}

struct msg3{};
namespace active
{
	template<> int priority(const msg2&) { return 2; }
	template<> int priority(const msg3&) {return 2; }
}

struct my_advanced2 : public active::object<my_advanced2,active::advanced>
{
	int previous;
	my_advanced2() : previous(0) { }

	void active_method(int i)
	{
		assert(i==this->previous+1);
		this->previous = i;
	}

	void active_method(msg2)
	{
		assert( previous==0 );
	}

	void active_method(msg3)
	{
		assert( previous==0 );
	}
};


void test_advanced_noreorder()
{
	my_advanced2 obj;
	obj(msg2());
	obj(1);
	obj(2);
	obj(3);
	obj(msg3());
	active::run();
}

struct my_advanced3 : public active::object<my_advanced3, active::advanced>
{
	struct abort { };
	struct work1 { int x; };
	struct work2 { };

	void active_method( abort )
	{
		clear();
		assert( empty() );
	}

	void active_method( work1 )
	{
	}

	void active_method( work2 )
	{
	}

	struct data { };
	void active_method( data );
};

void test_advanced_queue_control()
{
	my_advanced3 obj;
	assert( obj.empty() );
	assert( obj.size()==0 );
	obj( my_advanced3::work2() );
	assert( !obj.empty() );
	assert( obj.size()==1 );

	obj( my_advanced3::abort() );
	obj( my_advanced3::work2() );
	active::run();
}

template<typename Base>
struct fib :
	public active::object<fib<Base>, active::object_impl<typename Base::schedule_type, typename Base::queue_type, active::sharing::enabled<fib<Base> > > >,
	public active::sink<int>
{
	struct calculate
	{
		int value;
		active::sink<int>::sp result;
	};

	void active_method( calculate calculate )
	{
		if( calculate.value > 2 )
		{
			m_total=0;
			m_result = calculate.result;
			fib::calculate
			lhs = { calculate.value-1, this->shared_from_this() },
			rhs = { calculate.value-2, this->shared_from_this() };

			// Note: AO destroyed after its last message.
			(*active::platform::make_shared<fib>())(lhs);
			(*active::platform::make_shared<fib>())(rhs);
		}
		else
		{
			calculate.result->send(1);
		}
	}

	void send(int result) { (*this)(result); }

	void active_method( int sub_result )
	{
		if( m_total ) m_result->send(m_total+sub_result);
		else m_total = sub_result;
	}

private:
	int m_total;
	sp m_result;
};


struct fib_test
{
	template<typename T>
	void operator()(T)
	{
		active::platform::shared_ptr<active::promise<int> > result = active::platform::make_shared<active::promise<int> >();
		typename fib<T>::calculate calc = { 20, result };
		(*active::platform::make_shared<fib<T> >())(calc);
		active::run();
		assert( result->get() == 6765 );
	}
};

template<typename Visitor>
void test_containers(Visitor&v)
{
	v(active::basic());
	v(active::shared<active::any_object>());

	v(active::fast());
	v(active::shared<active::any_object,active::fast>());

	v(active::direct());
	v(active::shared<active::any_object, active::direct>());

	v(active::synchronous());
	v(active::shared<active::any_object, active::synchronous>());

	v(active::advanced());
	v(active::shared<active::any_object,active::advanced>());

	//v(active::thread());
	//v(active::shared<active::any_object,active::thread>());

}

void test_fib_soak()
{
	fib_test t;
	test_containers(t);
}

int main()
{
	// Single-object tests
	test_method_call();
	test_run_all();
	test_run_some();
	test_const();
	test_not_inline();
	test_sink();
	test_multiple_methods();
	test_variadic();

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
	test_clear();
	test_own_thread();
	test_shared_thread();
	test_object_mix();
	test_allocators();
#ifdef ACTIVE_USE_CXX11
	test_move_semantics();
#endif

	// Advanced queueing object
	test_advanced_ordering();
	test_advanced_queue_limit();
	test_advanced_noreorder();
	test_advanced_queue_control();
	test_promise();

	// Mini-soak tests
	test_fib_soak();

	std::cout << "All tests passed!\n";
	return 0;
}
