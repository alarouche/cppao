#include <active/object.hpp>
#include <iostream>

class HelloActive : public active::object<HelloActive>
{
public:
	void active_method(const char * sender, const char * msg)
	{
		std::cout << sender << " says " << msg << "!\n";
	}
};

int main()
{
	HelloActive hello;
	hello("Bob", "hello");
	active::run();
}
