#include <active/object.hpp>
#include <iostream>

class ComplexComputation : public active::object
{
public:
	struct result_handler
	{
		typedef int result;
		ACTIVE_IFACE( result );
	};

	struct computation
	{
		int a, b;
		result_handler & handler;
	};

	ACTIVE_METHOD( computation ) const;
};

class ComputationHandler : public active::object, public ComplexComputation::result_handler
{
public:
	typedef int result;

	ACTIVE_METHOD( result ) const
	{
		std::cout << "Result of computation = " << result << std::endl;
	}
};

void ComplexComputation::ACTIVE_IMPL( computation ) const
{
	computation.handler(computation.a + computation.b);
}

int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	cc(ComplexComputation::computation({ 1,2,handler }));
	active::run();
}
