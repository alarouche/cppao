#include <active/advanced.hpp>
#include <iostream>

class Computation : public active::object<Computation,active::advanced>
{
public:
	void compute( int a, int b, std::function<void(int)> result )
	{
		active_fn( [=] { 
			while(true)
			{
				std::cout << "Thinking..." << std::endl;
				if( get_priority()==10 ) break;
			}
		} );
	}

	void shutdown()
	{
		active_fn( [=]{
			clear();
			std::cout << "Shut down\n";
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
	active::run run;
	comp.compute( 1,2,[&](int result){display.receive_result(result);} );
	comp.compute( 3,4,[&](int result){display.receive_result(result);} );
#if ACTIVE_USE_BOOST
	boost::this_thread::sleep_for(boost::posix_time::seconds(1));
#else
	std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
	comp.shutdown();
}
