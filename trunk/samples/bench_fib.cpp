#include <active/object.hpp>
#include <active/promise.hpp>
#include <active/advanced.hpp>
#include <active/fast.hpp>
#include <active/synchronous.hpp>
#include <iostream>

struct bench_messages
{
	bench_messages(const char * str, int msgcount) : m_msgcount(msgcount)
	{
		std::cout << str << ": ";
		m_start = clock();
	}

	~bench_messages()
	{
		double duration = (clock()-m_start)/double(CLOCKS_PER_SEC);
		double mps = double(m_msgcount) / duration;
		if( mps > 1000000.0 )
			std::cout << (mps/1000000.0) << " million messages/second" << std::endl;
		else
			std::cout << ( mps/1000.0) << " thousand messages/second" << std::endl;
	}

private:
	const int m_msgcount;
	clock_t m_start;
};

int messages(int n)
{
	return n>2 ? 3+messages(n-1)+messages(n-2) : 1;
}

template<typename Type>
struct fib : public active::object<fib<Type>, Type>, public active::handle<fib<Type>, int>
{
	active::platform::shared_ptr<fib> fib1, fib2;

	fib(int n)
	{
		if( n>2 )
		{
			fib1.reset(new fib(n-1));
			fib2.reset(new fib(n-2));
		}
	}

	void active_method(int v)
	{
		if(m_value) m_result->send(m_value+v);
		else m_value=v;
	}

	void active_method(int n, active::sink<int> * result)
	{
		if( n>2 )
		{
			m_value=0;
			m_result = result;
			(*fib1)(n-1, this);
			(*fib2)(n-2, this);
		}
		else
		{
			result->send(1);
		}
	}

private:
	active::sink<int> * m_result;
	int m_value;
};


template<typename T>
void bench(const char * msg, int n=30)
{
	active::platform::shared_ptr<fib<T> > f( new fib<T>(n) );
	active::promise<int> result;
	bench_messages b(msg, messages(n));
	(*f)(n,&result);
	active::run();
}

int main(int argc, char**argv)
{
	int n=argc>1 ? atoi(argv[1]) : 30;
	bench<active::direct>("active::direct", n);
	bench<active::synchronous>("active::synchronous", n);
	bench<active::fast>("active::fast", n);
	bench<active::basic>("active::basic", n);
	bench<active::basic>("active::advanced", n);
}
