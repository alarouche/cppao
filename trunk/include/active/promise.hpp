
#ifndef ACTIVE_PROMISE_INCLUDED
#define ACTIVE_PROMISE_INCLUDED

#include "scheduler.hpp"
#include "direct.hpp"
#include <future>

namespace active
{
	template<typename T>
	struct promise : 
		public object_impl<schedule::thread_pool, queueing::direct_call, sharing::disabled>, 
		public active::sink<T>
	{
		typedef T value_type;
		
		ACTIVE_TEMPLATE(value_type) 
		{ 
			m_value.set_value(value_type); 
		}

		T get()
		{
			auto fut = m_value.get_future();
			while( !fut.valid() )	// gcc/clang calls it valid()
				steal();	// Avoid blocking the thread.
			return fut.get();
		}
	private:
		std::promise<value_type> m_value;
	};
}

#endif
