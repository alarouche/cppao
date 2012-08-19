/* Cppao: C++ Active Objects library
 * Copyright (C) Calum Grant 2012
 */

#ifndef ACTIVE_OBJECT_INCLUDED
#define ACTIVE_OBJECT_INCLUDED

#include <active/config.hpp>
#include "fifo.hpp"

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
	#include <vector>

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

	namespace policy
	{
		enum queue_full { ignore, block, discard, fail };
	}

	// Interface of all active objects.
	struct any_object
	{
		virtual ~any_object();
		virtual void run() throw()=0;
		virtual bool run_some(int n=100) throw()=0;
		virtual void exception_handler() throw();
		virtual bool idle() throw()=0;
		any_object * m_next;
	};

	class scheduler;
	bool idle(scheduler & sched) throw();

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
			type & get_scheduler() const { return default_scheduler; }
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
			   virtual ~message() { }
			   virtual void run()=0;
		   };
		public:

			shared(const allocator_type & alloc = allocator_type()) :
				m_queue(alloc)
			{
			}

			shared(const shared&o) : m_queue(o.m_queue.get_allocator())
			{
			}

			allocator_type get_allocator() const { return m_queue.get_allocator(); }

			template<typename Fn>
			bool enqueue_fn( any_object * obj, RVALUE_REF(Fn) fn, int )
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				m_queue.push( run_impl<Fn>(platform::forward<RVALUE_REF(Fn)>(fn)) );
				return m_queue.size()==1;
			}

			bool empty() const
			{
				return m_queue.empty();
			}

			bool mutexed_empty() const
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				return m_queue.size()<=1;
			}

			bool run_some(any_object * o, int n=100) throw()
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				while( !m_queue.empty() && n-->0)
				{
					message & m = m_queue.front();

					m_mutex.unlock();
					try
					{
						m.run();
					}
					catch (...)
					{
						o->exception_handler();
					}
					m_mutex.lock();
					m_queue.pop();
				}
				return !m_queue.empty();
			}

			void clear()
			{
				// Destroy all message except current.
				platform::lock_guard<platform::mutex> lock(m_mutex);
				m_queue.truncate();
			}

		private:

			template<typename Fn>
			struct run_impl : public message
			{
				run_impl(RVALUE_REF(Fn)fn) : m_fn(platform::forward<RVALUE_REF(Fn)>(fn)) { }
				Fn m_fn;
				void run()
				{
					m_fn();
				}
			};

			fifo<message, typename allocator_type::template rebind<message>::other> m_queue;
		protected:
			mutable platform::mutex m_mutex;
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
		typedef std::size_t size_type;

		object_impl(scheduler_type & tp = default_scheduler,
					const allocator_type & alloc = allocator_type())
					: m_schedule(tp), m_queue(alloc)
		{
		}

		~object_impl()
		{
			if(!m_queue.empty()) std::terminate();
		}

		allocator_type get_allocator() const
		{
			return m_queue.get_allocator();
		}

		void set_scheduler(scheduler_type & sched)
		{
			m_schedule.set_scheduler(sched);
		}

		scheduler_type & get_scheduler() const
		{
			return m_schedule.get_scheduler();
		}

		void set_capacity(size_type c)
		{
			m_queue.set_capacity(c);
		}

		void set_queue_policy(policy::queue_full p)
		{
			m_queue.set_policy(p);
		}

		size_type get_capacity() const
		{
			return m_queue.get_capacity();
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

		bool idle() throw()
		{
			return active::idle(get_scheduler());
		}

	private:
		void run() throw()
		{
			typename share_type::pointer_type p=typename share_type::pointer_type();
			m_share.deactivate(p);

			while( m_queue.run_some(this) )
				;
		}

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

		template<typename T>
		void enqueue_fn2(RVALUE_REF(T) fn, int priority=0)
		{
			if( m_queue.enqueue_fn(this, platform::forward<RVALUE_REF(T)>(fn), priority))
			{
				m_share.activate(this);
				m_schedule.activate(m_share.pointer(this));
			}
		}

	protected:

		template<typename T>
		void active_fn(RVALUE_REF(T) fn, int priority=0) const
		{
			const_cast<object_impl*>(this)->enqueue_fn2( platform::forward<RVALUE_REF(T)>(fn), priority );
		}

		template<typename T>
		void active_fn(RVALUE_REF(T) fn, int priority=0)
		{
			enqueue_fn2( platform::forward<RVALUE_REF(T)>(fn), priority );
		}

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

	/*	This is the base class of all active objects.
		Its main role is to implement operator().
	  */
	template<typename Derived, typename ObjectType=basic>
	class object : public ObjectType
	{
	public:
		typedef ObjectType object_type;
		typedef Derived derived_type;
		typedef typename ObjectType::scheduler_type scheduler_type;
		typedef typename ObjectType::allocator_type allocator_type;

		object(scheduler_type & sched = default_scheduler, const allocator_type & alloc = allocator_type()) :
			object_type(sched, alloc)
		{
		}

	private:
		derived_type & get_derived()
		{
			return *static_cast<derived_type*>(this);
		}

		const derived_type & get_derived() const
		{
			return *static_cast<const derived_type*>(this);
		}

#ifdef ACTIVE_USE_VARIADIC_TEMPLATES
		template<typename... Args>
		void run_active_method(Args ...args)
		{
			get_derived().active_method(args...);
		}

		template<typename... Args>
		void const_run_active_method(Args ...args) const
		{
			get_derived().active_method(args...);
		}
#endif

		template<typename Arg1>
		void run_active_method(Arg1 a1)
		{
			get_derived().active_method(active::platform::move(a1));
		}

		void run_active_method0()
		{
			get_derived().active_method();
		}

		void const_run_active_method0() const
		{
			get_derived().active_method();
		}

		template<typename Arg1, typename Arg2>
		void run_active_method(Arg1 a1, Arg2 a2)
		{
			get_derived().active_method(active::platform::move(a1), active::platform::move(a2));
		}

		template<typename Arg1, typename Arg2, typename Arg3>
		void run_active_method(Arg1 a1, Arg2 a2, Arg3 a3)
		{
			get_derived().active_method(active::platform::move(a1), active::platform::move(a2), active::platform::move(a3));
		}

		template<typename Arg1, typename Arg2, typename Arg3, typename Arg4>
		void run_active_method(Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4)
		{
			get_derived().active_method(active::platform::move(a1), active::platform::move(a2), active::platform::move(a3), active::platform::move(a4));
		}

		template<typename Arg1, typename Arg2, typename Arg3, typename Arg4>
		void const_run_active_method(Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4) const
		{
			get_derived().active_method(active::platform::move(a1), active::platform::move(a2), active::platform::move(a3), active::platform::move(a4));
		}

		template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
		void run_active_method(Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4, Arg5 a5)
		{
			get_derived().active_method(active::platform::move(a1), active::platform::move(a2), active::platform::move(a3), active::platform::move(a4), active::platform::move(a5));
		}

		template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
		void const_run_active_method(Arg1 a1, Arg2 a2, Arg3 a3, Arg4 a4, Arg5 a5) const
		{
			get_derived().active_method(active::platform::move(a1), active::platform::move(a2), active::platform::move(a3), active::platform::move(a4), active::platform::move(a5));
		}

		template<typename Arg1>
		void const_run_active_method(Arg1 a1) const
		{
			get_derived().active_method(active::platform::move(a1));
		}

		template<typename Arg1, typename Arg2>
		void const_run_active_method(Arg1 a1, Arg2 a2) const
		{
			get_derived().active_method(active::platform::move(a1), active::platform::move(a2));
		}

		template<typename Arg1, typename Arg2, typename Arg3>
		void const_run_active_method(Arg1 a1, Arg2 a2, Arg3 a3) const
		{
			get_derived().active_method(active::platform::move(a1), active::platform::move(a2), active::platform::move(a3));
		}

	public:
		derived_type & operator()()
		{
			this->active_fn( platform::bind(&object::run_active_method0,this),0);
			return get_derived();
		}

		const derived_type & operator()() const
		{
			this->active_fn( platform::bind(&object::const_run_active_method0,this),0);
			return get_derived();
		}

#ifdef ACTIVE_USE_VARIADIC_TEMPLATES
		template<typename Arg1, typename... Args>
		derived_type & operator()(Arg1 arg1, Args ...args)
		{
			this->active_fn( platform::bind(&object::run_active_method<Arg1,Args...>,
											this,arg1,args...),priority(arg1));
			return get_derived();
		}

		template<typename Arg1, typename... Args>
		const derived_type & operator()(Arg1 arg1, Args ...args) const
		{
			this->active_fn( platform::bind(&object::const_run_active_method<Arg1,Args...>,
											this,arg1,args...),priority(arg1));
			return get_derived();
		}
#else
		template<typename T>
		derived_type & operator()(const T msg)
		{
			this->active_fn( platform::bind( &object::run_active_method<T>, this, msg), priority(msg));
			return get_derived();
		}

		template<typename T>
		const derived_type & operator()(const T msg) const
		{
			this->active_fn( platform::bind( &object::const_run_active_method<T>, this, msg), priority(msg));
			return get_derived();
		}

		template<typename T1,typename T2>
		derived_type & operator()(T1 a1, T2 a2)
		{
			this->active_fn( platform::bind( &object::run_active_method<T1,T2>, this, a1, a2), priority(a1));
			return get_derived();
		}

		template<typename T1,typename T2>
		const derived_type & operator()(T1 a1, T2 a2) const
		{
			this->active_fn( platform::bind( &object::const_run_active_method<T1,T2>, this, a1, a2), priority(a1));
			return get_derived();
		}

		template<typename T1,typename T2,typename T3>
		derived_type & operator()(T1 a1, T2 a2, T3 a3)
		{
			this->active_fn( platform::bind( &object::run_active_method<T1,T2,T3>, this, a1, a2, a3), priority(a1));
			return get_derived();
		}

		template<typename T1,typename T2,typename T3>
		const derived_type & operator()(T1 a1, T2 a2, T3 a3) const
		{
			this->active_fn( platform::bind( &object::const_run_active_method<T1,T2,T3>, this, a1, a2, a3), priority(a1));
			return get_derived();
		}

		template<typename T1,typename T2,typename T3, typename T4>
		derived_type & operator()(T1 a1, T2 a2, T3 a3, T4 a4)
		{
			this->active_fn( platform::bind( &object::run_active_method<T1,T2,T3,T4>, this, a1, a2, a3, a4), priority(a1));
			return get_derived();
		}

		template<typename T1,typename T2,typename T3,typename T4>
		const derived_type & operator()(T1 a1, T2 a2, T3 a3,T4 a4) const
		{
			this->active_fn( platform::bind( &object::const_run_active_method<T1,T2,T3,T4>, this, a1, a2, a3, a4), priority(a1));
			return get_derived();
		}

		template<typename T1,typename T2,typename T3, typename T4, typename T5>
		derived_type & operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
		{
#if defined(ACTIVE_USE_BOOST) || !defined(_MSC_VER)
			this->active_fn( platform::bind( &object::run_active_method<T1,T2,T3,T4,T5>, this, a1, a2, a3, a4, a5), priority(a1));
#else
			// Ugly: For some reason, MSVC bind() suddenly stops working at 5 arguments
			// Neither does it support variadic templates.
			// Honestly, you'd think for a beta compiler, they would actually implement some of C++11.
			this->active_fn( [=]() mutable {
				run_active_method(std::move(a1),std::move(a2),std::move(a3),std::move(a4),std::move(a5));
			}, priority(a1));
#endif
			return get_derived();
		}

		template<typename T1,typename T2,typename T3,typename T4, typename T5>
		const derived_type & operator()(T1 a1, T2 a2, T3 a3,T4 a4, T5 a5) const
		{
#if defined(ACTIVE_USE_BOOST) || !defined(_MSC_VER)
			this->active_fn( platform::bind( &object::const_run_active_method<T1,T2,T3,T4,T5>, this, a1, a2, a3, a4, a5), priority(a1));
#else
			// Ugly: For some reason, MSVC bind() suddenly stops working at 5 arguments
			this->active_fn( [=]() mutable {
				const_run_active_method(std::move(a1),std::move(a2),std::move(a3),std::move(a4),std::move(a5));
			}, priority(a1));
#endif
			return get_derived();
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
	struct sink
	{
		typedef platform::shared_ptr<sink<T> > sp;
		typedef platform::weak_ptr<sink<T> > wp;
		typedef T value_type;
		virtual void send(value_type)=0;
	};

	template<typename Derived, typename T>
	struct handle : public sink<T>
	{
		void send(T value) { static_cast<Derived&>(*this)(value); }
	};
} // namespace active


#endif
