/*  Benchmark suite for cppao.
  */

#include <active/object.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>
#include <active/advanced.hpp>
#include <active/thread.hpp>
#include <active/fast.hpp>
#include <active/promise.hpp>
#include <active/shared.hpp>

#include <iostream>
#include <cstring>
#include <cassert>

#ifdef ACTIVE_USE_BOOST
#include <boost/timer.hpp>
using boost::timer;
#else
class timer
{
public:
	timer() { reset(); }
	double elapsed()
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto dur = (now-start).count();
		return double(std::chrono::high_resolution_clock::duration::period::num) * dur /
			double(std::chrono::high_resolution_clock::duration::period::den);
	}
	void reset()
	{
		start=std::chrono::high_resolution_clock::now();
	}
private:
	std::chrono::high_resolution_clock::time_point start;
	
};
#endif

struct test
{
	virtual void run(int threads)=0;
	virtual void params(std::ostream&)=0;
	virtual int messages()=0;
	virtual int objects()=0;
	virtual const char * name()=0;
    virtual bool validate()=0;
	void run_tests(int min_threads=1,
				   int max_threads=active::platform::thread::hardware_concurrency())
	{
        if( max_threads==0 ) max_threads=4;
		for(int t=min_threads; t<=2*max_threads; ++t)
		{
			timer timer;
			run(t);
			double duration=timer.elapsed();
			// Sleep for 1 second between tests
			const int sleep_time=0;
#ifdef ACTIVE_USE_BOOST
			active::platform::this_thread::sleep(boost::posix_time::milliseconds(sleep_time));
#else
			active::platform::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
#endif

			std::cout << name() << ",";
			params(std::cout);
			std::cout << "," << t << "," << duration << "," << objects() << ","
                << messages() << "," << (messages()/(1000000.0*duration)) << std::endl;
            if( !validate() )
            {
                std::cout << ",ERROR: Test validation failed!!!\n";
                std::terminate();   // Let unit test suite know that something has gone badly wrong.
            }
		}
	}
private:
	test& operator=(const test&);
};

template<typename Object>
const char * description() { return "todo"; }

template<> const char * description<active::direct> () { return "active::direct"; }
template<> const char * description<active::synchronous> () { return "active::synchronous"; }
template<> const char * description<active::basic> () { return "active::basic"; }
template<> const char * description<active::advanced> () { return "active::advanced"; }
template<> const char * description<active::fast> () { return "active::fast"; }
template<> const char * description<active::thread> () { return "active::thread"; }

namespace thread_ring
{
	template<typename Object>
	struct thread : public active::object<thread<Object>,Object>
	{
	};
	
	template<typename Object>
	struct thread_ring_test : public test
	{
		virtual void run(int threads)
		{
			m_nodes[0](m_messages);
			active::run r(threads);
		}
		virtual void params(std::ostream&os)
		{
			os << "(" << description<Object>() << " " << m_messages << " " << m_nodes.size() << ")";
		}
		virtual int messages()
		{
			return m_messages;
		}
		virtual int objects()
		{
			return m_nodes.size();
		}
        bool validate()
        {
            return m_nodes[0].last_count == m_messages % m_nodes.size();
        }
		virtual const char * name()
		{
			return "thread-ring";
		}
	public:
		thread_ring_test(int messages, int nodes) : m_nodes(nodes), m_messages(messages)
		{
			for(int n=1; n<nodes; ++n)
				m_nodes[n].next = &m_nodes[n-1];
			m_nodes[0].next=&m_nodes[nodes-1];
		}
		
	private:
		thread_ring_test & operator=(const thread_ring_test&);

		struct node : public active::object<node,Object>
		{
			node * next;
            int last_count;
			void active_method(int value)
			{
                last_count = value;
				if( value ) (*next)(value-1);
			}
            node() : last_count(-1) { }
		};
		
		std::vector<node> m_nodes;
		const int m_messages;
	};
	
