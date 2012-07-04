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
			
			template<typename Message, typename Accessor>
			bool enqueue( any_object * object, Message && msg, const Accessor& accessor)
			{
				this->m_mutex.lock();
				if( m_running || !this->empty())
				{
					this->m_mutex.unlock();	// ?? Better use of locks; also exception safety.
					Queue::enqueue( object, std::forward<Message&&>(msg), accessor );
					return false;
				}
				else
				{
					m_running = true;
					this->m_mutex.unlock();
					try
					{
						Accessor::active_run(object, std::forward<Message&&>(msg));
					}
					catch(...)
					{
						object->exception_handler();
					}
					this->m_mutex.lock();
					m_running = false;
					bool activate = !this->empty();
					this->m_mutex.unlock();
					return activate;
				}
			}
		private:
			bool m_running;
		};
		
	}
	
	typedef object_impl<schedule::thread_pool, queueing::steal<queueing::shared<>>, sharing::disabled> fast;
}


#endif
