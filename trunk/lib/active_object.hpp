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

#define ACTIVE_OBJECT_CONDITION 0

#define ACTIVE_IFACE(TYPE) virtual void operator()( const TYPE & )=0;

namespace active
{	
	struct any_object
	{
		virtual ~any_object();
		virtual void run()=0;
		virtual bool run_some(int n=100)=0;
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
		
		// Thread tracking
		void start_work();
		void stop_work();
		
		// Runs until there are no more messages in the entire pool.
		// Returns false if no more items.
		void run();
				
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
		
		bool run_managed();
	};
	
	// As a convenience, provide a global variable to run all active objects.
	extern pool default_pool;

	namespace sharing	// Namespace for sharing concepts
	{
		struct disabled
		{
			struct base { };
		
			typedef any_object * pointer_type;
		
			pointer_type pointer(any_object * obj) { return obj; }
		};
	
		template<typename Obj=any_object>
		struct enabled
		{
			typedef std::enable_shared_from_this<Obj> base;
		
			typedef std::shared_ptr<any_object> pointer_type;
		
			pointer_type pointer(base * obj) { return obj->shared_from_this(); }
		};
	}
		
	namespace schedule	// Schedulers
	{
		struct thread_pool
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

		struct own_thread // Under construction ... !!
		{
			typedef active::pool type;			
			type * m_pool;
			void set_pool(type&p)
			{
				m_pool = &p;
			}
			
			bool m_shutdown;
			bool m_activated;	// Can merge with m_object
			any_object * m_object;
			std::mutex m_mutex;
			std::condition_variable m_ready;
			std::thread m_thread;
			
			void activate(any_object * obj)
			{
				m_object = obj;
				m_activated=true;
				if( m_pool && m_activated ) m_pool->start_work();
				std::lock_guard<std::mutex> lock(m_mutex); // ??
				m_ready.notify_one();
			}
			
			void activate(const std::shared_ptr<any_object> & sp)
			{
				activate( sp.get() );
			}
			
			void thread_fn()
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				while( !m_shutdown )
				{
					if( m_activated )
					{
						lock.unlock();
						m_object->run();
						lock.lock();
						if(m_pool) m_pool->stop_work();
						m_activated = false;
					}
					m_ready.wait(lock);
				}
			}
			
			own_thread() : m_pool(&default_pool), m_shutdown(false), m_activated(false), m_thread( std::bind( &own_thread::thread_fn, this ) ) 
			{ 
			}
			
			~own_thread()
			{
				{
					std::lock_guard<std::mutex> lock(m_mutex); 
					m_shutdown = true;
					m_ready.notify_one();
				}
				m_thread.join();
			}
		};
	}
	
	
	
	namespace queueing	// The queuing policy classes
	{
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
			
			bool empty() const { return true; }
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
			
			bool empty() const { return true; }
			
		};
		
		// Under construction !!
		struct try_lock
		{
		};
		
		struct shared
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
			shared() : m_head(nullptr), m_tail(nullptr) { }
			shared(const shared&) : m_head(nullptr), m_tail(nullptr) { }
			
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
		
		struct separate
		{
			std::mutex m_mutex;
			
			typedef void (*ActiveRun)(any_object*);
			std::deque<ActiveRun> m_message_queue;
			
			separate() { }
			separate(const separate&) { }
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
				// !! Exception safety.
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
					this->m_mutex.unlock();	// !! Better use of locks; also exception safety.
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
	}
	
	
	
	template<
		typename Schedule=schedule::thread_pool, 
		typename Queue=queueing::shared, 
		typename Share=sharing::disabled>
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
		
		~object_impl() { if(!m_queue.empty()) std::terminate(); }
		
		void run()
		{
			while( m_queue.run_some(this) )
				;
		}

		bool run_some(int n)
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
			// !! Exception safety
			if( m_queue.enqueue(this, msg, accessor) )
				m_schedule.activate(m_share.pointer(this));
		}

		template<typename T, typename M>
		void enqueue( T && msg, const M&&accessor)
		{
			// !! Exception safety
			if( m_queue.enqueue(this, msg, accessor) )
				m_schedule.activate(m_share.pointer(this));
		}

		
	private:
		queue_type m_queue;
		schedule_type m_schedule;
		share_type m_share;
	};
	
#define ACTIVE_IMPL( MSG ) impl_##MSG(const MSG & MSG)

#define ACTIVE_METHOD( MSG ) \
	typename queue_type::template queue_data<MSG>::type queue_data_##MSG; \
	template<typename C> struct run_##MSG { \
		static void run(::active::any_object*o, const MSG&m) { static_cast<C>(o)->impl_##MSG(m); } \
		static typename queue_type::template queue_data<MSG>::type& data(::active::any_object*o) {return static_cast<C>(o)->queue_data_##MSG; } }; \
    void operator()(const MSG & x) { this->enqueue(x, run_##MSG<decltype(this)>() ); } \
	void operator()(MSG && x) { this->enqueue(x, run_##MSG<decltype(this)>() ); } \
	void ACTIVE_IMPL(MSG)

	
	// The default object type.
	typedef object_impl<schedule::thread_pool, queueing::shared, sharing::disabled> object;
	typedef object_impl<schedule::own_thread, queueing::shared, sharing::disabled> thread;
	
	typedef object_impl<schedule::thread_pool, queueing::steal<queueing::shared>, sharing::disabled> fast;
	
	// An active object which could be stored in a std::shared_ptr.
	// If this is the case, then a safer message queueing scheme is implemented
	// which for example guarantees to deliver all messages before destroying the object.
	template<typename T, typename Schedule=schedule::thread_pool, typename Queueing = queueing::shared>
	class shared : public object_impl<Schedule, Queueing, sharing::enabled<T> >
	{
	public:
		typedef std::shared_ptr<T> ptr;
	};
	
	template<typename T, typename Queueing = queueing::shared>
	class shared_thread : public object_impl<schedule::own_thread, Queueing, sharing::enabled<T> >
	{
	public:
		typedef std::shared_ptr<T> ptr;
	};
		
	// Run all objects in the default thread pool (single-threaded)
	void run();
	
	// Run all objects in the default thread pool
	void run(int threads);
	
	template<typename T>
	struct sink
	{
		typedef std::shared_ptr<sink<T>> ptr;
		ACTIVE_IFACE( T );
	};	
} // namespace active

    
#endif
