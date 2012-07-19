#include <active/object.hpp>
#include <iostream>

class ComputationHandler : public active::object<ComputationHandler>
{
public:
	void active_method( int result ) const
	{
		std::cout << "Result of computation = " << result << std::endl;
	}
};

class ComplexComputation : public active::object<ComplexComputation>
{
public:
	void active_method( int a, int b, ComputationHandler *handler )
	{
		(*handler)(a + b);
	}
};

int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	cc(1,2,&handler);
	active::run();
}
