#include <active_object.hpp>
#include <iostream>

class ComputationHandler : public active::object
{
public:
	typedef int result;
	
	ACTIVE_METHOD( result ) const
	{
		std::cout << "Result of computation = " << result << std::endl;
	}
};

class ComplexComputation : public active::object
{
public:
	struct computation
	{
		int a, b;
		ComputationHandler * handler;
	};
	
	ACTIVE_METHOD( computation ) const;	 // Look we can even have const active methods
};

void ComplexComputation::ACTIVE_IMPL( computation ) const
{
	(*computation.handler)(computation.a + computation.b);
}

int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	ComplexComputation::computation comp = { 1,2,&handler };
	cc(comp);
	active::run();
	return 0;
}