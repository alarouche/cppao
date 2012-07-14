
#ifndef ACTIVE_SHARED_INCLUDED
#define ACTIVE_SHARED_INCLUDED

#include "object.hpp"

namespace active
{
	namespace sharing
	{
		template<typename Obj=any_object>
		struct enabled
		{
			typedef std::enable_shared_from_this<Obj> base;
			typedef std::shared_ptr<any_object> pointer_type;
			pointer_type pointer(base * obj) { return obj->shared_from_this(); }
			void activate(base * obj) { m_activated = pointer(obj); }
			void deactivate(pointer_type & p) { m_activated.swap(p); }
		private:
			pointer_type m_activated;   // Prevent object being destroyed whilst active
		};
	}

	template<typename T=any_object, typename Object=basic> struct shared;

	template<typename T, typename Schedule, typename Queue, typename Share>
	struct shared<T, object_impl<Schedule, Queue, Share>> :
		public object<T, object_impl<Schedule, Queue, sharing::enabled<T>>>
	{
		typedef std::shared_ptr<T> ptr;
		typedef object_impl<Schedule, Queue, sharing::enabled<T>> type;
	};
}

#endif
