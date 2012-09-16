#include <active/atomic_fifo.hpp>
#include <active/atomic_lifo.hpp>
#include <thread>
#include <cassert>

/*  Experimentation with lock-free FIFO.  C++11.
 This example uses two linked lists, one for the input, and one for the output.
 
 A push() is completely lock/wait-free, because it does a standard slist push
 onto input_queue.
 
 A pop() is lock-free but not wait-free. If the output_queue is busy, then it must
 spin-wait. If the output_queue is empty, then pop() sets output_queue to busy,
 then transfer the input_queue to the output_queue.
 
 ABA problem is avoided because of the two exchanges in pop().
 */


namespace
{
	active::atomic_node busy = { (active::atomic_node*)(-1) };
	
	template<typename T>
	T acquire(std::atomic<T> & value, const T busy)
	{
		T v;
		int spin_count=2000;
		for(;;)
		{
			v = value.exchange(busy, std::memory_order_acquire);
			if(v!=busy) return v;
			else if(--spin_count==0)
			{
				// Yield
				std::this_thread::sleep_for(std::chrono::seconds(0));
				spin_count=2000;
			}
		}
	}

	template<typename T>
	void release(std::atomic<T> & value, const T new_value)
	{
		value.store(new_value, std::memory_order_release);
	}
}

active::atomic_fifo::atomic_fifo() : input_queue(nullptr), output_queue(nullptr)
{
}

void active::atomic_fifo::push(atomic_node*n)
{
	atomic_node * i;
	do
	{
		i = input_queue.load( std::memory_order_relaxed);
		n->next = i;
	}
	while( !input_queue.compare_exchange_weak( i, n, std::memory_order_relaxed) );
}

active::atomic_node * active::atomic_fifo::pop()
{
	atomic_node * t = acquire(output_queue, &busy);
	
	if( t==nullptr )
	{
		// Output queue is empty, so
		// reverse input_queue and assign it to output_queue
		
		atomic_node * old_input = input_queue.exchange(nullptr, std::memory_order_relaxed);
		atomic_node * result = nullptr;
		atomic_node * new_output = nullptr;
		if(old_input!=nullptr)
		{
			result = old_input;
			for(atomic_node * n=old_input; n!=nullptr; )
			{
				if( n->next )
				{
					result=n->next;
					n->next = new_output;
					new_output=n;
					n=result;
				}
				else break;
			}
		}
		release(output_queue, new_output);
		return result;
	}
	else
	{
		release(output_queue, t?t->next:nullptr);
		return t;
	}
}

active::atomic_lifo::atomic_lifo() : list(nullptr) { }

void active::atomic_lifo::push(atomic_node * n)
{
	n->next=acquire(list,&busy);
	release(list,n);	
}

active::atomic_node * active::atomic_lifo::pop()
{
	atomic_node * n = acquire(list,&busy);
	release(list,n?n->next:nullptr);
	return n;
}



