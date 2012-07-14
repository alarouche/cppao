#include <active/object.hpp>
#include <iostream>

class ComplexComputation : public active::object<ComplexComputation>
{
public:
	struct computation
	{
		int a, b;
		active::sink<int> & handler;
	};

	void compute( int a, int b, active::sink<int> & handler )
	{
		active_fn([=,&handler]{handler.send(a+b);});
	}
};

class ComputationHandler : 
	public active::object<ComputationHandler>, 
	public active::sink<int>
{
public:
	typedef int result;

	void active_method(int &&result)
	{
		std::cout << "Result of computation = " << result << std::endl;
	}
};

int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	cc.compute(1,2,handler);
	active::run();
}
