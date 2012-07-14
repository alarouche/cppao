
#ifndef ACTIVE_PROMISE_INCLUDED
#define ACTIVE_PROMISE_INCLUDED

#include "scheduler.hpp"
#include "direct.hpp"
#include <future>

namespace active
{
	template<typename T>
	struct promise : 
		public object<promise<T>, object_impl<schedule::thread_pool, queueing::direct_call, sharing::disabled>>, 
		public active::sink<T>
	{
		typedef T value_type;
		
		void active_method(value_type && v) 
		{ 
			m_value.set_value(v); 
		}

		T get()
		{
			auto fut = m_value.get_future();
			while( !fut.valid() && this->steal() )	// gcc/clang calls it valid()
			{
				// Do some work whilst waiting for the promise to be fulfilled.
				// However if steal() returns false, it means that there
				// is no more work (for now).
			}
			return fut.get();
		}
	private:
		std::promise<value_type> m_value;
	};
	
	template<typename T>
	T wait(std::promise<T> & promise, scheduler & sched = default_scheduler)
	{
		auto fut = promise.get_future();
		while( !fut.valid() && sched.run_one() )
		{
		}
		return fut.get();
	}	
}

#endif
