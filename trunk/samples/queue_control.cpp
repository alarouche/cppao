#include <active/advanced.hpp>
#include <iostream>

struct Shutdown { };

namespace active
{
	template<> int priority(const Shutdown&)
	{
		return 10;
	}
}


struct Result
{
	int value;
};

class Computation : public active::object<Computation,active::advanced>
{
public:
	struct Compute
	{
		int a, b;
		active::sink<Result> & result;
	};

	void active_method( Compute Compute )
	{
		Result r = { Compute.a + Compute.b };
		Compute.result.send(r);
	}

	void active_method( Shutdown )
	{
		std::cout << "Shutting down now...\n";
		clear();
	}
};


class Display : public active::object<Display>, public active::sink<Result>
{
public:
	void active_method( Result Result )
	{
		std::cout << "Result of computation = " << Result.value << std::endl;
	}
};

int main()
{
	Computation comp;
	Display display;
	Computation::Compute msg1 = {1,2,display};
	comp( msg1 );
	Computation::Compute msg2 = {4,5,display};
	comp( msg2 );
	comp( Shutdown() );
	active::run();
}
