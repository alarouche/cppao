
#ifndef ACTIVE_PROMISE_INCLUDED
#define ACTIVE_PROMISE_INCLUDED

#include "direct.hpp"

#ifdef ACTIVE_USE_BOOST
	#include <boost/thread/future.hpp>
	namespace active
	{
		namespace platform
		{
			using namespace boost;
			template<typename T>
			struct future
			{
				typedef unique_future<T> type;
			};
			template<typename T>
			bool is_valid(unique_future<T> &f) { return f.is_ready(); }
		}
	}
#else
	#include <future>
	namespace active
	{
		namespace platform
		{
			using namespace std;
			template<typename T>
			struct future
			{
				typedef std::future<T> type;
			};
			template<typename T>
			bool is_valid(std::future<T> &f) { return f.valid(); }
		}
	}
#endif

namespace active
{
	template<typename T>
	T wait(platform::promise<T> & promise, scheduler & sched = default_scheduler)
	{
		typename platform::future<T>::type fut = promise.get_future();
		while( !platform::is_valid(fut) && idle(sched) )
		{
		}
		return fut.get();
	}

	template<typename T>
	struct promise :
		public object<promise<T>, object_impl<schedule::thread_pool, queueing::direct_call, sharing::disabled> >,
		public active::sink<T>
	{
		typedef T value_type;

		void send(value_type v)
		{
			m_value.set_value(v);
		}

		T get()
		{
			return wait(m_value,this->get_scheduler());
		}
	private:
		platform::promise<value_type> m_value;
	};
}

#endif
