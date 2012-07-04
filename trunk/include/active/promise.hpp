
#ifndef ACTIVE_PROMISE_INCLUDED
#define ACTIVE_PROMISE_INCLUDED

#include <future>
#include "scheduler.hpp"

namespace active
{
	template<typename T>
	struct promise : public object_impl<schedule::thread_pool, queueing::mutexed_call, sharing::disabled>, public active::sink<T>
	{
		typedef T value_type;
		ACTIVE_TEMPLATE(value_type) { m_value.set_value(value_type); }

		T get()
		{
			auto fut = m_value.get_future();
			while( !fut.valid() )	// clang calls it valid()
				steal();
			return fut.get();
		}
	private:
		std::promise<value_type> m_value;
	};
}

#endif
