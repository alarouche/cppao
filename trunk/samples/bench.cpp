
#include <active_object.hpp>
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
		else
			std::cout << id << " ";		
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
		std::cout << (dur.count()/double(std::chrono::high_resolution_clock::duration::period::den)) << " seconds, ";
		double mps = double(N) * double(std::chrono::high_resolution_clock::duration::period::den) / dur.count();
		std::cout << mps << " messages/second\n";
	}
}


int main(int argc, char**argv)
{
	int N = argc>1 ? atoi(argv[1]) : 1000;

	if( argc>2 )
	{
		// Churn up memory a little
		bench_object( active::schedule::none(active::default_pool), active::queueing::direct_call(), active::sharing::disabled(), 10000, false );
		bench_object( active::schedule::none(active::default_pool), active::queueing::direct_call(), active::sharing::enabled<>(), 10000, false );

		// Now run tests
        std::cout << "active::direct (shared): ";
		bench_object( active::schedule::none(active::default_pool), active::queueing::direct_call(), active::sharing::enabled<>(), 10000 );
        std::cout << "active::direct: ";
		bench_object( active::schedule::none(active::default_pool), active::queueing::direct_call(), active::sharing::disabled(), 10000 );
        std::cout << "active::direct (shared): ";
		bench_object( active::schedule::none(active::default_pool), active::queueing::direct_call(), active::sharing::enabled<>(), 10000 );

        std::cout << "active::synchronous: ";
        bench_object( active::schedule::none(active::default_pool), active::queueing::mutexed_call(), active::sharing::enabled<>(), 10000 );

		//bench_object( active::schedule::none(active::default_pool), active::queueing::mutexed_call(), active::sharing::disabled(), 10000 );
		//bench_object( active::schedule::none(active::default_pool), active::queueing::mutexed_call(), active::sharing::enabled<>(), 10000 );
		
		//bench_object( active::schedule::none(active::default_pool), active::queueing::try_lock(), active::sharing::disabled(), 10000 );
		//bench_object( active::schedule::none(active::default_pool), active::queueing::try_lock(), active::sharing::enabled<>(), 10000 );

		std::cout << "active::fast: ";
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::steal<active::queueing::shared>(), active::sharing::disabled(), N );

		std::cout << "active::steal<shared>  shared: ";		
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::steal<active::queueing::shared>(), active::sharing::enabled<>(), N );

		std::cout << "active::steal<separate>: ";		
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::steal<active::queueing::separate>(), active::sharing::disabled(), N );
		std::cout << "active::steal<separate> shared: ";		
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::steal<active::queueing::separate>(), active::sharing::enabled<>(), N );
		
		std::cout << "active::object: ";
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::shared(), active::sharing::disabled(), N );
		std::cout << "active::shared<>: ";
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::shared(), active::sharing::enabled<>(), N );
		
		std::cout << "active::separate: ";
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::separate(), active::sharing::disabled(), N );
		std::cout << "active::separate shared: ";
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::separate(), active::sharing::enabled<>(), N );

        std::cout << "active::thread: ";
        std::this_thread::sleep_for(std::chrono::seconds(1));
        bench_object( active::schedule::own_thread(active::default_pool), active::queueing::separate(), active::sharing::disabled(), N );
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "active::thread: ";
        bench_object( active::schedule::own_thread(active::default_pool), active::queueing::separate(), active::sharing::disabled(), N );

        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "active::shared_thread<>: ";
		bench_object( active::schedule::own_thread(active::default_pool), active::queueing::separate(), active::sharing::enabled<>(), N );
		
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "active::shared_thread<>: ";
        bench_object( active::schedule::own_thread(active::default_pool), active::queueing::separate(), active::sharing::enabled<>(), N );
    }
	else
	{
		bench_object( active::schedule::thread_pool(active::default_pool), active::queueing::steal<active::queueing::shared>(), active::sharing::disabled(), N, false );
	}

}
