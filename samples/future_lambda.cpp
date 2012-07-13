#include <active/promise.hpp>
#include <iostream>

/* This example demonstrates using futures to return results.
 */

class ComplexComputation : public active::object
{
public:
	void compute( int a, int b, std::promise<int> & result )
	{
		active_method([=,&result]{result.set_value(a+b);});
	}
};

int main()
{
	active::run run;	// Run threads concurrently for scope of this function.
	std::promise<int> result;
	ComplexComputation cc;
	cc.compute(1,2,result);
	std::cout << "Result of computation = " << result.get_future().get() << "\n";
}
