#include <active_object.hpp>
#include <iostream>

struct Result
{
	int value;
};

class Computation : public active::object
{
public:
	struct Compute
	{
		int a, b;
		active::sink<Result> & result;
	};
	
	ACTIVE_METHOD( Compute )
	{
		Result r = { Compute.a + Compute.b };
		Compute.result(r);
	}
	
	struct Shutdown { };
	
	ACTIVE_METHOD( Shutdown )
	{
		std::cout << "Shutting down now...\n";
		clear();
	}
};

template<> 
int active::priority<Computation::Shutdown>(const Computation::Shutdown&) 
{ 
	return 10; 
}

class Display : public active::object, public active::sink<Result>
{
public:
	ACTIVE_METHOD( Result )
	{
		std::cout << "Result of computation = " << Result.value << std::endl;
	}
};

int main()
{
	Computation comp;
	Display display;
	comp( Computation::Compute({1,2,display}) );
	comp( Computation::Compute({4,5,display}) );
	comp( Computation::Shutdown() );
	active::run();
}