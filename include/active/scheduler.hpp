#ifndef ACTIVE_SCHEDULER_INCLUDED
#define ACTIVE_SCHEDULER_INCLUDED

#include "object.hpp"

namespace active
{
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
}

#endif
