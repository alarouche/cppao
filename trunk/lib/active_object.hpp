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
#include <thread>

// The following options control the implementation
// Options have been selected for best speed:

#ifndef ACTIVE_OBJECT_ENABLE
	#define ACTIVE_OBJECT_ENABLE 1
#endif

#ifndef ACTIVE_OBJECT_CONDITION
	#define ACTIVE_OBJECT_CONDITION 1
#endif

#ifndef ACTIVE_OBJECT_SHARED_QUEUE
	#define ACTIVE_OBJECT_SHARED_QUEUE 0
#endif

#define ACTIVE_IFACE(TYPE) virtual void operator()( const TYPE & )=0;

namespace active
{	
	struct any_object
	{
		virtual ~any_object();
		virtual void run()=0;
		virtual void run_some(int n=100)=0;
		virtual void exception_handler();
	};
	
	
	// Represents a pool of active objects which can be executed in a thread pool.
	class pool
	{
	public:
		typedef any_object * ObjectPtr;
		typedef std::shared_ptr<any_object> SharedPtr;

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
	
	// As a convenience, we provide a global variable to run all active objects.
	extern pool default_pool;
		
	struct message
	{
		virtual void run(any_object * obj)=0;
		virtual ~message();
		message *next;
	};

			
	struct no_sharing	// Sharing concept
	{
		struct base { };
		
		typedef any_object * pointer_type;
		
		pointer_type pointer(any_object * obj) { return obj; }
	};
	
	struct enable_sharing	// Sharing concept
	{
		typedef std::enable_shared_from_this<any_object> base;
		
		typedef std::shared_ptr<any_object> pointer_type;
		
		pointer_type pointer(base * obj) { return obj->shared_from_this(); }
	};
		
	struct thread_pool	// Schedule concept
	{
		typedef active::pool type;
		
		type * m_pool;
		
		thread_pool() : m_pool(&default_pool)
		{
		}
		
		void set_pool(type&p)
		{
			m_pool = &p;
		}
		
		void activate(const std::shared_ptr<any_object> & sp)
		{
			m_pool->signal(sp);
		}

		void activate(any_object * obj)
		{
			m_pool->signal(obj);
		}	
	};
	
	struct own_thread // Schedule concept
	{
		struct {} type;
		
		std::thread m_thread;
		
		void activate(any_object * obj)
		{
			
		}
	};
	
	// Schedule concept; object is mutexed
	struct mutexed
	{
	};
	
	// Faking up message handlers: just do a direct call.
	struct direct_call // Queue concept
	{
		// !! More sophistication: use a mutex and throw something.
		template<typename Message, typename Accessor>		
		bool enqueue(any_object * object, const Message & msg, const Accessor&)
		{
			try
			{
				Accessor::run(object, msg);
			}
			catch(...)
			{
				object->exception_handler();
			}
			return false;
		}
		
		bool run_some(any_object * o, int n=100)
		{
			return false;
		}
		
		template<typename T>
		struct queue_data
		{
			struct type { };
		};		
	};
	
	// 
	struct mutexed_call
	{
		std::mutex m_mutex;
		template<typename Message, typename Accessor>		
		bool enqueue(any_object * object, const Message & msg, const Accessor&)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			try
			{
				Accessor::run(object, msg);
			}
			catch(...)
			{
				object->exception_handler();
			}
			return false;
		}
		
		bool run_some(any_object * o, int n=100)
		{
			return false;
		}
		
		template<typename T>
		struct queue_data
		{
			struct type { };
		};		
	};
	
	struct shared_queue	// Queue concept
	{
		typedef std::mutex mutex_type;
		mutex_type m_mutex;
		
		struct message
		{
			message() : m_next(nullptr) { }
			virtual void run(any_object * obj)=0;
			virtual ~message() { }
			message *m_next;
		};
		
		
		// Push to tail, pop from head:
		message *m_head, *m_tail;
		shared_queue() : m_head(nullptr), m_tail(nullptr) { }
		shared_queue(const shared_queue&) : m_head(nullptr), m_tail(nullptr) { }
		
		template<typename T>
		struct queue_data
		{
			struct type { };
		};
		
		template<typename Message, typename Accessor>
		struct message_impl : public message
		{
			message_impl(const Message & m) : m_message(m) { }
			Message m_message;
			void run(any_object * o)
			{
				Accessor::run(o, m_message);
			}
		};
		
