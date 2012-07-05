
#ifndef ACTIVE_THREAD_INCLUDED
#define ACTIVE_THREAD_INCLUDED

#include "object.hpp"
#include <condition_variable>

namespace active
{
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

	typedef object_impl<schedule::own_thread, queueing::shared<>, sharing::disabled> thread;
}

#endif
