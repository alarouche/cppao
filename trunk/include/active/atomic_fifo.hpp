#ifndef CPPAO_ATOMIC_FIFO_INCLUDED
#define CPPAO_ATOMIC_FIFO_INCLUDED

#include <atomic>
#include "atomic_node.hpp"

namespace active
{
	class atomic_fifo
	{
	public:
		atomic_fifo();
		void push(atomic_node*n);
		atomic_node * pop();
	private:
		std::atomic<atomic_node*> input_queue, output_queue;
	};	
}

#endif
