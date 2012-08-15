#undef NDEBUG
#include <iostream>
#include <active/fifo.hpp>
#include <cassert>
#include <cstdlib>

void test1()
{
	active::fifo<int> f1;

	assert( f1.empty() );
	for(int i=0; i<100000; ++i)
	{
		assert( f1.size() == i );
		f1.push(i);
		assert( f1.front() == 0 );
		assert( !f1.empty() );
	}

	for(int i=0; i<100000; ++i)
	{
		assert( f1.front() == i );
		f1.pop();
	}
	assert( f1.size() == 0 );
	assert( f1.empty() );
}

void test2()
{
	active::fifo<int> f1;

	for(int i=0; i<100000; ++i)
	{
		assert( f1.size() == 0 );
		assert( f1.empty() );
		f1.push(i);
		assert( f1.front() == i );
		assert( !f1.empty() );
		assert( f1.size()==1 );
		f1.pop();
		assert( f1.empty() );
		assert( f1.size()==0 );
	}
}

void test3()
{
	active::fifo<int> f;

	int pop_count=0;
	int count=rand()%1000;
	bool pop=false;
	const int max=1000000;

	for(int i=0; i<max; ++i)
	{
		if( pop )
		{
			while( count-->0 && i>pop_count )
			{
				assert( f.front() == pop_count );
				++pop_count;
				f.pop();
			}
			pop=false;
			count = rand()%900;	// Bias towards 0
		}

		assert( f.empty() == (i==pop_count) );
		assert( f.size() == i-pop_count );

		if( --count<=0 )
		{
			count = rand()%1000;
			pop=true;
		}

		f.push(i);
	}

	while( pop_count < max )
	{
		assert( f.front() == pop_count );
		f.pop();
		++pop_count;
	}

	assert( f.empty() );
	assert( f.size()==0 );
}

void test_allocator()
{
}

struct A
{
	virtual ~A()
	{
	}
	virtual int value() const=0;
};

struct B : public A
{
	B(int v) : m_value(v) { }

	int value() const
	{
		return m_value;
	}

	const int m_value;
};

static int allocations=0;

struct C : public A
{
	C(int v) : m_value(v) { ++allocations; }
	C(const C&c) : m_value(c.m_value) { ++allocations; }
	~C() { --allocations; }

	int value() const
	{
		return -m_value;
	}

	const int m_value;
};

void test_polymorphism()
{
	active::fifo<A> f;
	f.push(B(1));
	f.push(C(-2));

	assert( f.front().value()==1 );
	f.pop();
	assert( f.front().value()==2 );
	f.pop();

	for(int i=0; i<1000; ++i)
	{
		if( i%3==0 )
			f.push(C(-i));
		else
			f.push(B(i));
	}

	for(int i=0; i<1000; ++i)
	{
		assert( f.front().value() == i);
		f.pop();
	}

	assert( f.empty() );
	assert( allocations==0 );
}

int main()
{
	test1();
	test2();
	test3();
	test_allocator();
	test_polymorphism();
}
