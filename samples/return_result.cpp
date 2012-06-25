#include <active_object.hpp>
#include <iostream>

/* This example demonstrates a basic approach to returning results.
 * It also shows that we can actually have messages which can be pointers
 * (including shared pointers), to avoid message content copying if that is undesirable.
 * Also shows implementation outside of class.
 */

class ComplexComputation : public active::object
{
public:
	struct computation
	{
		int a, b, result;
	};

	typedef computation * computationp;

	// Example of a forward declaration
	ACTIVE_METHOD( computationp ) const;	 // Look we can even have const active methods
};

void ComplexComputation::ACTIVE_IMPL( computationp ) const
{
	computationp->result = computationp->a + computationp->b;
}

int main()
{
	ComplexComputation cc;
	ComplexComputation::computation comp = { 1,2,0 };
	cc(&comp);
	cc.run();
	std::cout << "Result of computation = " << comp.result << "\n";
}