	template<typename Object>
	void run(int messages, int nodes)
	{
		thread_ring_test<Object> obj(messages,nodes);
		obj.run_tests();
	};
	
	void run(bool quick)
	{
		int num_messages = quick ? 100000 : 10000000;
		int num_nodes = 503;
		
		run<active::fast>(num_messages, 50);
		run<active::basic>(num_messages, num_nodes);
		run<active::advanced>(num_messages, num_nodes);
        run<active::thread>(num_messages/10,num_nodes/10);
	}
};

namespace sieve
{
	template<typename Object>
	struct sieve_test : public test
	{
		virtual void run(int threads)
		{
			base b(m_max);
			b(2);
			{ active::run r(threads); }
			m_messages=m_max;
			m_objects=1;
			for( active::platform::shared_ptr<prime> p = b.next; p; p=p->next )
			{
				++m_objects;
				m_messages += p->m_messages;
			}
			
		}
		virtual void params(std::ostream&os)
		{
			os << "(" << description<Object>() << " " << m_max << ")";
		}
		virtual int messages()
		{
			return m_messages;
		}
		virtual int objects()
		{
			return m_objects;
		}
		virtual const char * name()
		{
			return "sieve";
		}
        bool validate()
        {
            return true;
        }
		sieve_test(int max) : m_max(max) { }
	public:
		
		struct destroy{};
		
		struct prime : public active::shared<prime,Object>,active::handle<prime,int>
		{
			prime(int p) : m_value(p), m_messages(0) { }
			void active_method(int p)
			{
				++m_messages;
				if( p % m_value )
				{
					if( next ) next->send(p);
					else next.reset( new prime(p) );
				}
			}
			
			void active_method(destroy)
			{
				++m_messages;
				if(next)
				{
					(*next)(destroy());
					next.reset();
				}
			}
			
			const int m_value;
			active::platform::shared_ptr<prime> next;
			int m_messages;
		};
		
		struct base : public active::object<base, Object>
		{
			base(int max) : m_max(max) { }
			
			active::platform::shared_ptr<prime> next;
			
			void active_method(int n)
			{
				if(next) next->send(n);
				else next.reset(new prime(n));
				if( n<m_max ) (*this)(n+1);
				// else (*next)(destroy());
			}
			
			const int m_max;
		};
		const int m_max;
		int m_messages, m_objects;
	};
	
	template<typename Object>
	void run(int max)
	{
		sieve_test<Object> obj(max);
		obj.run_tests();
	}
	
	void run(bool quick)
	{
		int max = quick ? 5000 : 50000;
		int max_recursive = std::min(max,500);
		run<active::direct>(max_recursive);
		run<active::synchronous>(max_recursive);
		run<active::fast>(max_recursive);
		run<active::basic>(max);
		run<active::advanced>(max);
	}
}

namespace fib
{
	template<typename Object>
	class fib_test : public test
	{
	public:
		fib_test(int n) : m_value(n), m_node(n)
		{
		}
		
		int real_fib(int n)
		{
			int f1=1,f2=1,f=1;
			for(int i=2; i<n; ++i, f2=f1, f1=f)
				f=f1+f2;
			return f;
		}
		
		virtual void run(int threads)
		{
			active::promise<int> r;
			m_node(m_value, &r);
			active::run s(threads);
            m_result = r.get();
		}
        bool validate()
        {
            return m_result == real_fib(m_value);
        }
		virtual void params(std::ostream&os)
		{
			os << "(" << description<Object>() << " " << m_value << ")";
		}
		virtual int messages()
		{
			return m_node.m_messages;
		}
		virtual int objects()
		{
			return m_node.m_objects;
		}
		virtual const char * name()
		{
			return "fib";
		}
		
	private:

		struct node : public active::object<node,Object>, active::handle<node,int>
		{
			node(int v)
			{
				if( v>2 )
				{
					m_left.reset( new node(v-1)), m_right.reset(new node(v-2));
					m_objects = 1 + m_left->m_objects + m_right->m_objects;
					m_messages = 3 + m_left->m_messages + m_right->m_messages;
				}
				else
					m_objects=1, m_messages=1;
			}
			
