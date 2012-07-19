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

class Computation : public active::object<Computation,active::advanced>
{
public:
	void active_method( int a, int b, active::sink<int> * result )
	{
		result->send(a+b);
	}

	void active_method( Shutdown )
	{
		std::cout << "Shutting down now...\n";
		clear();
	}
};


class Display : public active::object<Display>, public active::sink<int>
{
public:
	void active_method( int result )
	{
		std::cout << "Result of computation = " << result << std::endl;
	}
};

int main()
{
	Computation comp;
	Display display;
	comp( 1,2,&display );
	comp( 3,4,&display );
	comp( Shutdown() );
	active::run();
}
