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
}

active::atomic_fifo::atomic_fifo() : input_queue(nullptr), output_queue(nullptr)
{
}

void active::atomic_fifo::push(atomic_node*n)
{
	atomic_node * i;
	do
	{
		i = input_queue.load(); // std::memory_order_relaxed);
		n->next = i;
	}
	while( !input_queue.compare_exchange_weak( i, n ) ); //  , std::memory_order_relaxed) );
}

active::atomic_node * active::atomic_fifo::pop()
{
	atomic_node * t;
	int spin_count=2000;
	for(;;)
	{
		t=output_queue.load(); // std::memory_order_acquire);
		if( t==&busy )
		{
			if(!spin_count--)
			{
				// Some C++11 does not support yield() yet
				std::this_thread::sleep_for(std::chrono::seconds(0));
				spin_count=2000;
			}
		}
		else if( t==nullptr )
		{
			if( output_queue.compare_exchange_weak( t, &busy) ) // , std::memory_order_relaxed ) )
			{
				atomic_node * old_input = input_queue.exchange(nullptr); // , std::memory_order_relaxed);
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
				output_queue.store( new_output ); // , std::memory_order_release );
				return result;
			}
		}
		else
		{
			atomic_node * old_output = output_queue.exchange( &busy ); // , std::memory_order_relaxed );
			
			if( old_output != &busy )
			{
				atomic_node * result = old_output;
				if(result)
					output_queue.store( result->next ); // , std::memory_order_release );
				else
					output_queue.store( nullptr ); // , std::memory_order_release );
				return result;
			}
		}
	}
}

active::atomic_lifo::atomic_lifo() : list(nullptr) { }

void active::atomic_lifo::push(atomic_node * n)
{
	atomic_node * l;
	int spin_count=2000;
	do {
		l = list.load();
		if( l==&busy )
		{
			if( --spin_count==0)
			{
				std::this_thread::sleep_for(std::chrono::seconds(0));
				spin_count=2000;
			}
		}
		else
			n->next = l;
	} while ( l==&busy || !list.compare_exchange_weak( l, n ) );
}

active::atomic_node * active::atomic_lifo::pop()
{
	int spin_count=2000;
	atomic_node * l;
	do {
		// Was: l=list.load();
		l=list.exchange(&busy); // std::memory_order_acquire);
		if( l==&busy &&--spin_count==0 )
		{
			std::this_thread::sleep_for(std::chrono::seconds(0));
			spin_count=2000;
		}
	} while ( l==&busy );  // Was: && !list.cas(l, &busy);
	
	assert(l!=&busy);
	
	list.store(l ? l->next : nullptr); // , std::memory_order_release);
	return l;
}



