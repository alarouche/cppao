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
			
			template<typename Message, typename Accessor>
			bool enqueue( any_object * object, Message && msg, const Accessor& accessor)
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				
				if( m_running )
				{
					return Queue::enqueue( object, std::forward<Message&&>(msg), accessor );
				}
				else
				{
					m_running = true;
					lock.unlock();
					try
					{
						Accessor::active_run(object, std::forward<Message&&>(msg));
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
