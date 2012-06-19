/* Cppao: C++ Active Objects library
 * Written by Calum Grant 2012
 *
 * It uses C++11. In g++, compile with "-std=c++0x -pthread"
 *
 * This library demonstrates a very simple approach to implementing active objects.
 */

#ifndef ACTIVE_OBJECT_INCLUDED
#define ACTIVE_OBJECT_INCLUDED

#include <mutex>
#include <deque>
#include <condition_variable>
#include <memory>
#include <thread>

#define ACTIVE_IFACE(TYPE) virtual void operator()( const TYPE & )=0;

namespace active
{	
    // Interface of all active objects.
	struct any_object
	{
		virtual ~any_object();
        virtual void run() throw()=0;
        virtual bool run_some(int n=100) throw()=0;
        virtual void exception_handler() throw();
        any_object * m_next;
	};
	
	
	// Represents a pool of active objects which can be executed in a thread pool.
	class pool
	{
	public:
		typedef any_object * ObjectPtr;

		pool();
		
		// Adds an active object to the pool.
		// There is no need to "un-add", but deleting an active object with active messages
		// is undefined (i.e. the application will crash).
		// void add(ObjectPtr p);
		
		// Used by an active object to signal that there are messages to process.
        void activate(ObjectPtr) throw();

		// Thread tracking:
        void start_work() throw();
        void stop_work() throw();
		
        // Runs in current thread until there are no more messages in the entire pool.
		// Returns false if no more items.
        void run();
				
		// Runs all objects in a thread pool, until there are no more messages.
        void run(int threads);

    private:
		std::mutex m_mutex;
        any_object *m_head, *m_tail;   // List of activated objects.
        std::condition_variable m_ready;
        int m_busy_count;	// Used to work out when we have actually finished.
        std::exception_ptr m_exception;
		void run_in_thread();
        bool run_managed() throw();
	};
	
	// As a convenience, provide a global variable to run all active objects.
	extern pool default_pool;

	namespace sharing	// Namespace for sharing concepts
	{
		struct disabled
		{
			struct base { };
			typedef any_object * pointer_type;
			pointer_type pointer(any_object * obj) { return obj; }
            void activate(base *) { }
            void deactivate(pointer_type) { }
		};
	
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
		
	namespace schedule	// Schedulers
	{
        // The object is not scheduled
		struct none
		{
			typedef active::pool type;
			none(type&) { }
			void set_pool(type&p) { }
            void activate(const std::shared_ptr<any_object> & sp) { }
            void activate(any_object * obj) { }
		};
		
        // The object is scheduled using the thread pool (active::pool)
		struct thread_pool
		{
			typedef active::pool type;
			thread_pool(type&);
			
			void set_pool(type&p);
            void activate(const std::shared_ptr<any_object> & sp);
            void activate(any_object * obj);
		private:
			type * m_pool;
		};

        // The object is scheduled by the OS in its own thread.
		struct own_thread
		{
			typedef active::pool type;			

            own_thread(const own_thread&);
            own_thread(type&);

            void set_pool(type&p);
            void activate(any_object * obj);
            void activate(const std::shared_ptr<any_object> & sp);

            own_thread();
            ~own_thread();
        private:
            void thread_fn();
            type * m_pool;  // Let pool track when all threads are idle => finished.
            bool m_shutdown;
            any_object * m_object;
            std::shared_ptr<any_object> m_shared;   // !! Remove this
            std::mutex m_mutex;
            std::condition_variable m_ready;
            std::thread m_thread;
        };
	}
	
	namespace queueing	// The queuing policy classes
	{
        // No queueing; run in calling thread without protection.
        struct direct_call
		{
			template<typename Message, typename Accessor>		
			bool enqueue(any_object * object, const Message & msg, const Accessor&)
			{
				try
				{
					Accessor::run(object, msg);
				}
				catch(...)
				{
					object->exception_handler();
				}
				return false;
			}
			
            bool run_some(any_object * o, int n=100) throw();
			
			template<typename T>
			struct queue_data
			{
				struct type { };
			};		
			
            bool empty() const;
		};
		
        // Safe but runs in the calling thread and can deadlock.
		struct mutexed_call
		{
            mutexed_call();
            mutexed_call(const mutexed_call&);
			
			template<typename Message, typename Accessor>		
			bool enqueue(any_object * object, const Message & msg, const Accessor&)
			{
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
				try
				{
					Accessor::run(object, msg);
				}
				catch(...)
				{
					object->exception_handler();
				}
				return false;
			}
			
            bool run_some(any_object * o, int n=100) throw();
			
			template<typename T>
			struct queue_data
			{
				struct type { };
			};	
			
            bool empty() const;

        private:
            std::recursive_mutex m_mutex;
		};
		

        // Default message queue shared between all message types.
        class shared
		{	
            struct message
            {
                message() : m_next(nullptr) { }
                virtual void run(any_object * obj)=0;
                virtual ~message() { }
                message *m_next;
            };

        public:
            shared();
            shared(const shared&);
			
			template<typename T>
			struct queue_data
			{
				struct type { };
			};

			template<typename Message, typename Accessor>		
			bool enqueue( any_object *, const Message & msg, const Accessor&)
			{
                return enqueue( new message_impl<Message, Accessor>(msg) );
            }
			
            bool empty() const;
			
            bool run_some(any_object * o, int n=100) throw();
        protected:
            std::mutex m_mutex;

        private:
            template<typename Message, typename Accessor>
            struct message_impl : public message
            {
                message_impl(const Message & m) : m_message(m) { }
                Message m_message;
                void run(any_object * o)
                {
                    Accessor::run(o, m_message);
                }
            };

            bool enqueue(message*impl);

