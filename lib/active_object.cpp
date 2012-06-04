#include "active_object.hpp"
#include <thread>
#include <iostream>


active::pool active::default_pool;

active::pool::pool() : m_busy_count(0)
{
}

active::any_object::~any_object()
{
}


// Used by an active object to signal that there are messages to process.
void active::pool::signal(ObjectPtr p)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_signalled1.push_back(p);
	++m_busy_count;
#if ACTIVE_OBJECT_CONDITION
	m_ready.notify_one();
#endif
}

// Used by an active object to signal that there are messages to process.
void active::pool::signal(const SharedPtr & p)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_signalled2.push_back(p);
	++m_busy_count;
#if ACTIVE_OBJECT_CONDITION
	m_ready.notify_one();
#endif
}

// Runs until there are no more messages in the entire pool.
// Returns false if no more items.
bool active::pool::run()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	while( !m_signalled1.empty() || !m_signalled2.empty() )
	{ 
		if( !m_signalled1.empty() )
		{
			ObjectPtr p = m_signalled1.front();
			m_signalled1.pop_front();
			lock.unlock();
			p->run_some();
			lock.lock();
			--m_busy_count;
		}
		
		if( !m_signalled2.empty() )
		{
			SharedPtr p = m_signalled2.front();
			m_signalled2.pop_front();
			lock.unlock();
			p->run_some();
			lock.lock();
			--m_busy_count;
		}
	}
	
	return m_busy_count!=0;
}
		
// Runs all objects in a thread pool, until there are no more messages.
void active::pool::run(int threads)
{
	std::deque<std::thread> tp(threads);	// ?? Why not vector ??
	
	for(int i=0; i<threads; ++i)
		tp[i] = std::thread( std::bind(thread_fn, this) ); 
	
	for(int i=0; i<threads; ++i)
		tp[i].join();
	
	if( m_exception )
		std::rethrow_exception( m_exception );
}	

// Like run(), but when starved, waits for a little while instead of exiting.
// Note we could use a condition variable here, but this is slow when you have a 
// silly number of threads like 503...
void active::pool::run_in_thread()
{
	try 
	{
		while( run() )
		{
			std::unique_lock<std::mutex> lock(m_mutex);
#if ACTIVE_OBJECT_CONDITION
			m_ready.wait(lock);
#else
			m_ready.wait_for(lock, std::chrono::milliseconds(50) );
#endif
		}
		std::unique_lock<std::mutex> lock(m_mutex);	// Needed on VS11
		m_ready.notify_all();
	}
	catch( ... )
	{
		// Basically something like bad_alloc.
		m_exception = std::current_exception();
	}
}

void active::pool::thread_fn(pool * p)
{
	p->run_in_thread();
}


void active::any_object::exception_handler()
{
	try
	{
		throw;
	}
	catch( std::exception & ex )
	{
		std::cerr << "Unprocessed exception during message processing: " << ex.what() << std::endl;
	}
	catch( ... )
	{
		std::cerr << "Unprocessed exception during message processing" << std::endl;
	}
}

		
void active::run()
{
	default_pool.run();
}

void active::run(int threads)
{
	default_pool.run(threads);
}

