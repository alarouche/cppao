#include <active/object.hpp>
#include <iostream>

class ComplexComputation : public active::object<ComplexComputation>
{
public:
	void active_method(int a, int b, active::platform::function<void(int)> result)
	{
		result(a+b);
	}
};

class ComputationHandler : public active::object<ComputationHandler>
{
public:
	void active_method(int result)
	{
		std::cout << "Result of computation = " << result << std::endl;
	}
};

int main()
{
	ComputationHandler handler;
	ComplexComputation cc;
	cc(1,2,[&](int i){handler(i);});
	active::run();
}