			void active_method(int n, active::sink<int>*result)
			{
				if(n>2)
				{
					m_value=0;
					m_result = result;
					(*m_left)(n-1, this);
					(*m_right)(n-2, this);
				}
				else
					result->send(1);
			}
			
			void active_method(int v)
			{
				if( m_value ) m_result->send(m_value+v);
				else m_value = v;
			}
			
			int m_objects, m_messages;
		private:
			active::platform::shared_ptr<node> m_left, m_right;
			int m_value;
			active::sink<int> * m_result;
		};
		
		const int m_value;
		node m_node;
        int m_result;
	};
	
	template<typename Object>
	void run(int n)
	{
		fib_test<Object> obj(n);
		obj.run_tests();
	}
	
	void run(bool quick)
	{
		int n = quick ? 26 : 30;
		run<active::direct>(n);
		run<active::synchronous>(n);
		run<active::fast>(n);
		run<active::basic>(n);
		run<active::advanced>(n);
	}
}

namespace fifo
{
	template<typename T>
	struct sink;
	
	template<typename T>
	struct source
	{
		virtual void send_ready(sink<T>&)=0;  // 3) Sink is now ready
		// virtual void send_to(sink<T>&)=0;	// 1) Source-initiated push
	};
	
	template<typename T>
	struct sink
	{
		virtual void wait_send(source<T>&)=0;	// 2) Sink-initiated pull
		virtual void send(T)=0; // 4) Actually send the message
	};
	
	template<typename T>
	struct queue :
		public active::object<queue<T>, active::synchronous>,
		public source<T>,
		public sink<T>
	{
        queue(int capacity) : m_capacity(capacity)
		{
		}
		
		// Request by new reader
		void send_ready(sink<T>& reader)
		{
			(*this)(&reader);
		}

		// Writer wants to send
		void wait_send(source<T>&writer)
		{
			(*this)(&writer);
		}
		
		void send(T value)
		{
			(*this)(value);
		}
		
	private:
		friend class active::object<queue<T>, active::synchronous>;
		
		void active_method(source<T> * w)
		{
			++m_message_count;
			if( m_values.size() < m_capacity )
				w->send_ready(*this);
			else
				m_writers.push_back(w);
		}
				
		void active_method(T value)
		{
			++m_message_count;
			if( m_readers.empty() || !m_values.empty() )
			{
				m_values.push_back(value);
			}
			else
			{
				sink<T> * r = m_readers.front();
				m_readers.pop_front();
				r->send(value);
			}
		}
		
		void active_method(sink<T> * r)
		{
			++m_message_count;
            while( m_values.size() <= m_capacity && !m_writers.empty() )
			{
				source<T> * w = m_writers.front();
				m_writers.pop_front();
				w->send_ready(*this);
			}
			
			if( m_values.empty() ) m_readers.push_back(r);
			else r->send(m_values.front()), m_values.pop_front();
		}
				
	public:
		const std::size_t m_capacity;
		int m_message_count;
		
		std::deque<T> m_values;
		std::deque<sink<T>*> m_readers;
		std::deque<source<T>*> m_writers;
	};
	
