#include <active/promise.hpp>
#include <iostream>

/* This example demonstrates using futures to return results.
 */

class ComplexComputation : public active::object<ComplexComputation>
{
public:
	void compute( int a, int b, active::platform::promise<int> & result )
	{
		active_fn([=,&result]{result.set_value(a+b);});
	}
};

int main()
{
	active::platform::promise<int> result;
	ComplexComputation cc;
	active::run run;	// Run threads concurrently for scope of this function.
	cc.compute(1,2,result);
	std::cout << "Result of computation = " << active::wait(result) << "\n";
}
