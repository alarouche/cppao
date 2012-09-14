
#ifndef ACTIVE_ATOMIC_LIFO_INCLUDED
#define ACTIVE_ATOMIC_LIFO_INCLUDED

#include <atomic>
#include "atomic_node.hpp"

namespace active
{
	class atomic_lifo
	{
	public:
		atomic_lifo();
		void push(atomic_node * n);
		atomic_node * pop();
	private:
		std::atomic<atomic_node*> list;
	};
}

#endif
