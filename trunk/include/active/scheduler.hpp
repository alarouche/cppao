#ifndef ACTIVE_SCHEDULER_INCLUDED
#define ACTIVE_SCHEDULER_INCLUDED

#include "object.hpp"

#ifdef ACTIVE_USE_BOOST
	#include <boost/thread/condition_variable.hpp>
	namespace active
	{
		namespace platform
		{
			using boost::condition_variable;
		}
	}
#else
	#include <condition_variable>
	namespace active
	{
		namespace platform
		{
			using std::condition_variable;
		}
	}
#endif

#ifdef ACTIVE_USE_CXX11
	#include "atomic_fifo.hpp"
#endif

namespace active
{
	// Represents a pool of active objects which can be executed in a thread pool.
	class scheduler
	{
	public:
		typedef any_object * ObjectPtr;

		scheduler();

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
		platform::mutex m_mutex;
		platform::condition_variable m_ready;
#ifdef ACTIVE_USE_CXX11
		atomic_fifo m_activated_objects;
		std::atomic<int> m_busy_count;
#else
		any_object * m_head;
		int m_busy_count;	// Used to work out when we have actually finished.
#endif

		bool run_managed() throw();
		void run_in_thread();
		bool locked_run_one();
	};
}

#endif
