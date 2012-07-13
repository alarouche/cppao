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
			
			template<typename Message, typename Accessor>
			bool enqueue(any_object * object, Message && msg, const Accessor&)
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutex);
				try
				{
					Accessor::active_run(object, std::forward<Message&&>(msg));
				}
				catch(...)
				{
					object->exception_handler();
				}
				return false;
			}
			
			template<typename Fn>
			bool enqueue_fn( any_object * object, Fn &&fn, int )
			{
				std::lock_guard<std::recursive_mutex> lock(m_mutex);
				try 
				{
					fn();
				} 
				catch (...) 
				{
					object->exception_handler();
				}
			}
			
			bool run_some(any_object * o, int n=100) throw();
			
			template<typename T>
			struct queue_data
			{
				struct type { };
			};
			
			bool empty() const;
			
		private:
			std::recursive_mutex m_mutex;
		};
		
	}
	
	typedef object_impl<schedule::none, queueing::mutexed_call, sharing::disabled> synchronous;
}

#endif