		template<typename Message, typename Accessor>		
		bool enqueue( any_object *, const Message & msg, const Accessor&)
		{
			message_impl<Message, Accessor> * impl = new message_impl<Message, Accessor>(msg);
			
			std::lock_guard<mutex_type> lock(m_mutex);
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
		
		bool empty() const
		{
			return !m_head;
		}
		
		bool run_some(any_object * o, int n=100)
		{
			std::lock_guard<mutex_type> lock(m_mutex);
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
	};
	
	struct separate_queue	// Queue concept
	{
		std::mutex m_mutex;
		
		typedef void (*ActiveRun)(any_object*);
		std::deque<ActiveRun> m_message_queue;
		
		separate_queue() { }
		separate_queue(const separate_queue&) { }
		template<typename T>
		struct queue_data
		{
			typedef std::deque<T> type;
		};
		
		template<typename Message, typename Accessor>
		static void run(any_object*obj)
		{
			Message m = Accessor::data(obj).front();
			Accessor::data(obj).pop_front();
			Accessor::run(obj, m);
		}
		
		template<typename Message, typename Accessor>		
		bool enqueue( any_object * object, const Message & msg, const Accessor&)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_message_queue.push_back( &run<Message, Accessor> );
			Accessor::data(object).push_back(msg);
			return m_message_queue.size()==1;
		}
		
		bool empty() const
		{
			return m_message_queue.empty();
		}

		bool run_some(any_object * o, int n=100)
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
	};
	
	template<typename Queue>
	struct steal  : public Queue // Queue concept	!! This has not beed tested
	{
		bool m_running;
		steal() : m_running(false) { }
		
		template<typename Message, typename Accessor>		
		bool enqueue( any_object * object, const Message & msg, const Accessor& accessor)
		{
			this->m_mutex.lock();
			if( m_running || !this->empty())
			{
				this->m_mutex.unlock();
				Queue::enqueue( object, msg, accessor );
				return false;
			}
			else
			{
				m_running = true;
				this->m_mutex.unlock();
				try
				{
					Accessor::run(object, msg);
				}
				catch(...)
				{
					object->exception_handler();
				}
				this->m_mutex.lock();
				m_running = false;
				bool signal = !this->empty();
				this->m_mutex.unlock();
				return signal;
			}
		}
	};
	
	template<
		typename Schedule=thread_pool, 
		typename Queue=shared_queue, 
		typename Share=no_sharing>
	class object_impl : public any_object, public Share::base
	{
	public:
		typedef Schedule schedule_type;
		typedef Share share_type;
		typedef Queue queue_type;
		
		object_impl(const Queue & q, 
					const Schedule & s = Schedule(),
					const Share & sh = Share() ) : m_queue(q), m_schedule(s), m_share(sh)
		{
		}
		
		object_impl() { }
		
		void run2()
		{
			while( m_queue.run_some(this) )
				;
		}

		bool run_some2(int n=100)
		{
			// Run a few messages from the queue
			// if we still have messages, then reactivate this object.
			if( m_queue.run_some(this, n) )
			{
				m_schedule.activate(m_share.pointer(this));
				return true;
			}
			else
			{
				return false;
			}
		}
		
		void set_scheduler(typename schedule_type::type & pool)
		{
			m_schedule.set_pool(pool);
		}
		
	protected:
		template<typename T, typename M>
		void enqueue( const T & msg, const M & accessor)
		{
			if( m_queue.enqueue(this, msg, accessor) )
				m_schedule.activate(m_share.pointer(this));
		}

#if 0		
		template<typename T, typename M>
		void enqueue( const T && msg, typename queue_type::template queue_data<T>::type & qd, const M&&accessor)
		{
			m_queue.enqueue(msg, qd, accessor);
		}
#endif
		
	private:
		queue_type m_queue;
		schedule_type m_schedule;
		share_type m_share;
	};
	
	// !! Also && version
#define MESSAGE( MSG ) \
	queue_type::queue_data<MSG>::type queue_data_##MSG; \
	template<typename C> struct run_##MSG { \
		static void run(any_object*o, const MSG&m) { static_cast<C>(o)->impl_##MSG(m); } \
		static queue_type::queue_data<MSG>::type& data(any_object*o) {return static_cast<C>(o)->queue_data_##MSG; } }; \
    void operator()(const MSG & x) { enqueue(x, run_##MSG<decltype(this)>() ); } \
	void impl_##MSG(const MSG & MSG)

	
	class object : public object_impl<>
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
				//try
				//{
					m_pool->signal(this->shared_from_this());
				//}
				//catch( std::bad_weak_ptr& )
				//{
				//	m_pool->signal(this);
				//}
			}
		}
	};
		
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
