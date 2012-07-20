
#ifndef ACTIVE_ADVANCED_INCLUDED
#define ACTIVE_ADVANCED_INCLUDED

#include "object.hpp"
#include <queue>

namespace active
{
	struct prioritize
	{
		template<typename T>
		int operator()(const T&v) const { return priority(v); }
	};

	namespace queueing
	{
		// Message queue offering cancellation, prioritization and capacity.
		template<typename Allocator=std::allocator<void>, typename Priority=prioritize>
		class advanced
		{
		public:
			typedef Allocator allocator_type;
			typedef Priority priority_type;

		private:
			struct message
			{
				message(int p, std::size_t s) : m_priority(p), m_sequence(s) { }
				virtual void run(any_object * obj)=0;
				virtual void destroy(allocator_type&)=0;
				const int m_priority;
				const std::size_t m_sequence;
			};

			struct msg_cmp;
			typedef std::vector<message*, typename allocator_type::template rebind<message*>::other> vector_type;
			typedef std::priority_queue<message*, vector_type, msg_cmp> queue_type;

		public:

			advanced(const allocator_type & alloc = allocator_type(),
					 std::size_t capacity=1000,
					 const priority_type p = priority_type()) :
				m_priority(p), m_allocator(alloc), m_messages(msg_cmp(), vector_type(alloc)),
				m_capacity(capacity), m_busy(false), m_sequence(0)
			{
			}

			advanced(const advanced&o) :
				m_priority(o.m_priority), m_allocator(o.m_allocator),
				m_messages(msg_cmp(), vector_type(o.m_allocator)),
				m_capacity(o.m_capacity), m_busy(false), m_sequence(0)
			{
			}

			allocator_type get_allocator() const { return m_allocator; }

			template<typename Fn>
			bool enqueue_fn(any_object *o, RVALUE_REF(Fn)fn, int priority)
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				if( m_messages.size() >= m_capacity ) throw std::bad_alloc();

				typename allocator_type::template rebind<fn_impl<Fn> >::other realloc(m_allocator);

				fn_impl<Fn> * impl = realloc.allocate(1);

				try
				{
					realloc.construct(impl, fn_impl<Fn>(platform::forward<RVALUE_REF(Fn)>(fn), priority, m_sequence++)  );
				}
				catch(...)
				{
					realloc.deallocate(impl,1);
					throw;
				}

				try
				{
					return enqueue( impl );
				}
				catch(...)
				{
					impl->destroy(m_allocator);
					throw;
				}
			}

			bool empty() const
			{
				return !m_busy && m_messages.empty();
			}

			bool mutexed_empty() const
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				return m_messages.empty(); // Note: different from empty();
			}

			void set_capacity(std::size_t new_capacity)
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				m_capacity = new_capacity;
			}

			std::size_t get_capacity() const
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				return m_capacity;
			}

			int get_priority() const
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				return m_messages.empty() ? 0 : m_messages.top()->m_priority;
			}

			bool run_some(any_object * o, int n=100) throw()
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				while( !m_messages.empty() && n-->0)
				{
					message * m = m_messages.top();
					m_messages.pop();
					m_busy = true;

					m_mutex.unlock();
					try
					{
						m->run(o);
					}
					catch (...)
					{
						o->exception_handler();
					}
					m_mutex.lock();
					m_busy = false;
					m->destroy(m_allocator);
				}
				return !m_messages.empty();
			}

			void clear()
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				while( !m_messages.empty() )
				{
					m_messages.top()->destroy(m_allocator);
					m_messages.pop();
				}
			}

			std::size_t size() const
			{
				platform::lock_guard<platform::mutex> lock(m_mutex);
				return m_messages.size();
			}

		private:

			struct msg_cmp
			{
				bool operator()(message *m1, message *m2) const
				{
					return m1->m_priority < m2->m_priority || (m1->m_priority == m2->m_priority && m1->m_sequence > m2->m_sequence);
				}
			};

			bool enqueue(message*impl)
			{
				bool first_one = m_messages.empty() && !m_busy;
				m_messages.push(impl);
				return first_one;
			}

			template<typename Fn>
			struct fn_impl : public message
			{
				fn_impl(RVALUE_REF(Fn)fn, int priority, std::size_t seq) :
					message(priority, seq),
					m_fn(platform::forward<RVALUE_REF(Fn)>(fn))
				{
				}
				Fn m_fn;
				void run(any_object *)
				{
					m_fn();
				}
				void destroy(allocator_type&a)
				{
					typename allocator_type::template rebind<fn_impl<Fn> >::other realloc(a);
					realloc.destroy(this);
					realloc.deallocate(this,1);
				}
			};

			priority_type m_priority;
			allocator_type m_allocator;
			queue_type m_messages;
			std::size_t m_capacity;
			bool m_busy;
			std::size_t m_sequence;

		protected:
			mutable platform::mutex m_mutex;
		};
	}

	typedef object_impl<schedule::thread_pool, queueing::advanced<>, sharing::disabled> advanced;
}

#endif
