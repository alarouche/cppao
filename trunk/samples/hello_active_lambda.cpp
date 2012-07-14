#include <active/object.hpp>
#include <iostream>

class HelloActive : public active::object
{
public:	
	void greet(const char * msg)
	{
		active_fn([=]{ std::cout << msg << std::endl; });
	}
};

int main()
{
	HelloActive hello;
	hello.greet("Hello, world!");
	active::run();
}
