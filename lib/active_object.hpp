/* Really Simple Active Objects library
 * Written by Calum Grant 2012
 * Original location: http://calumgrant.net/projects/active/active_object.hpp
 * Free of copyright.
 * 
 * It uses C++11. In g++, compile with "-std=c++0x -pthread"
 *
 * This library demonstrates a very simple approach to implementing active objects.
 */

#ifndef ACTIVE_OBJECT_INCLUDED
#define ACTIVE_OBJECT_INCLUDED

#include <mutex>
#include <deque>
#include <condition_variable>
#include <memory>

// The following options control the implementation
// Options have been selected for best speed:

#ifndef ACTIVE_OBJECT_ENABLE
	#define ACTIVE_OBJECT_ENABLE 1
#endif

#ifndef ACTIVE_OBJECT_CONDITION
	#define ACTIVE_OBJECT_CONDITION 0
#endif

#ifndef ACTIVE_OBJECT_SHARED_QUEUE
	#define ACTIVE_OBJECT_SHARED_QUEUE 1
#endif

#define ACTIVE_IFACE(TYPE) virtual void operator()( const TYPE & )=0;

namespace active
{	
	class object;
	
	// Represents a pool of active objects which can be executed in a thread pool.
	class pool
	{
	public:
		typedef object * ObjectPtr;
		typedef std::shared_ptr<object> SharedPtr;

		pool();
		
		// Adds an active object to the pool.
		// There is no need to "un-add", but deleting an active object with active messages
		// is undefined (i.e. the application will crash).
		// void add(ObjectPtr p);
		
		// Used by an active object to signal that there are messages to process.
		void signal(ObjectPtr);
		void signal(const SharedPtr &);
		
		// Runs until there are no more messages in the entire pool.
		// Returns false if no more items.
		bool run();
				
		// Runs all objects in a thread pool, until there are no more messages.
		void run(int threads);

	private:
		std::mutex m_mutex;
		std::deque<ObjectPtr> m_signalled1;	// List of signalled objects. We don't want to run all objects because that could be inefficient.
		std::deque<SharedPtr> m_signalled2;
		std::condition_variable m_ready;
		int m_busy_count;	// Used to work out when we have actually finished.
		void run_in_thread();		
		static void thread_fn(pool * p);
		std::exception_ptr m_exception;
	};
	
	struct message
	{
		virtual void run(object * obj)=0;
		virtual ~message();
		message *next;
	};
	
	class object
	{
	public:
		std::mutex m_active_mutex;
	protected:		
#if ACTIVE_OBJECT_SHARED_QUEUE
		void add_message( message* );
		// Push to tail, pop from head:
		message *m_head, *m_tail; 	// Could use boost::intrusive_slist I suppose.
#else
		typedef void (*ActiveRun)(object*);
		std::deque<ActiveRun> m_message_queue;
		void signal_ready();
#endif
		virtual void add_to_pool();
	public:	
		// Exception thrown whenever the message queue is full.
		struct queue_full : public std::runtime_error 
		{ 
			queue_full();
		};
		
		void set_pool(pool&);
		object();
		~object();
		
		// Run the active object until there are no more messages.
		void run();
		
		// Execute a few messages, re-signal if at the end we still have some left.
		void run_some(int n=100);
		
		// The default exception handler - can be overridden.
		void exception_handler();
		
	protected:
		pool * m_pool;	

	private:
		object(const object&); // =delete;
		object & operator=(const object&); // =delete;
	};	
	
	// An active object which could be stored in a std::shared_ptr.
	// If this is the case, then a safer message queueing scheme is implemented
	// which for example guarantees to deliver all messages before destroying the object.
	template<typename T>
	class shared : public object, public std::enable_shared_from_this<T>
	{
	public:
		typedef std::shared_ptr<T> ptr;
		
		void add_to_pool()
		{
			if( m_pool ) 
			{
				try
				{
					m_pool->signal(this->shared_from_this());
				}
				catch( std::bad_weak_ptr& )
				{
					m_pool->signal(this);
				}
			}
		}
	};
	
