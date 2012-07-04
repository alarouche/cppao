#include <active/promise.hpp>
#include <iostream>

/* This example demonstrates using futures to return results.
 */

class ComplexComputation : public active::object
{
public:
	struct computation
	{
		int a, b;
		active::sink<int> & result;
	};

	ACTIVE_METHOD( computation )
	{
		computation.result(computation.a + computation.b );
	}
};


int main()
{
	active::run run;	// Run threads concurrently for scope of this function.
	active::promise<int> result;
	ComplexComputation cc;
	cc(ComplexComputation::computation({ 1,2, result }));
	std::cout << "Result of computation = " << result.get() << "\n";
}
