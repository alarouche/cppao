#include <memory>

namespace active
{
	/*	Specialised container for use by cppao to store a message queue.
		A stable polymorphic deque.
		This could probably be expanded into a proper container, but for now
		the bare minimum is implemented to support cppao.
		This does not implement alignment 100% portably. */

	template<typename T, typename Alloc=std::allocator<T> >
	class fifo
	{
	public:
		typedef Alloc allocator_type;
		typedef typename allocator_type::size_type size_type;
		typedef T value_type;

		fifo(const allocator_type & a = allocator_type()) :
			m_allocator(a), m_size(0), m_head(0), m_tail(0) { }

		~fifo()
		{
			clear();
			if(m_head) erase(m_head);
		}

		void reserve(size_type c)
		{
			if( capacity() < c+aligned_size<entry>::value )
			{
				if( m_head && m_head->empty () )
				{
					erase(m_head);
					m_head = m_tail = 0;
				}

				add_chain(std::max(c+aligned_size<entry>::value,200UL));	// !! Where to get this value from ?
			}
		}

		// !! && version

		template<typename U>
		void push(const U&u)
		{
			reserve(aligned_size<U>::value);
			m_tail->push(u, m_allocator);
			++m_size;
		}

#ifdef ACTIVE_USE_CXX11
		template<typename U>
		void push(U&&u)
		{
			reserve(aligned_size<U>::value);
			m_tail->push(std::forward<U&&>(u), m_allocator);
			++m_size;
		}
#endif

		value_type & front()
		{
			return m_head->front();
		}

		const value_type & front() const
		{
			return m_head->front();
		}

		void pop()
		{
			m_allocator.destroy( &front() );
			m_head->pop();

			if( m_head->empty() )
			{
				if( m_head->m_next )
				{
					chain * new_head = m_head->m_next;
					erase(m_head);
					m_head = new_head;
					if( !m_head ) m_tail=0;
				}
				else
				{
					m_head->reset();
				}
			}
			--m_size;
		}

		size_type size() const
		{
			return m_size;
		}

		void clear()
		{
			while(!empty()) pop();
		}

		// Pop everything except first item (which must exist)
		void truncate()
		{
			chain * front_chain = m_head;
			char * front_item = m_head->m_front;
			m_head->pop();
			while( !m_head->empty() )
			{
				m_allocator.destroy(&m_head->front());
				m_head->pop();
			}
			m_head=m_head->m_next;
			while( m_head )
			{
				while( !m_head->empty() )
				{
					m_allocator.destroy(&m_head->front());
					m_head->pop();
				}
				chain * old_head = m_head;
				m_head=m_head->m_next;
				erase(old_head);
			}

			front_chain->m_next=0;
			front_chain->m_front = front_item;
			front_chain->m_back = reinterpret_cast<entry*>(front_item)->m_next;
			m_head = m_tail = front_chain;
			m_size=1;
		}

		bool empty() const { return !m_head || m_head->empty(); }

		allocator_type get_allocator() const { return m_allocator; }

		void swap(fifo&other)
		{
			std::swap( m_head, other.m_head );
			std::swap( m_tail, other.m_tail );
			std::swap( m_size, other.m_size );
			std::swap( m_allocator, other.m_allocator );
		}

	private:

		// Helper to *try* to ensure that all all allocated objects are aligned properly.
		template<typename U, size_type alignment=8>
		struct aligned_size
		{
			static const size_type value =
					sizeof(U)%alignment ? sizeof(U)-(sizeof(U)%alignment)+alignment : sizeof(U);
		};

		struct entry
		{
			char * m_next;
		};

		struct chain
		{
			char *m_front, *m_back, *m_end;
			chain *m_next;

			size_type capacity() const { return m_end-m_back; }

			template<typename U>
			void push(const U & u, allocator_type & allocator)
			{
				entry * e = reinterpret_cast<entry*>(m_back);
				U * data = reinterpret_cast<U*>(m_back+aligned_size<entry>::value);
				typename allocator_type::template rebind<U>::other alloc(allocator);
				alloc.construct(data, u);
				m_back = (char*)data+aligned_size<U>::value;
				e->m_next = m_back;
			}

#ifdef ACTIVE_USE_CXX11
			template<typename U>
			void push(U && u, allocator_type & allocator)
			{
				entry * e = reinterpret_cast<entry*>(m_back);
				U * data = reinterpret_cast<U*>(m_back+aligned_size<entry>::value);
				typename allocator_type::template rebind<U>::other alloc(allocator);
				alloc.construct(data, u);
				m_back = (char*)data+aligned_size<U>::value;
				e->m_next = m_back;
			}
#endif

			value_type & front()
			{
				value_type * data = reinterpret_cast<value_type*>(m_front+aligned_size<entry>::value);
				return * data;
			}

			void pop()
			{
				entry * e = reinterpret_cast<entry*>(m_front);
				m_front = e->m_next;
			}

			bool empty() const { return m_front == m_back; }

			void reset()
			{
				m_front = m_back = reinterpret_cast<char*>(this) + aligned_size<chain>::value;
			}
		};

		size_type capacity() const
		{
			return m_tail ? m_tail->capacity() : 0;
		}

		void add_chain(size_type bytes)
		{
			typename allocator_type::template rebind<char>::other alloc(m_allocator);
			char *buffer = alloc.allocate(bytes);
			chain *c = reinterpret_cast<chain*>(buffer);
			c->m_next = 0;
			c->reset();
			c->m_end = buffer+bytes;
			if(m_tail) m_tail->m_next = c, m_tail = c;
			else m_head = m_tail = c;
		}

		void erase(chain * c)
		{
			typename allocator_type::template rebind<char>::other alloc(m_allocator);
			alloc.deallocate( reinterpret_cast<char*>(c), c->m_end-reinterpret_cast<char*>(c) );
		}

		chain *m_head, *m_tail;
		size_type m_size;
		allocator_type m_allocator;

		fifo(const fifo&);
		fifo & operator=(const fifo&);
	};
}
