#define BOOST_DISABLE_ASSERTS 1

#include <active/object.hpp>
#include <active/thread.hpp>
#include <active/advanced.hpp>
#include <active/shared.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>
#include <active/fast.hpp>

#include <chrono>
#include <iostream>


template<typename Object>
struct Thread : public Object
{
	int id;
	Thread * next;

	void send(int token)
	{
		this->active_fn( [=]
		{
			if( token>0 ) this->next->send(token-1);
		} );
	}
};

// template<typename Schedule, typename Queue,typename Shared>
template<typename Object>
void bench_object(Object, int N, bool output)
{
	typedef Thread<Object> type;
	std::shared_ptr<type> thread[503];

	for( int t=0; t<503; ++t )
	{
		thread[t].reset(new type());
		thread[t]->id=t+1;
	}

	for( int t=0; t<502; ++t )
	{
		thread[t]->next = thread[t+1].get();
	}
	thread[502]->next = thread[0].get();

	std::chrono::high_resolution_clock::time_point clk1=std::chrono::high_resolution_clock::now();
	thread[0]->send(N);
	active::run();

	if( output )
	{
		std::chrono::high_resolution_clock::duration dur = std::chrono::high_resolution_clock::now()-clk1;
		double mps = double(N) * double(std::chrono::high_resolution_clock::duration::period::den) / dur.count();
		double mmps = mps/1000000.0;
		std::cout << mmps << " million messages/second\n";
	}
}

template<typename Object>
void bench_object(Object obj, int N)
{
	bench_object(obj,N,false);
	bench_object(obj,N,true);
}

int main(int argc, char**argv)
{
	int N = argc>1 ? atoi(argv[1]) : 100000;

	std::cout << "1:  direct              ";
	bench_object( active::direct(), 20000 );

	std::cout << "2:  shared<direct>      ";
	bench_object( active::direct(), 20000 );

	std::cout << "3:  synchronous         ";
	bench_object( active::synchronous(), 20000 );

	std::cout << "4:  shared<synchronous> ";
	bench_object( active::shared<active::any_object,active::synchronous>(), 20000 );

	std::cout << "5:  fast                ";
	bench_object( active::fast(), N );

	std::cout << "6:  shared<fast>        ";
	bench_object( active::shared<active::any_object, active::fast>(), N );

	std::cout << "7:  object              ";
	bench_object( active::basic(), N );

	std::cout << "8:  shared<object>      ";
	bench_object( active::shared<active::any_object>(), N );

	std::cout << "9: advanced            ";
	bench_object( active::advanced(), N );

	std::cout << "10: shared<advanced>    ";
	bench_object( active::shared<active::any_object,active::advanced>(), N );

	std::cout << "11: thread              ";
	bench_object( active::thread(), N );

	std::cout << "12: shared<thread>      ";
	bench_object( active::shared<active::any_object,active::thread>(), N );
}

