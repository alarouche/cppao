#include <active_object.hpp>
#include <iostream>
#include <future>

/* This example demonstrates using futures to return results (via a std::promise).
 */

class ComplexComputation : public active::object
{
public:
	struct computation
	{
		int a, b;
		std::promise<int> * result;
	};
		
	ACTIVE_METHOD( computation )
	{
		computation.result->set_value( computation.a + computation.b );
	}	
};


int main()
{
	std::promise<int> result;
	ComplexComputation cc;
	ComplexComputation::computation comp = { 1,2, &result };
	cc(comp);
	cc.run();
	
	std::cout << "Result of computation = " << result.get_future().get() << "\n";
}