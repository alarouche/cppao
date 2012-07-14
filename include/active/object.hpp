/* Cppao: C++ Active Objects library
 * Copyright (C) Calum Grant 2012
 *
 * It uses C++11. In g++, compile with "-std=c++0x -pthread"
 */

#ifndef ACTIVE_OBJECT_INCLUDED
#define ACTIVE_OBJECT_INCLUDED

#include <mutex>
#include <vector>
#include <memory>
#include <thread>

namespace active
{
	template<typename T> int priority(const T&) { return 0; }
	
	// Interface of all active objects.
	struct any_object
	{
		virtual ~any_object();
		virtual void run() throw()=0;
		virtual bool run_some(int n=100) throw()=0;
		virtual void exception_handler() throw();
		virtual void active_fn(std::function<void()>&&fn, int priority)=0;
		any_object * m_next;
	};

	class scheduler;

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
	}


	namespace queueing	// The queuing policy classes
	{
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
			
			template<typename Fn>
			bool enqueue_fn( any_object * obj, Fn &&fn, int )
			{
				typename allocator_type::template rebind<run_impl<Fn>>::other realloc(m_allocator);
				
				run_impl<Fn> * impl = realloc.allocate(1);
				try 
				{
					realloc.construct(impl, std::forward<Fn&&>(fn));
				} 
				catch (...) 
				{
					realloc.deallocate(impl,1);
					throw;
				}
				return enqueue(impl);
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
			
			template<typename Fn>
			struct run_impl : public message
			{
				run_impl(Fn&&fn) : m_fn(std::forward<Fn&&>(fn)) { }
				Fn m_fn;
				void run(any_object*)
				{
					m_fn();
				}
				void destroy(allocator_type&a)
				{
					typename allocator_type::template rebind<run_impl<Fn>>::other realloc(a);
					realloc.destroy(this);
					realloc.deallocate(this,1);
				}
			};

			allocator_type m_allocator;
		protected:
			std::mutex m_mutex;
		};
	}

	template<
		typename Schedule,
		typename Queue,
		typename Share>
	class object_impl : virtual public any_object, public Share::base
	{
	public:
		typedef Schedule schedule_type;
		typedef Share share_type;
		typedef Queue queue_type;
		typedef typename schedule_type::type scheduler_type;
		typedef typename queue_type::allocator_type allocator_type;
		typedef std::size_t size_type;

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
		
		typedef std::function<void()> run_type;
					
		// ?? Public
		template<typename T>
		void enqueue_fn2(T && fn, int priority=0)
		{
			if( m_queue.enqueue_fn(this, std::forward<T&&>(fn), priority))
			{
				m_share.activate(this);
				m_schedule.activate(m_share.pointer(this));
			}
		}
				
		template<typename T>
		void active_fn(T && fn, int priority=0)
		{
			enqueue_fn2( std::forward<T&&>(fn), priority );
		}

		void active_fn(std::function<void()>&&fn, int priority)
		{
			enqueue_fn2( std::forward<std::function<void()>&&>(fn), priority );
		}
		
		template<typename T>
		void active_msg(T && msg, int priority=0)
		{
		}
		
		template<typename T>
		void active_msg(const T & msg, int priority=0)
		{
		}
		

	protected:
		// Clear all pending messages. Only callable from active methods.
		void clear()
		{
			m_queue.clear();
		}

	private:
		schedule_type m_schedule;
		queue_type m_queue;
		share_type m_share;
	};

	// The default object type.
	typedef object_impl<schedule::thread_pool, queueing::shared<>, sharing::disabled> basic;

	template<typename Derived, typename ObjectType=basic>
	struct object : public ObjectType
	{
		typedef ObjectType object_type;
		typedef Derived derived_type;
		using typename ObjectType::scheduler_type;
		using typename ObjectType::allocator_type;
		
		object(scheduler_type & sched = default_scheduler, const allocator_type & alloc = allocator_type()) :
			object_type(sched, alloc)
		{
		}
		
		template<typename T>
		derived_type & operator()(T&&msg)
		{
			this->active_fn( [=]() mutable { static_cast<derived_type*>(this)->active_method(std::move(msg)); } );
			return *static_cast<derived_type*>(this);
		}
		
		template<typename T>
		derived_type & operator()(const T & msg)
		{
			T msg2(msg);
			this->active_fn( [=]() mutable { static_cast<derived_type*>(this)->active_method(std::move(msg2)); } );			
			return *static_cast<derived_type*>(this);
		}

		template<typename T>
		derived_type & operator()(T * msg)
		{
			this->active_fn( [=]() mutable { static_cast<derived_type*>(this)->active_method(std::move(msg)); } );			
			return *static_cast<derived_type*>(this);
		}
		
		template<typename T1, typename T2>
		derived_type & operator()(T1 a1, T2 a2)
		{
			this->active_fn( [=]() mutable { static_cast<derived_type*>(this)->active_method(std::move(a1),std::move(a2)); } );			
			return *static_cast<derived_type*>(this);
		}

		template<typename T1, typename T2, typename T3>
		derived_type & operator()(T1 a1, T2 a2, T3 a3)
		{
			this->active_fn( [=]() mutable { static_cast<derived_type*>(this)->active_method(std::move(a1),std::move(a2),std::move(a3)); } );			
			return *static_cast<derived_type*>(this);
		}
		
		// !! More parameters

#if 0
		// !! Not supported by compiler (yet)
		template<typename... Args>
		derived_type & operator()(Args... args)
		{
			this->active_method( [args...]() mutable { static_cast<derived_type*>(this)->active_method(args...); } );						
			return *static_cast<derived_type*>(this);
		}
#endif
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
		std::vector<std::thread> m_threads;
	};

	template<typename T>
	struct sink : virtual public any_object
	{
		typedef std::shared_ptr<sink<T>> sp;
		typedef std::weak_ptr<sink<T>> wp;
		
		virtual void active_method(T&&)=0;
		
		sink<T> & send(T&&msg, int priority=0)
		{
			this->active_fn( [=]() mutable { this->active_method(std::move(msg)); }, priority );
			return *this;
		}

		sink<T> & send(const T& msg, int priority=0)
		{
			T msg2(msg);
			this->active_fn( [this,msg2]() mutable { this->active_method(std::move(msg2)); }, priority );
			return *this;
		}
	
	};
} // namespace active


#endif
