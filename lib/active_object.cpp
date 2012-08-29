#include <active/object.hpp>
#include <active/scheduler.hpp>
#include <active/thread.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>
#include <cstdio>

// Various tweaks which can affect performance:

// Whether to use a condition variable to signal to waiting worker threads.
// Set to 0 because this gives a performance penalty.
#define ACTIVE_OBJECT_CONDITION 0

// Whether to use C++11 atomics for managing the queue of activated objects.
// This is much faster.
#define ATOMIC_QUEUE 1

// Whether a worker thread takes one or all activated objects from the queue.
// There is a performance penalty for doing this, because sometimes adding threads
// actually adds contention on the queue, but in the general case should be better.
// If this is set to 0, then there could be a deadlock because a waiting thread
// may be blocked on an object later in the list.
#define QUEUE_PUT_BACK 1

// Our global variable, the scheduler.
// I generally hate global variables, but actually this one makes sense since
// it appears to offer some background facility such as a memory allocator
// or a thread. Use of this is optional.
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
#if defined(ACTIVE_USE_CXX11) && ATOMIC_QUEUE
	// Use atomics
	
	// This loop implements a lock-free push onto a singly-linked list
	ObjectPtr head;
	do
	{
		head = m_head;
		p->m_next = head;	// ABA here
	}
	while( !m_head.compare_exchange_weak(head, p, std::memory_order_relaxed) );
#else
	// Not using atomics
	platform::lock_guard<platform::mutex> lock(m_mutex);
	p->m_next = m_head;
	m_head = p;
#endif

#if ACTIVE_OBJECT_CONDITION
	// ?? Lock guard needed here
	m_ready.notify_one();
#endif
}

// Run one item, return true if there are more items.
bool active::scheduler::locked_run_one()
{
#if defined(ACTIVE_USE_CXX11) && ATOMIC_QUEUE	
	// Implement a lock-free pop from a singly-linked list.
	// The traditional solution suffers from the ABA problem which occurs
	// quite regularly in this case.
	// There are no ordering guarantees.
	// I made this algorithm up so beware.
	// The basic strategy is to pop the whole list from m_head into p,
	// Then push the remainder (r=p->m_next) back onto m_head.
	// This requires two CAS on average, but if there is a collision
	// with another thread, then it could be more.
	// The loop is designed to detect if anything got pushed onto m_head
	// whilst we attempt to push back the remainder r. If this is the case,
	// then remove m_head and join it to r, and try again.
	
	if( ObjectPtr p = m_head.exchange(nullptr) )
	{
#if QUEUE_PUT_BACK
		// Process one item at a time.
		
		ObjectPtr r = p->m_next;	// Push back the list r onto m_head
		while(r && (r=m_head.exchange(r)))	// There is a list to push back
		{
			if(ObjectPtr q = m_head.exchange(nullptr))
			{
				// Oops, something managed to push something onto m_head
				// in the meantime.
				
				// We now end up with two lists, q and r which we want to
				// push back onto r.
				ObjectPtr i;
#if 0
				// Splice r onto the end of q
				for(i=q; i->m_next; i=i->m_next, ++len)
					;
				i->m_next = r;
				r=q;
#else
				// r is short, q is long, so splice q onto the end of r.
				for(i=r; i->m_next; i=i->m_next)
					;
				i->m_next = q;
#endif
			}
		}
		p->run_some();
#else
		// Process the entire queue
		for(ObjectPtr i=p; i;)
		{
			ObjectPtr j=i;
			i=i->m_next;
			j->run_some();
		}
#endif
		return true;
	}
#else
	if( m_head )
	{
		ObjectPtr p = m_head;
		m_head=m_head->m_next;
		m_mutex.unlock();
		p->run_some();
		m_mutex.lock();
		return true;
	}
#endif	
	return false;
}

// Runs until there are no more messages in the entire pool.
// Returns false if no more items.
bool active::scheduler::run_managed() throw()
{
#if !defined(ACTIVE_USE_CXX11) || !ATOMIC_QUEUE
	platform::unique_lock<platform::mutex> lock(m_mutex);
#endif
	++m_busy_count;
	while( locked_run_one() )
		;

	// Can be non-zero if the queues are empty, but other threads are processing.
	// the result of processing could be to add more signalled objects.
	return 0!=--m_busy_count;
}

bool active::scheduler::run_one()
{
#if !defined(ACTIVE_USE_CXX11) || !ATOMIC_QUEUE
	platform::unique_lock<platform::mutex> lock(m_mutex);
#endif
	++m_busy_count;
	locked_run_one();
	return 0!=--m_busy_count;
}

active::run::run(int num_threads, scheduler & sched) :
	m_scheduler(sched)
{
	if( num_threads<1 ) num_threads=4;
	m_scheduler.start_work();	// Prevent threads from exiting prematurely
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
		m_ready.wait_for(lock, std::chrono::milliseconds(1));
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
	try
	{
		throw;
	}
	catch( std::exception & ex )
	{
		// std::cerr is NOT threadsafe.
		fprintf(stderr, "Unhandled exception during message processing: %s\n", ex.what());
	}
	catch( ... )
	{
		fprintf(stderr, "Unahndled exception during message processing\n");
	}
}

void active::scheduler::start_work() throw()
{
#ifndef ACTIVE_USE_CXX11
	platform::lock_guard<platform::mutex> lock(m_mutex);
#endif
	++m_busy_count;
}

void active::scheduler::stop_work() throw()
{
#ifndef ACTIVE_USE_CXX11
	platform::lock_guard<platform::mutex> lock(m_mutex);
#endif
	if(0==--m_busy_count)
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
	if(platform::this_thread::get_id() == m_thread.get_id())
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