	template<typename Object>
	struct test_source :
		public active::object<test_source<Object>, Object>,
		public source<int>
	{
		void send_ready(sink<int>&output)
		{
			(*this)(&output);
		}
		
		void active_method(sink<int>*output, int n)
		{
			++m_message_count;
			m_remaining = n;
			output->wait_send(*this);
		}

		void active_method(sink<int>*output)
		{
			++m_message_count;
			if( m_remaining>0 )
			{
				// std::cout << "Sending " << m_remaining << std::endl;
				output->send(m_remaining--);
				output->wait_send(*this);
			}
		}
		
	public:
		int m_remaining;
		int m_message_count;
	};
	
	
	template<typename Object>
	struct test_sink :
		public active::object<test_sink<Object>, Object>,
		public sink<int>
	{
		test_sink(source<int> &src) :
			m_total(0),
			m_source(src)
		{
		}
		
		void wait_send(source<int>&src)
		{
			(*this)(&src);
		}
		
		void active_method(source<int>*src)
		{
			src->send_ready(*this);
		}
		
		void send(int i)
		{
			(*this)(i);
		}
		
		void active_method(int v)
		{
			// std::cout << "Received " << v << std::endl;
			++m_message_count;
			m_total+=v;
			
			// Bug: This message is superfluous with no buffer
			m_source.send_ready(*this);
		}
		
		int m_total;
		int m_message_count;
	private:
		source<int> &m_source;
	};

	template<typename Object>
	struct queue_test_no_buffer : public test
	{
		queue_test_no_buffer(int messages) : m_messages(messages), m_sink(m_source)
		{
		}
	
		virtual void run(int threads)
        {
            m_sink.m_total=0;
			m_source.m_message_count = m_sink.m_message_count=0;
			m_source(&m_sink, m_messages);
			active::run s(threads);
		}
        bool validate()
        {
            return m_sink.m_total == m_messages * (m_messages+1)/2;
        }
		virtual void params(std::ostream&os)
		{
			os << "(" << description<Object>() << " no-buffer " << m_messages << ")";
		}
		virtual int messages()
		{
			return m_source.m_message_count + m_sink.m_message_count; // 2*m_messages;
		}
		virtual int objects()
		{
			return 2;
		}
		virtual const char * name()
		{
			return "fifo";
		}

	private:
		const int m_messages;
		test_source<Object> m_source;
		test_sink<Object> m_sink;
	};
	
	template<typename Object>
	struct buffered_queue_test : public test
	{
		buffered_queue_test(int messages, int queue_size, int readers, int writers) :
			m_messages(messages), m_queue_size(queue_size),
			m_readers(readers), m_writers(writers),
			m_sources(m_writers),
			m_queue(queue_size),
			m_sinks(m_readers, m_queue)
		{
		}
		virtual void run(int threads)
		{
			m_queue.m_message_count=0;
			for(int r=0; r<m_readers; ++r)
			{
				m_sinks[r].m_total=0;
				m_sinks[r].m_message_count=0;
				m_queue.send_ready(m_sinks[r]);
			}
			for(int w=0; w<m_writers; ++w)
			{
				m_sources[w].m_message_count=0;
				m_sources[w](&m_queue, m_messages);
			}

			{ active::run s(threads); }
        }
        bool validate()
        {
            int validation=0;
            for(int r=0; r<m_readers; ++r)
            {
                validation += m_sinks[r].m_total;
            }
            int expected = m_writers * (m_messages/2) * (m_messages+1);
            return validation == expected;
        }
		virtual void params(std::ostream&os)
		{
			os << "(" << description<Object>() << " buffered " << m_messages <<
				" " << m_queue_size << " " << m_writers << " " << m_readers << ")";
		}
		virtual int messages()
		{
			int total = m_queue.m_message_count;
			for(int w=0; w<m_writers; ++w)
				total += m_sources[w].m_message_count;
			for(int r=0; r<m_readers; ++r)
				total += m_sinks[r].m_message_count;
			return total;
		}
		virtual int objects()
		{
			return 1+m_readers+m_writers;
		}
		virtual const char * name()
		{
			return "fifo";
		}
	private:
		const int m_messages, m_queue_size, m_readers, m_writers;
		std::vector<test_source<Object> > m_sources;
		queue<int> m_queue;
		std::vector<test_sink<Object> > m_sinks;
	};
	
	struct advanced_queue_test : public test
	{
		struct sink : public active::object<sink,active::advanced>
		{
			sink(int capacity) : m_total(0)
			{
				set_capacity(capacity);
			}
			
			void active_method(int i)
			{
				m_total += i;
			}
			
			int m_total;
		};
		
