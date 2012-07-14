#include <active/object.hpp>
#include <iostream>

class ComputationHandler : public active::object
{
public:
	int ready(int result)
	{
		std::cout << "Result of computation = " << result << std::endl;
	}
};

class ComplexComputation : public active::object
{
public:
	void compute(int a, int b, ComputationHandler & result)
	{
		active_fn([a,b,&result]{result.ready(a+b);});
	}
};


int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	cc.compute(1,2,handler);
	active::run();
}
