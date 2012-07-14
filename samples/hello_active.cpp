#include <active/object.hpp>
#include <iostream>

class HelloActive : public active::object<HelloActive>
{
public:
	void active_method(const char * msg1, const char * msg2)
	{
		std::cout << msg1 << ", " << msg2 << "!\n";
	}
};

int main()
{
	HelloActive hello;
	hello("Hello", "world");
	active::run();
}
