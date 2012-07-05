#include <active/object.hpp>
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
	HelloActive::Greet msg = {"Hello, world!"};
	hello(msg);
	active::run();
}
