/* Cppao: C++ Active Objects library
 * Copyright (C) Calum Grant 2012
 */

#ifndef ACTIVE_OBJECT_INCLUDED
#define ACTIVE_OBJECT_INCLUDED

#include <active/config.hpp>

#include <vector>
#include <memory>

#ifdef ACTIVE_USE_CXX11
	#define RVALUE_REF(T) T&&
	namespace active
	{
		namespace platform
		{
			using std::forward;
			using std::move;
		}
	}
#else
	#define RVALUE_REF(T) const T&
	namespace active
	{
		namespace platform
		{
			template<typename T>
			const T & forward(const T&v) { return v; }
			template<typename T>
			const T & move(const T&v) { return v; }
		}
		const int nullptr=0;
	}
#endif

#ifdef ACTIVE_USE_BOOST
	#include <boost/thread.hpp>
	#include <boost/thread/mutex.hpp>
	#include <boost/shared_ptr.hpp>
	#include <boost/bind.hpp>

	namespace active
	{
		namespace platform
		{
			using namespace boost;
		}
	}
#else
	#include <mutex>
	#include <thread>
	namespace active
	{
		namespace platform
		{
			using namespace std;
		}
	}
#endif



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
		virtual void active_fn(RVALUE_REF(platform::function<void()>) fn, int priority)=0;
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
			void activate(const platform::shared_ptr<any_object> & sp) { }
			void activate(any_object * obj) { }
		};

		// The object is scheduled using the thread pool (active::pool)
		struct thread_pool
		{
			typedef active::scheduler type;
			thread_pool(type&);

			void set_scheduler(type&p);
			void activate(const platform::shared_ptr<any_object> & sp);
			void activate(any_object * obj);
			type & get_scheduler() const { return *m_pool; }
		private:
			type * m_pool;
		};
	}


	namespace queueing	// The queuing policy classes
	{
		// Default message queue shared between all message types.
		template< typename Allocator=std::allocator<void> >
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
			bool enqueue_fn( any_object * obj, RVALUE_REF(Fn) fn, int )
			{
				typename allocator_type::template rebind<run_impl<Fn> >::other realloc(m_allocator);

				run_impl<Fn> * impl = realloc.allocate(1);
				try
				{
					realloc.construct(impl, platform::forward<RVALUE_REF(Fn)>(fn));
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
				platform::lock_guard<platform::mutex> lock(m_mutex);
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
				 platform::lock_guard<platform::mutex> lock(m_mutex);
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
				run_impl(RVALUE_REF(Fn)fn) : m_fn(platform::forward<RVALUE_REF(Fn)>(fn)) { }
				Fn m_fn;
				void run(any_object*)
				{
					m_fn();
				}
				void destroy(allocator_type&a)
				{
					typename allocator_type::template rebind<run_impl<Fn> >::other realloc(a);
					realloc.destroy(this);
					realloc.deallocate(this,1);
				}
			};

			allocator_type m_allocator;
		protected:
			platform::mutex m_mutex;
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
			typename share_type::pointer_type p=typename share_type::pointer_type();
			m_share.deactivate(p);

			while( m_queue.run_some(this) )
				;
		}

		// Not threadsafe - do not call unless you know what you are doing.
		bool run_some(int n) throw()
		{
			typename share_type::pointer_type p=typename share_type::pointer_type();	// !! Poor
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

		// ?? Public
		template<typename T>
		void enqueue_fn2(RVALUE_REF(T) fn, int priority=0)
		{
			if( m_queue.enqueue_fn(this, platform::forward<RVALUE_REF(T)>(fn), priority))
			{
				m_share.activate(this);
				m_schedule.activate(m_share.pointer(this));
			}
		}

		template<typename T>
		void active_fn(RVALUE_REF(T) fn, int priority=0)
		{
			enqueue_fn2( platform::forward<RVALUE_REF(T)>(fn), priority );
		}

		void active_fn(RVALUE_REF(platform::function<void()>) fn, int priority)
		{
			enqueue_fn2( platform::forward<RVALUE_REF(platform::function<void()>)>(fn), priority );
		}

		// !! Not implemented
		template<typename T>
		void active_msg(RVALUE_REF(T) msg, int priority=0)
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
		typedef typename ObjectType::scheduler_type scheduler_type;
		typedef typename ObjectType::allocator_type allocator_type;

		object(scheduler_type & sched = default_scheduler, const allocator_type & alloc = allocator_type()) :
			object_type(sched, alloc)
		{
		}

#ifdef ACTIVE_USE_CXX11
		template<typename T>
		derived_type & operator()(RVALUE_REF(T)msg)
		{
			this->active_fn( [=]() mutable { static_cast<derived_type*>(this)->active_method(std::move(msg)); }, priority(msg) );
			return *static_cast<derived_type*>(this);
		}

		template<typename T>
		derived_type & operator()(const T & msg)
		{
			T msg2(msg);
			this->active_fn( [=]() mutable { static_cast<derived_type*>(this)->active_method(std::move(msg2)); }, priority(msg) );
			return *static_cast<derived_type*>(this);
		}

		template<typename T>
		derived_type & operator()(T * msg)
		{
			this->active_fn( [=]() mutable { static_cast<derived_type*>(this)->active_method(std::move(msg)); }, priority(msg) );
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
#else
	private:
		template<typename T1>
		static void run1(derived_type *p, const T1 a1)
		{
			p->active_method(a1);
		}

		template<typename T1, typename T2>
		static void run2(derived_type *p, T1 a1, T2 a2)
		{
			p->active_method(a1,a2);
		}

		template<typename T1, typename T2, typename T3>
		static void run3(derived_type *p, T1 a1, T2 a2, T3 a3)
		{
			p->active_method(a1,a2,a3);
		}
	public:

		template<typename T>
		derived_type & operator()(const T msg)
		{
			this->active_fn( platform::bind( &run1<T>, static_cast<derived_type*>(this), msg), priority(msg));
			return *static_cast<derived_type*>(this);
		}

		template<typename T1,typename T2>
		derived_type & operator()(T1 a1, T2 a2)
		{
			this->active_fn( platform::bind( &run2<T1,T2>, static_cast<derived_type*>(this), a1, a2), priority(a1));
			return *static_cast<derived_type*>(this);
		}

		template<typename T1,typename T2,typename T3>
		derived_type & operator()(T1 a1, T2 a2, T3 a3)
		{
			this->active_fn( platform::bind( &run3<T1,T2,T3>, static_cast<derived_type*>(this), a1, a2, a3), priority(a1));
			return *static_cast<derived_type*>(this);
		}
		// !! More parameters
#endif


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
		explicit run(int threads=platform::thread::hardware_concurrency(), scheduler & sched = default_scheduler);
		~run();
	private:
		run(const run&);
		run & operator=(const run&);
		scheduler & m_scheduler;
#ifdef ACTIVE_USE_BOOST
		typedef boost::thread_group threads;
#else
		typedef std::vector<platform::thread> threads;
#endif
		threads m_threads;
	};

	template<typename T>
	struct sink : virtual public any_object
	{
		typedef platform::shared_ptr<sink<T> > sp;
		typedef platform::weak_ptr<sink<T> > wp;

		virtual void active_method(T)=0;

#ifdef ACTIVE_USE_CXX11
		sink<T> & send(RVALUE_REF(T)msg)
		{
			this->active_fn( [=]() mutable { this->active_method(platform::move(msg)); }, priority(msg) );
			return *this;
		}

		sink<T> & send(const T& msg)
		{
			T msg2(msg);
			this->active_fn( [this,msg2]() mutable { this->active_method(platform::move(msg2)); }, priority(msg) );
			return *this;
		}
#else

		sink<T> & send(const T&msg)
		{
			this->active_fn(platform::bind(&sink<T>::run_msg, this, msg),priority(msg));
			return *this;
		}
#endif
	private:
		static void run_msg(sink *s, T msg) { s->active_method(msg); }
	};
} // namespace active


#endif
