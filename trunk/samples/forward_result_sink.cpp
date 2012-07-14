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

	void active_method(computation&&c)
	{
		c.handler.send(c.a + c.b);
	}
};

class ComputationHandler : 
	public active::object<ComputationHandler>,
	public active::sink<int>
{
public:
	void active_method(int &&result)
	{
		std::cout << "Result of computation = " << result << std::endl;
	}
};


int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	ComplexComputation::computation msg = {1,2,handler};
	cc(msg);
	active::run();
}