	// As a convenience, we provide a global variable to run all active objects.
	extern pool default_pool;
	
	// Run all objects in the default thread pool (single-threaded)
	void run();
	
	// Run all objects in the default thread pool
	void run(int threads);
	
	// Like a std::lock_guard but works in reverse :-)
	template<typename Mutex>
	class unlock_guard
	{
	public:
		typedef Mutex mutex_type;
		explicit unlock_guard(mutex_type & m) : m_mutex(m) { m_mutex.unlock(); }
		~unlock_guard() { m_mutex.lock(); }
	private:
		mutex_type & m_mutex;
		unlock_guard(const unlock_guard&); // = delete;
		unlock_guard & operator=(const unlock_guard&); // = delete;		
	};

	template<typename T>
	struct sink
	{
		typedef std::shared_ptr<sink<T>> ptr;
		ACTIVE_IFACE( T );
	};	
} // namespace active

#if ACTIVE_OBJECT_ENABLE
	
	// Use to implement active method outside of the class.
	#define ACTIVE_IMPL2( TYPE, NAME ) active_impl_##NAME( const TYPE & NAME )
	
#if ACTIVE_OBJECT_SHARED_QUEUE
	// Implementation #1
	// A touch slower but uses less memory.
	// !! What about exception safety

	/* This macro is used to add a new active method to an active object.
	* NAME must be a typename.
	*/
	#define ACTIVE_METHOD2( TYPE, NAME ) \
		template<typename Derived> struct active_message_##NAME : public ::active::message { \
			active_message_##NAME( const TYPE & p ) : payload(p) { } \
			NAME payload; \
			void run(::active::object * obj) { \
				Derived object = static_cast<Derived>(obj); \
				::active::unlock_guard<std::mutex> lck(object->m_active_mutex);\
				try { object->active_impl_##NAME(std::move(payload)); } \
				catch(...) { object->exception_handler(); } } \
			}; \
		void operator()( const TYPE & value ) { \
			add_message( new active_message_##NAME<decltype(this)>(value) ); } \
		void ACTIVE_IMPL2( TYPE, NAME )
#else
	// Implementation #2
	// A bit faster but uses more memory.
	
	/* This macro is used to add a new active method to an active object.
	* NAME must be a typename.
	*/
	#define ACTIVE_METHOD2( TYPE, NAME ) \
		std::deque<TYPE> m_message_queue_##NAME; \
		template<typename Derived> 	static void active_run_##NAME(::active::object * obj) { \
			Derived object = static_cast<Derived>(obj); \
			NAME v(object->m_message_queue_##NAME.front()); \
			object->m_message_queue_##NAME.pop_front(); \
			::active::unlock_guard<std::mutex> lck(object->m_active_mutex); \
			try { object->active_impl_##NAME(std::move(v)); } \
			catch(...) { object->exception_handler(); } } \
		void operator()( const TYPE & value ) { \
			std::lock_guard<std::mutex> lock(m_active_mutex); \
			m_message_queue_##NAME.push_back(value); \
			try { m_message_queue.push_back( &active_run_##NAME<decltype(this)> ); } \
			catch(...) { m_message_queue_##NAME.pop_back(); throw; } \
			try { signal_ready(); } catch(...) \
			{ m_message_queue_##NAME.pop_back(); m_message_queue.pop_back(); throw; } } \
		void ACTIVE_IMPL2( TYPE, NAME )
#endif
#else
	// Implementation #3:
	// Makes all active calls synchronous (just like normal methods).
	// This will be the fastest if your active methods are trivial due
	// to the sheer overhead of building and queueing messages.

	#define ACTIVE_IMPL2( TYPE, NAME ) operator()(const TYPE & NAME)
	#define ACTIVE_METHOD2( TYPE, NAME ) void ACTIVE_IMPL( TYPE, NAME )    
    
#endif   

#define ACTIVE_METHOD( NAME ) ACTIVE_METHOD2( NAME, NAME )
#define ACTIVE_IMPL( NAME) ACTIVE_IMPL2( NAME, NAME )
    
#endif
