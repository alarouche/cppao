
#ifndef ACTIVE_THREAD_INCLUDED
#define ACTIVE_THREAD_INCLUDED

#include "object.hpp"

namespace active
{
	namespace sharing
	{
		template<typename T> struct enabled;
	}
	
	namespace schedule
	{
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
			std::shared_ptr<any_object> m_shared;
			std::mutex m_mutex;
			std::condition_variable m_ready;
			std::thread m_thread;
		};
	}
	
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

	typedef object_impl<schedule::own_thread, queueing::shared<>, sharing::disabled> thread;	
}

#endif
