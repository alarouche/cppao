#include <active/advanced.hpp>
#include <iostream>


class Computation : public active::object<Computation,active::advanced>
{
public:
	void compute( int a, int b, std::function<void(int)> result )
	{
		active_fn( [=] { result(a+b); } );
	}

	void shutdown()
	{
		active_fn( [=]{
			std::cout << "Shutting down now...\n";
			clear();
		}, 10 );	// Message priority=10
	}
};


class Display : public active::object<Display>
{
public:
	void receive_result(int result)
	{
		active_fn([=]{
				std::cout << "Result of computation = " << result << std::endl;
		});
	}
};

int main()
{
	Computation comp;
	Display display;
	comp.compute( 1,2,[&](int result){display.receive_result(result);} );
	comp.compute( 3,4,[&](int result){display.receive_result(result);} );
	comp.shutdown();
	active::run();
}
