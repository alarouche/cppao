
#include <active/object.hpp>
#include <active/thread.hpp>
#include <active/separate.hpp>
#include <active/advanced.hpp>
#include <active/shared.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>
#include <active/fast.hpp>

#include <chrono>
#include <iostream>


template<typename Schedule, typename Queue, typename Shared>
struct Thread : public active::object_impl<Schedule, Queue, Shared>
{
	typedef Queue queue_type;
	typedef int token;

	int id;
	Thread * next;

	ACTIVE_TEMPLATE(token)
	{
		if( token>0 )
			(*next)(token-1);
	}
};

template<typename Schedule, typename Queue,typename Shared>
void bench_object(Schedule,Queue,Shared, int N, bool output=true)
{
	typedef Thread<Schedule,Queue,Shared> type;
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
	(*thread[0])(N);
	active::run();

	if( output )
	{
		std::chrono::high_resolution_clock::duration dur = std::chrono::high_resolution_clock::now()-clk1;
		double mps = double(N) * double(std::chrono::high_resolution_clock::duration::period::den) / dur.count();
		double mmps = mps/1000000.0;
		std::cout << mmps << " million messages/second\n";
	}
}


int main(int argc, char**argv)
{
	int N = argc>1 ? atoi(argv[1]) : 1000;

	if( argc>2 )
	{
		// Churn up memory a little
		bench_object( active::schedule::none(active::default_scheduler), active::queueing::direct_call(), active::sharing::disabled(), 10000, false );
		bench_object( active::schedule::none(active::default_scheduler), active::queueing::direct_call(), active::sharing::enabled<>(), 10000, false );

		// Now run tests
		bench_object( active::schedule::none(active::default_scheduler), active::queueing::direct_call(), active::sharing::enabled<>(), 10000, false );
		std::cout << "1: ";
		bench_object( active::schedule::none(active::default_scheduler), active::queueing::direct_call(), active::sharing::disabled(), 10000 );
		bench_object( active::schedule::none(active::default_scheduler), active::queueing::direct_call(), active::sharing::enabled<>(), 10000, false );

		std::cout << "2: ";
		bench_object( active::schedule::none(active::default_scheduler), active::queueing::mutexed_call(), active::sharing::enabled<>(), 10000 );

		std::cout << "3: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::steal<active::queueing::shared<>>(), active::sharing::disabled(), N );

		std::cout << "4: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::steal<active::queueing::shared<>>(), active::sharing::enabled<>(), N );

		std::cout << "5: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::steal<active::queueing::separate<>>(), active::sharing::disabled(), N );
		std::cout << "6: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::steal<active::queueing::separate<>>(), active::sharing::enabled<>(), N );

		std::cout << "7: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::shared<>(), active::sharing::disabled(), N );
		std::cout << "8: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::shared<>(), active::sharing::enabled<>(), N );

		std::cout << "9: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::separate<>(), active::sharing::disabled(), N );
		std::cout << "10: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::separate<>(), active::sharing::enabled<>(), N );

		std::cout << "!!: ";
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::advanced<>(), active::sharing::disabled(), N );

		std::cout << "11: ";
		std::this_thread::sleep_for(std::chrono::seconds(1));
		bench_object( active::schedule::own_thread(active::default_scheduler), active::queueing::separate<>(), active::sharing::disabled(), N );
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "11: ";
		bench_object( active::schedule::own_thread(active::default_scheduler), active::queueing::separate<>(), active::sharing::disabled(), N );

		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "12: ";
		bench_object( active::schedule::own_thread(active::default_scheduler), active::queueing::separate<>(), active::sharing::enabled<>(), N );

		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "12: ";
		bench_object( active::schedule::own_thread(active::default_scheduler), active::queueing::separate<>(), active::sharing::enabled<>(), N );
	}
	else
	{
		bench_object( active::schedule::thread_pool(active::default_scheduler), active::queueing::steal<active::queueing::shared<>>(), active::sharing::disabled(), N, false );
	}

}
