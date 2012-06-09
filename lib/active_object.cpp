#include "active_object.hpp"
#include <thread>
#include <iostream>

#define ACTIVE_OBJECT_CONDITION 0


active::pool active::default_pool;

active::pool::pool() : m_head(nullptr), m_tail(nullptr), m_busy_count(0)
{
}

active::any_object::~any_object()
{
}


// Used by an active object to signal that there are messages to process.
void active::pool::activate(ObjectPtr p) noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
    p->m_next=nullptr;
    if( !m_head )
        m_head = m_tail = p;
    else
        m_tail->m_next = p, m_tail=p;
	++m_busy_count;
#if ACTIVE_OBJECT_CONDITION
	m_ready.notify_one();
#endif
}


// Runs until there are no more messages in the entire pool.
// Returns false if no more items.
bool active::pool::run_managed() noexcept
{
	std::unique_lock<std::mutex> lock(m_mutex);

    while( m_head )
	{ 
        ObjectPtr p = m_head;
        m_head = m_head->m_next;
        lock.unlock();
        p->run_some();
        lock.lock();
        --m_busy_count;
    }
    m_tail=nullptr;
	
	// Can be non-zero if the queues are empty, but other threads are processing.
	// the result of processing could be to add more signalled objects.
	return m_busy_count!=0;
}
		
// Runs all objects in a thread pool, until there are no more messages.
void active::pool::run(int threads)
{
	std::deque<std::thread> tp(threads);	// ?? Why not vector ??
	
	for(int i=0; i<threads; ++i)
		tp[i] = std::thread( std::bind(&pool::run_in_thread, this) ); 
	
	for(int i=0; i<threads; ++i)
		tp[i].join();

    if( m_exception )
        std::rethrow_exception( m_exception );
}

void active::pool::run()
{
	while( run_managed() )
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

// Like run(), but when starved, waits for a little while instead of exiting.
// Note we could use a condition variable here, but this is slow when you have a 
// silly number of threads like 503...
void active::pool::run_in_thread()
{
	try 
	{
		run();
	}
	catch( ... )
	{
		// Basically something like bad_alloc.
		m_exception = std::current_exception();
	}
}


void active::any_object::exception_handler() noexcept
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

void active::run(int threads)
{
	if( threads<=0 ) threads=4;
	default_pool.run(threads);
}

void active::pool::start_work() noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	++m_busy_count;
}

void active::pool::stop_work() noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if( 0 == --m_busy_count )
	{
		m_ready.notify_one();
	}
}

active::schedule::thread_pool::thread_pool(pool & p) : m_pool(&p)
{
}

void active::schedule::thread_pool::set_pool(type&p)
{
	m_pool = &p;
}

void active::schedule::thread_pool::activate(const std::shared_ptr<any_object> & sp)
{
    if( m_pool ) m_pool->activate(sp.get());
}

void active::schedule::thread_pool::activate(any_object * obj)
{
	if(m_pool) m_pool->activate(obj);
}	

void active::schedule::own_thread::activate(any_object * obj)
{
    m_object = obj;
    if( m_pool ) m_pool->start_work();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ready.notify_one();
}

void active::schedule::own_thread::activate(const std::shared_ptr<any_object> & sp)
{
    m_shared = sp;
    activate( sp.get() );
}

void active::schedule::own_thread::thread_fn()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while( !m_shutdown )
    {
        if( m_object )
        {
            lock.unlock();
            m_object->run();
            lock.lock();
            if(m_pool) m_pool->stop_work();
            m_object = nullptr;
            if( m_shared.unique() ) m_shutdown = true;
        }
        m_ready.wait(lock);
    }
    m_shared.reset();	// Allow object to be destroyed
}

active::schedule::own_thread::own_thread(const own_thread&other) :
    m_pool(other.m_pool),
    m_shutdown(false),
    m_object(nullptr),
    m_thread( std::bind( &own_thread::thread_fn, this ) )
{
}

active::schedule::own_thread::own_thread(pool & p) :
    m_pool(&p),
    m_shutdown(false),
    m_object(nullptr),
    m_thread( std::bind( &own_thread::thread_fn, this ) )
{
}

active::schedule::own_thread::~own_thread()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown = true;
        m_ready.notify_one();
    }
    m_thread.join();
}

bool active::queueing::direct_call::run_some(any_object * o, int n)
{
    return false;
}

bool active::queueing::direct_call::empty() const
{
    return true;
}

active::queueing::mutexed_call::mutexed_call()
{
}

active::queueing::mutexed_call::mutexed_call(const mutexed_call&)
{
}

bool active::queueing::mutexed_call::run_some(any_object * o, int n)
{
    return false;
}

bool active::queueing::mutexed_call::empty() const
{
    return true;
}

bool active::queueing::try_lock::run_some(any_object * o, int n)
{
    return false;
}

bool active::queueing::try_lock::empty() const
{
    return true;
}

active::queueing::separate::separate()
{
}

active::queueing::separate::separate(const separate&)
{
}

bool active::queueing::separate::empty() const
{
    return m_message_queue.empty();
}

bool active::queueing::separate::run_some(any_object * o, int n)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    while( !m_message_queue.empty() && n-->0 )
    {
        ActiveRun run = m_message_queue.front();
        m_mutex.unlock();
        try
        {
            (*run)(o);
        }
        catch (...)
        {
            o->exception_handler();
        }
        m_mutex.lock();
        m_message_queue.pop_front();
    }
    return !m_message_queue.empty();
}

active::queueing::shared::shared() : m_head(nullptr), m_tail(nullptr)
{
}

active::queueing::shared::shared(const shared&) : m_head(nullptr), m_tail(nullptr)
{
}

bool active::queueing::shared::enqueue(message*impl)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if( m_tail )
    {
        m_tail->m_next = impl;
        m_tail = impl;
        return false;
    }
    else
    {
        m_head = m_tail = impl;
        return true;
    }
}

bool active::queueing::shared::empty() const
{
    return !m_head;
}

bool active::queueing::shared::run_some(any_object * o, int n)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while( m_head && n-->0)
    {
        message * m = m_head;

        m_mutex.unlock();
        try
        {
            m->run(o);
        }
        catch (...)
        {
            o->exception_handler();
        }
        m_mutex.lock();
        m_head = m_head->m_next;
        if(!m_head) m_tail=nullptr;
        delete m;
    }
    return m_head;
}
