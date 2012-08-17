#include <active/object.hpp>
#include <active/scheduler.hpp>
#include <active/thread.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>

#include <iostream>

#define ACTIVE_OBJECT_CONDITION 0	// !! Slow


active::scheduler active::default_scheduler;

active::scheduler::scheduler() : m_head(nullptr), m_busy_count(0)
{
}

active::any_object::~any_object()
{
}

// Used by an active object to signal that there are messages to process.
void active::scheduler::activate(ObjectPtr p) throw()
{
	//platform::lock_guard<platform::mutex> lock(m_mutex);
	//++m_busy_count;

#if 1
	ObjectPtr head;
	do
	{
		head = m_head;
		p->m_next = head;	// ABA here
	}
	while( !m_head.compare_exchange_weak(head, p, std::memory_order_relaxed) );
#endif

	//p->m_next = m_head;
	//m_head = p;

#if ACTIVE_OBJECT_CONDITION
	m_ready.notify_one();
#endif
}

// Runs until there are no more messages in the entire pool.
// Returns false if no more items.
bool active::scheduler::run_managed() throw()
{
	++m_busy_count;

	// This works:

	while( ObjectPtr p = m_head.exchange(nullptr) )
	{
		for(ObjectPtr i=p; i;)
		{
			ObjectPtr j=i;
			i=i->m_next;
			j->run_some();
		}
	}

	// Can be non-zero if the queues are empty, but other threads are processing.
	// the result of processing could be to add more signalled objects.
	// return m_busy_count!=0;
	// return (m_busy_count-=count)!=0;
	return 0!=--m_busy_count;
}

bool active::scheduler::run_one()
{
	++m_busy_count;
	if( ObjectPtr p = m_head.exchange(nullptr) )
	{
		for(ObjectPtr i=p; i;)
		{
			ObjectPtr j=i;
			i=i->m_next;
			j->run_some();
		}
	}

	return 0!=--m_busy_count;
}

active::run::run(int num_threads, scheduler & sched) :
	m_scheduler(sched)
{
	if( num_threads<1 ) num_threads=4;
	// m_threads.resize(num_threads);
	m_scheduler.start_work();	// Prevent threads from exiting immediately
	for( int t=0; t<num_threads; ++t )
#ifdef ACTIVE_USE_BOOST
		m_threads.add_thread(new platform::thread( platform::bind(&scheduler::run, &sched) ) );
#else
		m_threads.push_back(platform::thread( platform::bind(&scheduler::run, &sched) ) );
#endif
}

active::run::~run()
{
	m_scheduler.stop_work();
#ifdef ACTIVE_USE_BOOST
	m_threads.join_all();
#else
	for(threads::iterator t=m_threads.begin(); t!=m_threads.end(); ++t)
		t->join();
#endif
}

void active::scheduler::run()
{
	while( run_managed() )
	{
		platform::unique_lock<platform::mutex> lock(m_mutex);
#if ACTIVE_OBJECT_CONDITION
		m_ready.wait(lock);
#else
#ifdef ACTIVE_USE_BOOST
		m_ready.timed_wait(lock, boost::posix_time::milliseconds(1));
#else
		m_ready.wait_for(lock, std::chrono::milliseconds(1) );
#endif
#endif
	}
	platform::unique_lock<platform::mutex> lock(m_mutex);	// Needed on VS11
	m_ready.notify_all();
}

void active::scheduler::run_in_thread()
{
	run();
}

void active::any_object::exception_handler() throw()
{
	// !! Bug: std::cerr is not threadsafe
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

void active::scheduler::start_work() throw()
{
	//platform::lock_guard<platform::mutex> lock(m_mutex);
	++m_busy_count;
}

void active::scheduler::stop_work() throw()
{
	//platform::lock_guard<platform::mutex> lock(m_mutex);
	if( 0 == --m_busy_count )
	{
		m_ready.notify_one();
	}
}

active::schedule::thread_pool::thread_pool(type & p) : m_pool(&p)
{
}

void active::schedule::thread_pool::set_scheduler(type&p)
{
	m_pool = &p;
}

void active::schedule::thread_pool::activate(const platform::shared_ptr<any_object> & sp)
{
	if(m_pool) m_pool->activate(sp.get());
}

void active::schedule::thread_pool::activate(any_object * obj)
{
	if(m_pool) m_pool->activate(obj);
}

void active::schedule::own_thread::activate(any_object * obj)
{
	platform::lock_guard<platform::mutex> lock(m_mutex);
	m_object = obj;
	if( m_pool ) m_pool->start_work();
	m_ready.notify_one();
}

void active::schedule::own_thread::activate(const platform::shared_ptr<any_object> & sp)
{
	platform::lock_guard<platform::mutex> lock(m_mutex);
	m_shared = sp;
	m_object = sp.get();
	if( m_pool ) m_pool->start_work();
	m_ready.notify_one();
}

void active::schedule::own_thread::thread_fn()
{
	platform::unique_lock<platform::mutex> lock(m_mutex);
	while( !m_shutdown )
	{
		if( m_object )
		{
			any_object * obj = m_object;
			m_object = nullptr;
			lock.unlock();
			obj->run();
			lock.lock();
			if(m_pool) m_pool->stop_work();
			if( m_shared )
			{
				platform::weak_ptr<any_object> wp(m_shared);
				platform::shared_ptr<any_object> shared;
				shared.swap(m_shared);
				lock.unlock();
				shared.reset();
				if( wp.expired() ) return;	// Object destroyed
				lock.lock();
			}
		}
		if( !m_object && !m_shutdown )
			m_ready.wait(lock);
	}
}

active::schedule::own_thread::own_thread(const own_thread&other) :
	m_pool(other.m_pool),
	m_shutdown(false),
	m_object(nullptr),
	m_thread( platform::bind( &own_thread::thread_fn, this ) )
{
}

active::schedule::own_thread::own_thread(type & p) :
	m_pool(&p),
	m_shutdown(false),
	m_object(nullptr),
	m_thread( platform::bind( &own_thread::thread_fn, this ) )
{
}

active::schedule::own_thread::~own_thread()
{
	if( platform::this_thread::get_id() == m_thread.get_id())
	{
		m_thread.detach();	// Destroyed from within thread
	}
	else
	{
		{
			platform::lock_guard<platform::mutex> lock(m_mutex);
			m_shutdown = true;
			m_ready.notify_one();
		}
		m_thread.join();	// Destroyed outside of thread
	}
}

active::scheduler & active::schedule::own_thread::get_scheduler() const
{
	return *m_pool;
}

active::queueing::direct_call::direct_call(const allocator_type&)
{
}

bool active::queueing::direct_call::run_some(any_object * o, int n) throw()
{
	return false;
}

bool active::queueing::direct_call::empty() const
{
	return true;
}

bool active::queueing::direct_call::mutexed_empty() const
{
	return true;
}

active::queueing::mutexed_call::mutexed_call(const allocator_type&)
{
}

active::queueing::mutexed_call::mutexed_call(const mutexed_call&)
{
}

bool active::queueing::mutexed_call::run_some(any_object * o, int n) throw()
{
	return false;
}

bool active::queueing::mutexed_call::empty() const
{
	return true;
}

bool active::queueing::mutexed_call::mutexed_empty() const
{
	return true;
}

void active::queueing::direct_call::clear()
{
}

void active::queueing::mutexed_call::clear()
{
}

bool active::idle(scheduler & sched) throw()
{
	return sched.run_one();
}