            // Push to tail, pop from head:
            message *m_head, *m_tail;
		};
		
		struct separate
		{			
            separate();
            separate(const separate&);

			template<typename T>
			struct queue_data
			{
				typedef std::deque<T> type;
			};
						
			template<typename Message, typename Accessor>		
			bool enqueue( any_object * object, const Message & msg, const Accessor&)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
                m_message_queue.push_back( &run<Message, Accessor> );
                try
                {
                    Accessor::data(object).push_back(msg);
                }
                catch(...)
                {
                    m_message_queue.pop_back();
                    throw;
                }
                return m_message_queue.size()==1;
			}
			
            bool empty() const;
            bool run_some(any_object * o, int n=100) throw();

        protected:
            std::mutex m_mutex;

        private:
            typedef void (*ActiveRun)(any_object*);
            std::deque<ActiveRun> m_message_queue;

            template<typename Message, typename Accessor>
            static void run(any_object*obj)
            {
                Message m = Accessor::data(obj).front();
                Accessor::data(obj).pop_front();
                Accessor::run(obj, m);
            }
		};
		
		template<typename Queue>
        struct steal  : public Queue
		{
			steal() : m_running(false) { }
			
			template<typename Message, typename Accessor>		
			bool enqueue( any_object * object, const Message & msg, const Accessor& accessor)
			{
				this->m_mutex.lock();
				if( m_running || !this->empty())
				{
                    this->m_mutex.unlock();	// ?? Better use of locks; also exception safety.
					Queue::enqueue( object, msg, accessor );
					return false;
				}
				else
				{
					m_running = true;
					this->m_mutex.unlock();
					try
					{
						Accessor::run(object, msg);
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
	
	
	
	template<
		typename Schedule=schedule::thread_pool, 
		typename Queue=queueing::shared, 
		typename Share=sharing::disabled>
	class object_impl : public any_object, public Share::base
	{
	public:
		typedef Schedule schedule_type;
		typedef Share share_type;
		typedef Queue queue_type;
		
		object_impl(typename schedule_type::type & tp = default_pool) : m_schedule(tp) { }
		
		~object_impl() { if(!m_queue.empty()) std::terminate(); }
		
		void run() throw()
		{
            typename share_type::pointer_type p=0;
            m_share.deactivate(p);

            while( m_queue.run_some(this) )
				;
		}

		bool run_some(int n) throw()
		{
            typename share_type::pointer_type p=0;
            m_share.deactivate(p);

            // Run a few messages from the queue
			// if we still have messages, then reactivate this object.
			if( m_queue.run_some(this, n) )
			{
                m_share.activate(this);
				m_schedule.activate(m_share.pointer(this));
				return true;
			}
			else
			{
				return false;
			}
		}
		
		void set_scheduler(typename schedule_type::type & pool)
		{
			m_schedule.set_pool(pool);
		}
		
	protected:
		template<typename T, typename M>
		void enqueue( const T & msg, const M & accessor)
		{
			if( m_queue.enqueue(this, msg, accessor) )
            {
                m_share.activate(this);
				m_schedule.activate(m_share.pointer(this));
            }
		}

		
	private:
		queue_type m_queue;
		schedule_type m_schedule;
		share_type m_share;
	};
	
#define ACTIVE_IMPL( MSG ) impl_##MSG(const MSG & MSG)


#define ACTIVE_METHOD2( MSG, TYPENAME, TEMPLATE ) \
	TYPENAME queue_type::TEMPLATE queue_data<MSG>::type queue_data_##MSG; \
	template<typename C> struct run_##MSG { \
		static void run(::active::any_object*o, const MSG&m) { static_cast<C>(o)->impl_##MSG(m); } \
		static TYPENAME queue_type::TEMPLATE queue_data<MSG>::type& data(::active::any_object*o) {return static_cast<C>(o)->queue_data_##MSG; } }; \
    void operator()(const MSG & x) { this->enqueue(x, run_##MSG<decltype(this)>() ); } \
	void ACTIVE_IMPL(MSG)
	
	
#define ACTIVE_METHOD( MSG ) ACTIVE_METHOD2(MSG,,)
#define ACTIVE_TEMPLATE(MSG) ACTIVE_METHOD2(MSG,typename,template)

	// The default object type.
	typedef object_impl<schedule::thread_pool, queueing::shared, sharing::disabled> object;
	typedef object_impl<schedule::own_thread, queueing::shared, sharing::disabled> thread;
	
	typedef object_impl<schedule::thread_pool, queueing::steal<queueing::shared>, sharing::disabled> fast;
	typedef object_impl<schedule::none, queueing::direct_call, sharing::disabled> direct;
    typedef object_impl<schedule::none, queueing::mutexed_call, sharing::disabled> synchronous;
	
	// An active object which could be stored in a std::shared_ptr.
	// If this is the case, then a safer message queueing scheme is implemented
	// which for example guarantees to deliver all messages before destroying the object.
	template<typename T=any_object, typename Schedule=schedule::thread_pool, typename Queueing = queueing::shared>
	class shared : public object_impl<Schedule, Queueing, sharing::enabled<T> >
	{
	public:
		typedef std::shared_ptr<T> ptr;
	};
	
	template<typename T=any_object, typename Queueing = queueing::shared>
	class shared_thread : public object_impl<schedule::own_thread, Queueing, sharing::enabled<T> >
	{
	public:
		typedef std::shared_ptr<T> ptr;
	};

	// Run all objects in the default thread pool
	void run(int threads=std::thread::hardware_concurrency());
	
	template<typename T>
	struct sink
	{
		typedef std::shared_ptr<sink<T>> ptr;
		ACTIVE_IFACE( T );
	};	
} // namespace active

    
#endif
