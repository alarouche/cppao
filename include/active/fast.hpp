#ifndef ACTIVE_FAST_INCLUDED
#define ACTIVE_FAST_INCLUDED

#include "object.hpp"

namespace active
{
	namespace queueing
	{
		// Executes the call in the caller thread if possible.		
		template<typename Queue>
		struct eager  : public Queue
		{
			typedef typename Queue::allocator_type allocator_type;
			eager(const allocator_type & alloc = allocator_type()) :
				Queue(alloc)
			{
			}

			eager(const eager&) { }

			template<typename Fn>
			bool enqueue_fn( any_object * object, RVALUE_REF(Fn) fn, int priority)
			{
				platform::unique_lock<platform::mutex> lock(m_mutex, platform::try_to_lock);
				
				if( lock.owns_lock() )
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
				else
				{
					return Queue::enqueue_fn( object, platform::forward<RVALUE_REF(Fn)>(fn), priority );
				}
			}

			bool empty() const
			{
				platform::unique_lock<platform::mutex> lock(m_mutex, platform::try_to_lock);
				return lock.owns_lock() && Queue::empty();
			}

			bool run_some(any_object * o, int n=100) throw()
			{
				platform::unique_lock<platform::mutex> lock(m_mutex, platform::try_to_lock);
				return !lock.owns_lock() || Queue::run_some(o,n);
			}

		private:
			mutable platform::mutex m_mutex;
		};

	}

	typedef object_impl<
		schedule::thread_pool,
		queueing::eager<queueing::shared<> >,
		sharing::disabled> fast;
}


#endif
