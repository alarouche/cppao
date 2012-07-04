/* Cppao: C++ Active Objects library
 * Written by Calum Grant 2012
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
#include <future>
#include <queue>

#define ACTIVE_IFACE(MSG) virtual void operator()( MSG & )=0; virtual void operator()( MSG && )=0;

namespace active
{
	// Interface of all active objects.
	struct any_object
	{
		virtual ~any_object();
		virtual void run() throw()=0;
		virtual bool run_some(int n=100) throw()=0;
		virtual void exception_handler() throw();
		any_object * m_next;
	};


	// Represents a pool of active objects which can be executed in a thread pool.
	class scheduler
	{
	public:
		typedef any_object * ObjectPtr;

		scheduler();

		// Adds an active object to the pool.
		// There is no need to "un-add", but deleting an active object with active messages
		// is undefined (i.e. the application will crash).
		// void add(ObjectPtr p);

		// Used by an active object to signal that there are messages to process.
		void activate(ObjectPtr) throw();

		// Thread tracking:
		void start_work() throw();
		void stop_work() throw();

		// Runs in current thread until there are no more messages in the entire pool.
		// Returns false if no more items.
		// Can be run concurrently.
		void run();
		
		// Runs for a short while.
		// Called from inside a message loop.
		// Returns true if there are still messages to be processed,
		// false if no more messages are available.
		// Can be run concurrently.
		bool run_one();

	private:
		std::mutex m_mutex;
		any_object *m_head, *m_tail;   // List of activated objects.
		std::condition_variable m_ready;
		int m_busy_count;	// Used to work out when we have actually finished.
		bool run_managed() throw();
		void run_in_thread();
	};

	// As a convenience, provide a global variable to run all active objects.
	extern scheduler default_scheduler;

	namespace sharing	// Namespace for sharing concepts
	{
		struct disabled
		{
			struct base { };
			typedef any_object * pointer_type;
			pointer_type pointer(any_object * obj) { return obj; }
			void activate(base *) { }
			void deactivate(pointer_type) { }
		};

		template<typename Obj=any_object>
		struct enabled
		{
			typedef std::enable_shared_from_this<Obj> base;
			typedef std::shared_ptr<any_object> pointer_type;
			pointer_type pointer(base * obj) { return obj->shared_from_this(); }
			void activate(base * obj) { m_activated = pointer(obj); }
			void deactivate(pointer_type & p) { m_activated.swap(p); }
		private:
			pointer_type m_activated;   // Prevent object being destroyed whilst active
		};
	}

	namespace schedule	// Schedulers
	{
		// The object is not scheduled
		struct none
		{
			typedef active::scheduler type;
			none(type&) { }
			void set_scheduler(type&p) { }
			void activate(const std::shared_ptr<any_object> & sp) { }
			void activate(any_object * obj) { }
		};

		// The object is scheduled using the thread pool (active::pool)
		struct thread_pool
		{
			typedef active::scheduler type;
			thread_pool(type&);

			void set_scheduler(type&p);
			void activate(const std::shared_ptr<any_object> & sp);
			void activate(any_object * obj);
			type & get_scheduler() const { return *m_pool; }
		private:
			type * m_pool;
		};

		// The object is scheduled by the OS in its own thread.
		struct own_thread
		{
			typedef active::scheduler type;

			own_thread(const own_thread&);
			own_thread(type&);

			void set_scheduler(type&p);
			void activate(any_object * obj);
			void activate(const std::shared_ptr<any_object> & sp);

			own_thread();
			~own_thread();
		private:
			void thread_fn();
			type * m_pool;  // Let pool track when all threads are idle => finished.
			bool m_shutdown;
			any_object * m_object;
			std::shared_ptr<any_object> m_shared;   // !! Remove this
			std::mutex m_mutex;
			std::condition_variable m_ready;
			std::thread m_thread;
		};
	}
	
	template<typename T> int priority(const T&) { return 0; }
	
	struct prioritize
	{
		template<typename T>
		int operator()(const T&v) const { return priority(v); }
	};

	namespace queueing	// The queuing policy classes
	{
		// No queueing; run in calling thread without protection.
		struct direct_call
		{
			typedef std::allocator<void> allocator_type;

			direct_call(const allocator_type& =allocator_type());

			allocator_type get_allocator() const { return allocator_type(); }

			template<typename Message, typename Accessor>
			bool enqueue(any_object * object, Message && msg, const Accessor&)
			{
				try
				{
					Accessor::active_run(object, std::forward<Message&&>(msg));
				}
				catch(...)
				{
					object->exception_handler();
				}
				return false;
			}

			bool run_some(any_object * o, int n=100) throw();

			template<typename T>
			struct queue_data
			{
				struct type { };
			};

			bool empty() const;
		};

		// Safe but runs in the calling thread and can deadlock.
		struct mutexed_call
		{
			typedef std::allocator<void> allocator_type;

			mutexed_call(const allocator_type & alloc = allocator_type());
			mutexed_call(const mutexed_call&);

			allocator_type get_allocator() const { return allocator_type(); }

			template<typename Message, typename Accessor>
			bool enqueue(any_object * object, Message && msg, const Accessor&)
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutex);
				try
				{
					Accessor::active_run(object, std::forward<Message&&>(msg));
				}
				catch(...)
				{
					object->exception_handler();
				}
				return false;
			}

			bool run_some(any_object * o, int n=100) throw();

			template<typename T>
			struct queue_data
			{
				struct type { };
			};

			bool empty() const;

		private:
			std::recursive_mutex m_mutex;
		};


		// Default message queue shared between all message types.
		template<typename Allocator=std::allocator<void>>
		class shared
		{
		public:
			typedef Allocator allocator_type;

		private:
		   struct message
		   {
			   message() : m_next(nullptr) { }
			   virtual void run(any_object * obj)=0;
			   virtual void destroy(allocator_type&)=0;
			   message *m_next;
		   };
		public:

			shared(const allocator_type & alloc = allocator_type()) :
				m_head(nullptr), m_tail(nullptr), m_allocator(alloc)
			{
			}

			shared(const shared&o) :
				 m_head(nullptr), m_tail(nullptr), m_allocator(o.m_allocator)
			{
			}

			allocator_type get_allocator() const { return m_allocator; }

			template<typename T>
			struct queue_data
			{
				struct type { };
			};

			template<typename Message, typename Accessor>
			bool enqueue( any_object *, Message && msg, const Accessor&)
			{
				typename allocator_type::template rebind<message_impl<Message, Accessor>>::other realloc(m_allocator);

				message_impl<Message, Accessor> * impl = realloc.allocate(1);
				try
				{
					realloc.construct(impl, std::forward<Message&&>(msg));
				}
				catch(...)
				{
					realloc.deallocate(impl,1);
					throw;
				}

				return enqueue( impl );
			}

			bool empty() const
			{
				return !m_head;
			}

			bool run_some(any_object * o, int n=100) throw()
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
					m->destroy(m_allocator);
				}
				return m_head!=nullptr;
			}

		private:
			// Push to tail, pop from head:
			message *m_head, *m_tail;
			bool enqueue(message*impl)
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

		private:
			template<typename Message, typename Accessor>
			struct message_impl : public message
			{
				message_impl(Message && m) : m_message(std::forward<Message&&>(m)) { }
				Message m_message;
				void run(any_object * o)
				{
					Accessor::active_run(o, std::move(m_message));
				}
				void destroy(allocator_type&a)
				{
					typename allocator_type::template rebind<message_impl<Message, Accessor>>::other realloc(a);
					realloc.destroy(this);
					realloc.deallocate(this,1);
				}
			};

			allocator_type m_allocator;
		protected:
			std::mutex m_mutex;

		};
		
		// Message queue offering cancellation, prioritization and size
		template<typename Allocator=std::allocator<void>, typename Priority=prioritize>
		class advanced
		{
		public:
			typedef Allocator allocator_type;
			typedef Priority priority_type;
			
		private:
			struct message
			{
				message(int p, std::size_t s) : m_priority(p), m_sequence(s) { }
				virtual void run(any_object * obj)=0;
				virtual void destroy(allocator_type&)=0;
				const int m_priority;
				const std::size_t m_sequence;
			};
		public:
			
			advanced(const allocator_type & alloc = allocator_type(), std::size_t capacity=1000, const priority_type p = priority_type()) : 
				m_priority(p), m_allocator(alloc), m_messages(alloc), m_capacity(capacity), m_busy(false), m_sequence(0)
			{
			}
			
			advanced(const advanced&o) : 
				m_priority(o.m_priority), m_allocator(o.m_allocator), m_messages(m_allocator), m_capacity(o.m_capacity), m_busy(false), m_sequence(0)
			{
			}
			
			allocator_type get_allocator() const { return m_allocator; }
			
			template<typename T>
			struct queue_data
			{
				struct type { };
			};
			
			template<typename Message, typename Accessor>
			bool enqueue( any_object *, Message && msg, const Accessor&)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if( m_messages.size() >= m_capacity ) throw std::bad_alloc();
				
				typename allocator_type::template rebind<message_impl<Message, Accessor>>::other realloc(m_allocator);
				
				message_impl<Message, Accessor> * impl = realloc.allocate(1);
				int priority = m_priority(msg);
				
				try
				{
					realloc.construct(impl, message_impl<Message, Accessor>(std::forward<Message&&>(msg), priority, m_sequence++)  );
				}
				catch(...)
				{
					realloc.deallocate(impl,1);
					throw;
				}
				
				try
				{
					return enqueue( impl );
				}
				catch(...)
				{
					impl->destroy(m_allocator);
					throw;
				}
			}
			
			bool empty() const
			{
				return !m_busy && m_messages.empty();
			}
			
			bool mutexed_empty() const
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				return m_messages.empty(); // Note: different from empty();
			}
			
			void set_capacity(std::size_t new_capacity) 
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_capacity = new_capacity;
			}
			
			bool run_some(any_object * o, int n=100) throw()
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				while( !m_messages.empty() && n-->0)
				{
					message * m = m_messages.top();
					m_messages.pop();
					m_busy = true;
					
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
					m_busy = false;
					m->destroy(m_allocator);
				}
				return !m_messages.empty();
			}
			
			void clear()
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				while( !m_messages.empty() )
				{
					m_messages.top()->destroy(m_allocator);
					m_messages.pop();
				}
			}
			
			std::size_t size() const
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				return m_messages.size();
			}
			
		private:
			priority_type m_priority;
			allocator_type m_allocator;
			
			struct msg_cmp { bool operator()(message *m1, message *m2) const 
			{ 
				return m1->m_priority < m2->m_priority || (m1->m_priority == m2->m_priority && m1->m_sequence > m2->m_sequence);
			} };
			std::priority_queue<message*, std::vector<message*, typename allocator_type::template rebind<message*>::other>, msg_cmp> m_messages;
			
			bool enqueue(message*impl)
			{
				bool first_one = m_messages.empty() && !m_busy;
				m_messages.push(impl);
				return first_one;
			}
			
			template<typename Message, typename Accessor>
			struct message_impl : public message
			{
				message_impl(Message && m, int priority, std::size_t seq) : message(priority, seq), m_message(std::forward<Message&&>(m)) { }
				Message m_message;
				void run(any_object * o)
				{
					Accessor::active_run(o, std::move(m_message));
				}
				void destroy(allocator_type&a)
				{
					typename allocator_type::template rebind<message_impl<Message, Accessor>>::other realloc(a);
					realloc.destroy(this);
					realloc.deallocate(this,1);
				}
			};
			std::size_t m_capacity;
			bool m_busy;
			std::size_t m_sequence;
			
		protected:
			mutable std::mutex m_mutex;
			
		};
		

		template<typename Allocator=std::allocator<void>>
		struct separate
		{
			typedef Allocator allocator_type;

			separate(const allocator_type & alloc=allocator_type()) :
			   m_message_queue(alloc)
			{
			}

			allocator_type get_allocator() const { return m_message_queue.get_allocator(); }

			separate(const separate&o) :
				m_message_queue(o.m_message_queue.get_allocator())
			{
			}

			template<typename T>
			struct queue_data
			{
				typedef std::deque<T, typename allocator_type::template rebind<T>::other> type;
			};

			template<typename Message, typename Accessor>
			bool enqueue( any_object * object, Message && msg, const Accessor&)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_message_queue.push_back( &run<Message, Accessor> );
				try
				{
					Accessor::active_data(object).push_back(std::forward<Message&&>(msg));
				}
				catch(...)
				{
					m_message_queue.pop_back();
					throw;
				}
				return m_message_queue.size()==1;
			}

			bool empty() const
			{
				return m_message_queue.empty();
			}

			bool run_some(any_object * o, int n=100) throw()
			{
			   std::lock_guard<std::mutex> lock(m_mutex);

			   while( !m_message_queue.empty() && n-->0 )
			   {
				   m_mutex.unlock();
				   try
				   {
					   ActiveRun run = m_message_queue.front();
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

		protected:
			std::mutex m_mutex;

		private:
			typedef void (*ActiveRun)(any_object*);
			std::deque<ActiveRun, typename allocator_type::template rebind<ActiveRun>::other> m_message_queue;

			template<typename Message, typename Accessor>
			static void run(any_object*obj)
			{
				Message m = std::move(Accessor::active_data(obj).front());
				Accessor::active_data(obj).pop_front();
				Accessor::active_run(obj, std::move(m));
			}
		};

		template<typename Queue>
		struct steal  : public Queue
		{
			typedef typename Queue::allocator_type allocator_type;
			steal(const allocator_type & alloc = allocator_type()) : Queue(alloc), m_running(false) { }

			template<typename Message, typename Accessor>
			bool enqueue( any_object * object, Message && msg, const Accessor& accessor)
			{
				this->m_mutex.lock();
				if( m_running || !this->empty())
				{
					this->m_mutex.unlock();	// ?? Better use of locks; also exception safety.
					Queue::enqueue( object, std::forward<Message&&>(msg), accessor );
					return false;
				}
				else
				{
					m_running = true;
					this->m_mutex.unlock();
					try
					{
						Accessor::active_run(object, std::forward<Message&&>(msg));
					}
					catch(...)
					{
						object->exception_handler();
					}
					this->m_mutex.lock();
					m_running = false;
					bool activate = !this->empty();
					this->m_mutex.unlock();
					return activate;
				}
			}
		private:
			bool m_running;
		};
	}



	template<
		typename Schedule,
		typename Queue,
		typename Share>
	class object_impl : public any_object, public Share::base
	{
	public:
		typedef Schedule schedule_type;
		typedef Share share_type;
		typedef Queue queue_type;
		typedef typename schedule_type::type scheduler_type;
		typedef typename queue_type::allocator_type allocator_type;

		object_impl(scheduler_type & tp = default_scheduler,
					const allocator_type & alloc = allocator_type())
					: m_schedule(tp), m_queue(alloc) { }

		~object_impl() { if(!m_queue.empty()) std::terminate(); }

		object_impl(const queue_type & q, scheduler_type & tp = default_scheduler)
			: m_schedule(tp), m_queue(q) { }

		allocator_type get_allocator() const { return m_queue.get_allocator(); }

		void run() throw()
		{
			typename share_type::pointer_type p=0;
			m_share.deactivate(p);

			while( m_queue.run_some(this) )
				;
		}

		// Not threadsafe - do not call unless you know what you are doing.
		bool run_some(int n) throw()
		{
			typename share_type::pointer_type p=0;
			m_share.deactivate(p);

			// Run a few messages from the queue
			// if we still have messages, then reactivate this object.
			if( m_queue.run_some(this, n) )
			{
				m_share.activate(this);
				m_schedule.activate(m_share.pointer(this));
				return true;
			}
			else
			{
				return false;
			}
		}
		
		// Not threadsafe; must be called before message processing.
		void set_scheduler(typename schedule_type::type & sch)
		{
			m_schedule.set_scheduler(sch);
		}
		
		typename schedule_type::type & get_scheduler() const
		{
			return m_schedule.get_scheduler();
		}
		
		// Do something whilst waiting for something else.
		bool steal()
		{
			return get_scheduler().run_one();
		}
		
		void set_capacity(std::size_t c)
		{
			m_queue.set_capacity(c);
		}
		
		typedef std::size_t size_type;
		
		size_type size() const
		{
			return m_queue.size();
		}
		
		bool empty() const
		{
			return m_queue.mutexed_empty();
		}
		
		// Gets the priority of the next waiting message
		int get_priority() const
		{
			return m_queue.get_priority();
		}
		
	protected:
		// Clear all pending messages. Only callable from active methods.
		void clear()
		{
			m_queue.clear();
		}

	protected:
		template<typename T, typename M>
		void enqueue( T && msg, const M & accessor)
		{
			if( m_queue.enqueue(this, std::forward<T&&>(msg), accessor) )
			{
				m_share.activate(this);
				m_schedule.activate(m_share.pointer(this));
			}
		}

	private:
		schedule_type m_schedule;
		queue_type m_queue;
		share_type m_share;
	};

#define ACTIVE_IMPL( MSG ) impl_##MSG(MSG && MSG)


#define ACTIVE_METHOD2( MSG, TYPENAME, TEMPLATE, CAST ) \
	TYPENAME queue_type::TEMPLATE queue_data<MSG>::type queue_data_##MSG; \
	template<typename C> struct run_##MSG { \
		static void active_run(::active::any_object*o, MSG&&m) { CAST<C>(o)->impl_##MSG(std::forward<MSG&&>(m)); } \
		static TYPENAME queue_type::TEMPLATE queue_data<MSG>::type& active_data(::active::any_object*o) {return CAST<C>(o)->queue_data_##MSG; } }; \
	void operator()(MSG && m) { this->enqueue(std::forward<MSG&&>(m), run_##MSG<decltype(this)>() ); } \
	void operator()(MSG & m) { this->enqueue(std::move(m), run_##MSG<decltype(this)>() ); } \
	void ACTIVE_IMPL(MSG)


#define ACTIVE_METHOD( MSG ) ACTIVE_METHOD2(MSG,,,static_cast)
#define ACTIVE_TEMPLATE(MSG) ACTIVE_METHOD2(MSG,typename,template,reinterpret_cast)

	// The default object type.
	typedef object_impl<schedule::thread_pool, queueing::shared<>, sharing::disabled> object;
	typedef object_impl<schedule::own_thread, queueing::shared<>, sharing::disabled> thread;

	typedef object_impl<schedule::thread_pool, queueing::steal<queueing::shared<>>, sharing::disabled> fast;
	typedef object_impl<schedule::none, queueing::direct_call, sharing::disabled> direct;
	typedef object_impl<schedule::none, queueing::mutexed_call, sharing::disabled> synchronous;
	typedef object_impl<schedule::thread_pool, queueing::advanced<>, sharing::disabled> advanced;

	// An active object which could be stored in a std::shared_ptr.
	// If this is the case, then a safer message queueing scheme is implemented
	// which for example guarantees to deliver all messages before destroying the object.
	template<typename T=any_object, typename Schedule=schedule::thread_pool, typename Queueing = queueing::shared<>>
	class shared : public object_impl<Schedule, Queueing, sharing::enabled<T> >
	{
	public:
		typedef typename Queueing::allocator_type allocator_type;
		typedef typename Schedule::type scheduler_type;
		shared(scheduler_type & sch=default_scheduler, const allocator_type & alloc = allocator_type()) :
			object_impl<Schedule, Queueing, sharing::enabled<T> >(sch, alloc)
		{
		}                                                                                                           // !!shared_thread(shceduler, allocator)
		typedef std::shared_ptr<T> ptr;
	};

	template<typename T=any_object, typename Queueing = queueing::shared<>>
	class shared_thread : public object_impl<schedule::own_thread, Queueing, sharing::enabled<T> >
	{
	public:
		typedef typename Queueing::allocator_type allocator_type;
		typedef schedule::own_thread::type scheduler_type;
		shared_thread(scheduler_type & sch=default_scheduler, const allocator_type & alloc = allocator_type()) :
			object_impl<schedule::own_thread, Queueing, sharing::enabled<T> >(sch, alloc)
		{
		}

		typedef std::shared_ptr<T> ptr;
	};

	// Runs the scheduler for a given duration.
	class run
	{
	public:
		explicit run(int threads=std::thread::hardware_concurrency(), scheduler & sched = default_scheduler);
		~run();
	private:
		run(const run&);
		run & operator=(const run&);
		scheduler & m_scheduler;
		std::deque<std::thread> m_threads;
	};

	template<typename T>
	struct sink
	{
		typedef std::shared_ptr<sink<T>> sp;
		typedef std::weak_ptr<sink<T>> wp;
		ACTIVE_IFACE( T );
	};

	template<typename T>
	struct promise : public object_impl<schedule::thread_pool, queueing::mutexed_call, sharing::disabled>, public active::sink<T>
	{
		typedef T value_type;
		ACTIVE_TEMPLATE(value_type) { m_value.set_value(value_type); }
				
		T get()
		{
			auto fut = m_value.get_future();
			while( !fut.valid() )	// clang calls it valid()
				steal();
			return fut.get();
		}
	private:
		std::promise<value_type> m_value;
	};
} // namespace active


#endif
