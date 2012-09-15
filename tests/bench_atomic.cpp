/* A test/benchmark for atomic_fifo and atomic_lifo.
 */

#include <active/atomic_fifo.hpp>
#include <active/atomic_lifo.hpp>

#include <mutex>
#include <cassert>
#include <vector>
#include <thread>
#include <iostream>
#include <condition_variable>

/*	Naive stack for performance comparison.
	This is completely lock/wait-free but suffers from ABA.
 */
class unsafe_lifo
{
public:
	unsafe_lifo() : list(nullptr) { }
	
	void push(active::atomic_node * n)
	{
		active::atomic_node * l;
		do
		{
			l = list;
			n->next = l;
		}
		while( !list.compare_exchange_weak( l, n ) );
	}
	
	active::atomic_node * pop()
	{
		active::atomic_node * l;
		do
		{
			l=list;
		}
		while ( l && !list.compare_exchange_weak(l, l->next) );
		return l;
	}
private:
	std::atomic<active::atomic_node*> list;
};


class mutex_fifo
{
public:
	mutex_fifo() : head(nullptr), tail(nullptr) { }
	
	void push(active::atomic_node *n)
	{
		n->next=nullptr;
		std::lock_guard<std::mutex> lock(mutex);
		if( head )
			tail->next=n, tail=n;
		else
			head=tail=n;
	}
	
	active::atomic_node * pop()
	{
		std::lock_guard<std::mutex> lock(mutex);
		active::atomic_node * result=head;
		if( head )
		{
			head = result->next;
		}
		return result;
	}
	
private:
	active::atomic_node * head, *tail;
	std::mutex mutex;
};



struct int_node : public active::atomic_node
{
	int value;
};

class synchronize
{
	std::mutex m_mutex;
	std::condition_variable m_ready;
	int m_busy_count;
public:
	void start()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		++m_busy_count;
	}
	void sync()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if( 0==--m_busy_count )
			m_ready.notify_all();
		else while( m_busy_count>0 )
			m_ready.wait(lock);
	}
};


template<typename Q>
struct thread
{
	thread(Q&q, synchronize & sync, int items, int loops) :
		m_q(q), m_sync(sync), m_items(items), m_loops(loops),
		m_node_count(0), m_value_count(0)
	{
	}
		
	Q & m_q;
	synchronize & m_sync;
	const int m_items, m_loops;
	
	int m_node_count, m_value_count;
	std::vector<int_node> m_nodes;
	
	void thread_fn()
	{
		std::vector<int_node> nodes(m_items);
							 
		for(int l=0; l<m_loops; ++l)
		{
			m_sync.start();
			for(auto & n : nodes)
			{
				n.value = rand()%5;
				++m_node_count;
				m_value_count += n.value;
				m_q.push(&n);
			}
			
			active::atomic_node * n;
			
			do
			{
				n = m_q.pop();
				if(n!=nullptr)
				{
					--m_node_count;
					m_value_count -= static_cast<int_node*>(n)->value;
				}
			}
			while(n!=nullptr);
			
			m_sync.sync();
			// We MUST wait for all threads to finish before continuing.
			// They might be in the process of processing the last few items
			// from our vector so it's not safe to reuse/destroy them yet.
		}
	}
};



template<typename Q>
int run_tests(int threads, int items, int loops)
{
	Q q;
	
	synchronize sync;
	
	std::vector<thread<Q>> threadvec(threads, thread<Q>(q,sync,items,loops));
	std::vector<std::thread> threadvec2;
	
	for(int t=0; t<threads; ++t)
	{
		threadvec2.push_back( std::thread(&thread<Q>::thread_fn, &threadvec[t]) );
	}
	
	for(auto &t : threadvec2)
	{
		t.join();
	}
	
	int extra_count=0;
	for( extra_count=0; q.pop(); ++extra_count)
		;
	
	if(extra_count)
	{
		std::cerr << "Queue not empty!\n";
		std::cerr << "There are an additional " << extra_count << " items\n";
		return 5;
	}
	
	int node_count=0, value_count=0;
	for(auto &t : threadvec)
	{
		node_count += t.m_node_count;
		value_count += t.m_value_count;
	}
	if( node_count!=0 )
	{
		std::cerr << "Error in node structure!\n";
		std::cerr << "Difference in node count = " << node_count << std::endl;
		return 1;
	}
	if( value_count!=0 )
	{
		std::cerr << "Error in value structure!\n";
		std::cerr << "Difference in value count = " << value_count << std::endl;
		return 2;
	}
	return 0;
}


int main(int argc, char**argv)
{
	if(argc<5)
	{
		std::cout << "Usage: aq algorithm threads items loops\n"
			"algorithm: 1-atomic_fifo, 2-atomic-lifo, 3-mutexed_fifo, 4-unsafe_lifo\n";
		return 1;
	}
	
	int algorithm= atoi(argv[1]);
	int threads = atoi(argv[2]);
	int items = atoi(argv[3]);
	int loops = atoi(argv[4]);
	
	switch( algorithm )
	{
		case 1: return run_tests<active::atomic_fifo>(threads, items, loops);
		case 2: return run_tests<active::atomic_lifo>(threads, items, loops);
		case 3: return run_tests<mutex_fifo>(threads, items, loops);
		case 4: return run_tests<unsafe_lifo>(threads, items, loops);
		default:
			std::cerr << "Unknown algorithm\n";
			return 3;
	}
}

