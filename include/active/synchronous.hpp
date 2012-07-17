#ifndef ACTIVE_SYNCHRONOUS_INCLUDED
#define ACTIVE_SYNCHRONOUS_INCLUDED

#include "object.hpp"

namespace active
{
	namespace queueing	// The queuing policy classes
	{
		// Ability to wrap object in a mutex. Useful, but not an AO.
		struct mutexed_call
		{
			typedef std::allocator<void> allocator_type;

			mutexed_call(const allocator_type & alloc = allocator_type());
			mutexed_call(const mutexed_call&);

			allocator_type get_allocator() const { return allocator_type(); }

			template<typename Fn>
			bool enqueue_fn( any_object * object, RVALUE_REF(Fn) fn, int )
			{
				platform::lock_guard<platform::recursive_mutex> lock(m_mutex);
				try
				{
					fn();
				}
				catch (...)
				{
					object->exception_handler();
				}
				return false;
			}

			bool run_some(any_object * o, int n=100) throw();

			bool empty() const;

		private:
			platform::recursive_mutex m_mutex;
		};

	}

	typedef object_impl<schedule::none, queueing::mutexed_call, sharing::disabled> synchronous;
}

#endif
