#include <active/object.hpp>
#include <iostream>

class ComplexComputation : public active::object
{
public:
	struct computation
	{
		int a, b;
		active::sink<int> & handler;
	};

	ACTIVE_METHOD( computation ) const;
};

class ComputationHandler : public active::object, public active::sink<int>
{
public:
	typedef int result;

	ACTIVE_METHOD( result )
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
	ComplexComputation::computation msg = {1,2,handler};
	cc(msg);
	active::run();
}
