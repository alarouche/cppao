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
	struct computation
	{
		int a, b;
		ComputationHandler & handler;
	};

	void active_method( computation&&computation ) const
	{
		computation.handler(computation.a + computation.b);
	}
};

int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	ComplexComputation::computation comp = { 1,2,handler };
	cc(comp);
	active::run();
}
