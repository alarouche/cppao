#include <active/advanced.hpp>
#include <active/sink.hpp>
#include <iostream>

/* The "advanced" object type offers more features.
   Here we "interrupt" some long piece of work by peeking at the message queue
   and determining that a method of priority 10 is waiting.
  */

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
		while(true)
		{
			std::cout << "Thinking..." << std::endl;
			if( get_priority()==10 ) break;	// Interruptable work
		}
	}

	void active_method( Shutdown )
	{
		clear();
		std::cout << "Shut down\n";
	}
};


class Display : public active::object<Display>, public active::handle<Display,int>
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
	active::run run;
	comp(1,2,&display)(3,4,&display);
#ifdef ACTIVE_USE_BOOST
	boost::this_thread::sleep(boost::posix_time::seconds(1));
#else
	std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
	comp( Shutdown() );
}
