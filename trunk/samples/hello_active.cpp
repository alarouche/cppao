#include <active/object.hpp>
#include <iostream>

class HelloActive : public active::object
{
public:
	typedef const char * msg;
	ACTIVE_METHOD(msg)
	{
		std::cout << msg << std::endl;
	}
};

int main()
{
	HelloActive hello;
	hello("Hello, world!");
	active::run();
}
