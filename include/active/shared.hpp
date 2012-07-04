
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
	
	// An active object which could be stored in a std::shared_ptr.
	// If this is the case, then a safer message queueing scheme is implemented
	// which for example guarantees to deliver all messages before destroying the object.
	template<typename T=any_object, typename Schedule=schedule::thread_pool, typename Queueing = queueing::shared<>>
	class shared : public object_impl<Schedule, Queueing, sharing::enabled<T> >
	{
	public:
		typedef typename Queueing::allocator_type allocator_type;
		typedef typename Schedule::type scheduler_type;
		shared(scheduler_type & sch=default_scheduler, const allocator_type & alloc = allocator_type()) :
		object_impl<Schedule, Queueing, sharing::enabled<T> >(sch, alloc)
		{
		}                                                                                                           // !!shared_thread(shceduler, allocator)
		typedef std::shared_ptr<T> ptr;
	};
	
}

#endif