		struct source : public active::object<source,active::advanced>
		{
			source(sink &s) : m_sink(s)
			{
			}
			
			void active_method(int n)
			{
				for(int i=0; i<n; ++i) m_sink(i);
			}
			
			sink & m_sink;
		};

        bool m_validated;
		virtual void run(int threads)
		{
			sink s(m_capacity);
			source src(s);
			src(m_count);
            { active::run r(threads); }
            m_validated = s.m_total == m_count*(m_count-1)/2;
		}
        bool validate()
        {
            return m_validated;
        }
		virtual void params(std::ostream&os)
		{
			os << "(advanced " << m_count << " " << m_capacity << ")";
		}
		virtual int messages()
		{
			return m_count;
		}
		virtual int objects()
		{
			return 2;
		}
		virtual const char * name()
		{
			return "fifo";
		}
		
		advanced_queue_test(int count, int capacity) : m_count(count), m_capacity(capacity)
		{
		}
		
		const int m_count, m_capacity;
	};
	
	template<typename Obj>
	void run_no_buffer_test(bool quick)
	{
		int messages = quick ? 10000 : 1000000;
		
		queue_test_no_buffer<Obj> t1(messages);
		t1.run_tests();
	}
	
	template<typename Obj>
	void run_buffer_test(bool quick, int writers, int readers)
	{
		int messages = quick ? 10000 : 1000000;
		int queue_size = quick ? 1000 : 100000;
		
		buffered_queue_test<Obj> t1(messages, queue_size, writers, readers);
		t1.run_tests();
	}
	
	void run(bool quick)
	{
		int messages = quick ? 10000 : 1000000;
		int queue_size = quick ? 1000 : 100000;

		run_buffer_test<active::fast>(quick, 2, 2);
		run_buffer_test<active::basic>(quick, 2, 2);
		run_buffer_test<active::advanced>(quick, 2, 2);
		run_buffer_test<active::thread>(quick, 2, 2);

		run_buffer_test<active::fast>(quick, 1, 1);
		run_buffer_test<active::basic>(quick, 1, 1);
		run_buffer_test<active::advanced>(quick, 1, 1);
		run_buffer_test<active::thread>(quick, 1, 1);
		
		run_no_buffer_test<active::fast>(quick);
		run_no_buffer_test<active::basic>(quick);
		// run_no_buffer_test<active::advanced>(quick);
		run_no_buffer_test<active::thread>(quick);
		
		advanced_queue_test t2(messages, queue_size);
		t2.run_tests(2);
	}
}

int main(int argc, char**argv)
{
	std::cout << "Welcome to the cppao benchmark suite!\n"
		"The purpose of this test suite is to track the performance of cppao\n"
		"using a number of measures, and to measure scalability with hardware\n"
		"The tests are as follows:\n\n"
		"Test name,Parameters,Description\n"
		"thread-ring,(object type, nodes, iterations),Bounce a message around a ring of objects\n"
		"sieve,(object type,max),prime number sieve\n"
		"fib,(object type, value),Recursive fibonacci\n"
		"fifo,(object type,producers,consumers,count,buffer size),FIFO\n";
	;

	std::cout << "\nGlobal parameters:\n";
	std::cout << "Hardware concurrency, " << active::platform::thread::hardware_concurrency() << std::endl;
	std::cout << "Architecture, " << 8 * sizeof(void*) << " bit\n";
	std::cout << "Debug, " <<
#ifdef NDEBUG
	0
#else
	1
#endif
		<< std::endl;
	bool quick = argc>1 && strcmp(argv[1],"quick")==0;

	std::cout << "Quick, " << int(quick) << "\n";
	
	std::cout <<
	"\nResults:\n"
	"Test name,Parameters,Threads,Time(s),Objects,Messages,Million messages per second\n";
	
	sieve::run(quick);
	thread_ring::run(quick);
	fib::run(quick);
	fifo::run(quick);
	std::cout << "Benchmarks finished!\n";
}
