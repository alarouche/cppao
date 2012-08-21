#include <active/object.hpp>
#include <active/direct.hpp>
#include <active/synchronous.hpp>
#include <active/advanced.hpp>
#include <active/thread.hpp>
#include <active/fast.hpp>
#include <active/promise.hpp>

#include <iostream>
#include <cstring>

#ifdef ACTIVE_USE_BOOST
#else
class timer
{
public:
	timer() { reset(); }
	double seconds()
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
	void run_tests(int min_threads=1,
				   int max_threads=active::platform::thread::hardware_concurrency())
	{
		for(int t=min_threads; t<=2*max_threads; ++t)
		{
			timer timer;
			run(t);
			double duration=timer.seconds();
			std::cout << name() << ",";
			params(std::cout);
			std::cout << "," << t << "," << duration << "," << objects() << ","
				<< messages() << "," << (messages()/(1000000.0*duration)) << std::endl;
		}
	}
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
		struct node : public active::object<node,Object>
		{
			node * next;
			void active_method(int value)
			{
				if( value ) (*next)(value-1);
			}
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
		run<active::thread>(num_messages/10,num_nodes);
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
		sieve_test(int max) : m_max(max) { }
	public:
		
		struct prime : public active::object<prime,Object>,active::handle<prime,int>
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
		int max_recursive = std::min(max,10000);
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
		fib_test(int n) : m_node(n), m_value(n)
		{
		}
		
		virtual void run(int threads)
		{
			active::promise<int> r;
			m_node(m_value, &r);
			active::run s(threads);
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
	
	thread_ring::run(quick);
	sieve::run(quick);
	fib::run(quick);
}