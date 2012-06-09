#include <active_object.hpp>
#include <iostream>

class HelloActive : public active::object
{
public:
	struct Greet { const char * message; };
	
	ACTIVE_METHOD( Greet )
	{
		std::cout << Greet.message << std::endl;
	}
};

int main()
{
	HelloActive hello;
	HelloActive::Greet message = { "Hello, world!" };
	hello(message);
	active::run();
	return 0;
}