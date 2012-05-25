/* This program attempts to "benchmark" active objects
 * using the basic thread ring at http://shootout.alioth.debian.org/u32/performance.php?test=threadring#about
 * 
 *  The result is very quick on my machine (8.8s for 50000000) which actually beats the best C++ 
 *  benchmark.
 */

#include <active_object.hpp>

#include <iostream>

class Thread : public active::object
{
public:
	int id;
	Thread * next;
	
	typedef int token;
	
	ACTIVE_METHOD( token )
	{
		if( token>0 )
			(*next)(token-1);
		else
			std::cout << id << std::endl;
	}
};

int main(int argc, char**argv)
{
	int N = argc>1 ? atoi(argv[1]) : 1000;
	
	active::pool pool;
	Thread thread[503];
	
	for( int t=0; t<503; ++t )
	{
		thread[t].set_pool(pool);
		thread[t].id=t+1;
		thread[t].next = thread+t+1;
	}
	
	thread[502].next = thread;
	thread[0](N);
	
	// Run 503 OS threads, servicing 503 active objects.
	// Note there is not necessarily a 1-1 correspondence between
	// active objects and OS threads due to a kind of work stealing.
	pool.run(503);
	return 0;
}