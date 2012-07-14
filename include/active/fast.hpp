#ifndef ACTIVE_FAST_INCLUDED
#define ACTIVE_FAST_INCLUDED

#include "object.hpp"

namespace active
{
	namespace queueing
	{
		// Not really stealing, but "eager" execution of object in caller thread.
		template<typename Queue>
		struct steal  : public Queue
		{
			typedef typename Queue::allocator_type allocator_type;
			steal(const allocator_type & alloc = allocator_type()) : 
				Queue(alloc), m_running(false)
			{ 
			}
			
			steal(const steal&) : m_running(false) { }
			
			template<typename Fn>
			bool enqueue_fn( any_object * object, Fn && fn, int priority)
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				
				if( m_running )
				{
					return Queue::enqueue_fn( object, std::forward<Fn&&>(fn), priority );
				}
				else
				{
					m_running = true;
					lock.unlock();
					try
					{
						fn();
					}
					catch(...)
					{
						object->exception_handler();
					}
					lock.lock();
					m_running = false;
					return false;
				}
			}
			
			bool empty() const
			{
				return !m_running && Queue::empty();
			}
			
			bool run_some(any_object * o, int n=100) throw()
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				if( m_running ) return true;	// Cannot run at this time
				m_running = true;
				lock.unlock();
				bool activate = Queue::run_some(o,n);
				lock.lock();
				m_running = false;
				return activate;
			}
			
		private:
			std::mutex m_mutex;
			bool m_running;
		};
		
	}
	
	typedef object_impl<schedule::thread_pool, queueing::steal<queueing::shared<>>, sharing::disabled> fast;
}


#endif
