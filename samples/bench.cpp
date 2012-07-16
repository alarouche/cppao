#define BOOST_DISABLE_ASSERTS 1

#include <active/object.hpp>
#include <active/thread.hpp>
#include <active/advanced.hpp>
#include <active/shared.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>
#include <active/fast.hpp>

#include <iostream>
#include <ctime>


template<typename Object>
struct Thread : public active::object<Thread<Object>,Object>
{
	int id;
	Thread * next;

	void active_method(int token)
	{
		if( token>0 )
			(*next)(token-1);
	}
};


template<typename Object>
void bench_object(Object, int N, bool output)
{
	typedef Thread<Object> type;
	active::platform::shared_ptr<type> thread[503];

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

	clock_t clk1 = clock();
	(*thread[0])(N);
	active::run();

	if( output )
	{
		double duration = (clock()-clk1)/double(CLOCKS_PER_SEC);
		double mps = double(N) / duration;
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
	int RECURSIVE=10000;
	std::cout << "1:  direct              ";
	bench_object( active::direct(), RECURSIVE );

	std::cout << "2:  shared<direct>      ";
	bench_object( active::direct(), RECURSIVE );

	std::cout << "3:  synchronous         ";
	bench_object( active::synchronous(), RECURSIVE );

	std::cout << "4:  shared<synchronous> ";
	bench_object( active::shared<active::any_object,active::synchronous>::type(), RECURSIVE );

	std::cout << "5:  fast                ";
	bench_object( active::fast(), N );

	std::cout << "6:  shared<fast>        ";
	bench_object( active::shared<active::any_object, active::fast>::type(), N );

	std::cout << "7:  object              ";
	bench_object( active::basic(), N );

	std::cout << "8:  shared<object>      ";
	bench_object( active::shared<active::any_object>::type(), N );

	std::cout << "9:  advanced            ";
	bench_object( active::advanced(), N );

	std::cout << "10: shared<advanced>    ";
	bench_object( active::shared<active::any_object,active::advanced>::type(), N );

	std::cout << "11: thread              ";
	bench_object( active::thread(), N );

	std::cout << "12: shared<thread>      ";
	bench_object( active::shared<active::any_object,active::thread>::type(), N );
}

