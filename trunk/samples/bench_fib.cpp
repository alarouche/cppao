#include <active/object.hpp>
#include <active/promise.hpp>
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

template<int N, typename Type>
struct fib : public active::object<fib<N,Type>, Type>, public active::sink<int>
{
	fib<N-1,Type> fib1;
	fib<N-2,Type> fib2;
		
	void active_method(int v)
	{
		if(m_value) m_result->send(m_value+v);
		else m_value=v;
	}
	
	void active_method(int n, active::sink<int> * result)
	{
		m_value=0;
		m_result = result;
		fib1(n-1, this);
		fib2(n-2, this);
	}
	
	static const int Messages = 3+fib<N-1,Type>::Messages+fib<N-2,Type>::Messages;
private:
	active::sink<int> * m_result;
	int m_value;
};

template<typename Type>
struct fib<2,Type> : public active::object<fib<2,Type>, Type>
{
	void active_method(int i, active::sink<int> * result)
	{
		result->send(1);
	}
	
	static const int Messages=1;
};

template<typename Type>
struct fib<1,Type> : public active::object<fib<1,Type>, Type>
{
	void active_method(int i, active::sink<int> * result)
	{
		result->send(1);
	}
	
	static const int Messages=1;
};



template<typename T>
void bench(const char * msg)
{
	std::auto_ptr<fib<30,T> > f( new fib<30,T>() );
	active::promise<int> result;
	bench_messages b(msg, fib<30,T>::Messages);
	(*f)(30,&result);
	active::run();
}

int main()
{
	bench<active::direct>("active::direct");
	bench<active::basic>("active::basic");
}
