#ifndef ACTIVE_DIRECT_INCLUDED
#define ACTIVE_DIRECT_INCLUDED

#include "object.hpp"

namespace active
{
	namespace queueing	// The queuing policy classes
	{
		// No queueing; run in calling thread without protection.
		struct direct_call
		{
			typedef std::allocator<void> allocator_type;

			direct_call(const allocator_type& =allocator_type());

			allocator_type get_allocator() const { return allocator_type(); }

			template<typename Fn>
			bool enqueue_fn(any_object * object, RVALUE_REF(Fn) fn, int)
			{
				try
				{
					fn();
				}
				catch(...)
				{
					object->exception_handler();
				}
				return false;
			}

			bool run_some(any_object * o, int n=100) throw();
			bool empty() const;
			bool mutexed_empty() const;
			void clear();
		};
	}

	typedef object_impl<schedule::none, queueing::direct_call, sharing::disabled> direct;
}

#endif
