#include <active/object.hpp>
#include <iostream>

class ComplexComputation : public active::object<ComplexComputation>
{
public:	
	void computation(int a, int b, std::function<void(int)> result)
	{
		active_fn([=]{result(a+b);});
	}
};

class ComputationHandler : public active::object<ComputationHandler>
{
public:
	void handle_result(int result)
	{
		active_fn([=]{
			std::cout << "Result of computation = " << result << std::endl;
		});
	}
};

int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	cc.computation(1,2,[&](int i){handler.handle_result(i);});
	active::run();
}
