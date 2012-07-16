#include <active/promise.hpp>
#include <iostream>

/* This example demonstrates using futures to return results.
 */

class ComplexComputation : public active::object<ComplexComputation>
{
public:
	void active_method(int a, int b, active::sink<int> * handler)
	{
		handler->send(a+b);
	}
};


int main()
{
	active::run run;	// Run threads concurrently for scope of this function.
	active::promise<int> result;
	ComplexComputation cc;
	cc(1,2,&result);
	std::cout << "Result of computation = " << result.get() << "\n";
}
