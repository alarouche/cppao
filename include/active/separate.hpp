
#ifndef ACTIVE_SEPARATE_INCLUDED
#define ACTIVE_SEPARATE_INCLUDED

#include "object.hpp"
#include <deque>

namespace active
{
	namespace queueing
	{
		// A different implementation, probably safe to ignore.
		template<typename Allocator=std::allocator<void>>
		struct separate
		{
			typedef Allocator allocator_type;
			
			separate(const allocator_type & alloc=allocator_type()) :
			m_message_queue(alloc)
			{
			}
			
			allocator_type get_allocator() const { return m_message_queue.get_allocator(); }
			
			separate(const separate&o) :
			m_message_queue(o.m_message_queue.get_allocator())
			{
			}
			
			template<typename T>
			struct queue_data
			{
				typedef std::deque<T, typename allocator_type::template rebind<T>::other> type;
			};
			
			template<typename Message, typename Accessor>
			bool enqueue( any_object * object, Message && msg, const Accessor&)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_message_queue.push_back( [object](){ run<Message, Accessor>(object);} );
				try
				{
					Accessor::active_data(object).push_back(std::forward<Message&&>(msg));
				}
				catch(...)
				{
					m_message_queue.pop_back();
					throw;
				}
				return m_message_queue.size()==1;
			}
			
			template<typename Fn>
			bool enqueue_fn(any_object * object, Fn&&fn, int)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_message_queue.push_back(fn);
				return m_message_queue.size()==1;
			}
			
			bool empty() const
			{
				return m_message_queue.empty();
			}
			
			bool run_some(any_object * o, int n=100) throw()
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				
				while( !m_message_queue.empty() && n-->0 )
				{
					m_mutex.unlock();
					try
					{
						std::function<void()> run = std::move(m_message_queue.front());
						run();
					}
					catch (...)
					{
						o->exception_handler();
					}
					m_mutex.lock();
					m_message_queue.pop_front();
				}
				return !m_message_queue.empty();
			}
			
		protected:
			std::mutex m_mutex;
			
		private:
			typedef void (*ActiveRun)(any_object*);
			// std::deque<ActiveRun, typename allocator_type::template rebind<ActiveRun>::other> m_message_queue;
			std::deque<std::function<void()>, typename allocator_type::template rebind<std::function<void()>>::other> m_message_queue;
			
			template<typename Message, typename Accessor>
			static void run(any_object*obj)
			{
				Message m = std::move(Accessor::active_data(obj).front());
				Accessor::active_data(obj).pop_front();
				Accessor::active_run(obj, std::move(m));
			}
		};
	}
	
	typedef object_impl<schedule::thread_pool, queueing::separate<>, sharing::disabled> separate;
}

#endif