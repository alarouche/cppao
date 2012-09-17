#undef NDEBUG
#include <active/atomic_fifo.hpp>
#include <active/atomic_lifo.hpp>

#include <vector>
#include <cassert>

template<typename Fifo>
void test_fifo()
{
	Fifo q;
	std::vector<active::atomic_node> atomic_nodes(10000000);
	for( auto & n : atomic_nodes )
	{
		q.push(&n);
	}
	for( auto & n : atomic_nodes )
	{
		active::atomic_node * m = q.pop();
		assert(m==&n);
	}
	assert( !q.pop() );
}

template<typename Stack>
void test_stack()
{
	Stack q;
	std::vector<active::atomic_node> atomic_nodes(10000000);
	for( auto & n : atomic_nodes )
	{
		q.push(&n);
	}
	for( auto n = atomic_nodes.rbegin(); n!=atomic_nodes.rend(); ++n )
	{
		active::atomic_node * m = q.pop();
 		assert(m==&*n);
	}
	assert( !q.pop() );
}

int main(int argc, const char * argv[])
{
	test_fifo<active::atomic_fifo>();
	test_stack<active::atomic_lifo>();
    return 0;
}
