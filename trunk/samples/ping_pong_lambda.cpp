#include "active/object.hpp"
#include <iostream>

class PingPong : public active::object<PingPong>
{
public:
	void ping(int remaining)
	{
		active_fn([=]{
			std::cout << "Ping\n";
			if( remaining>0 ) pong(remaining-1);
		});
	}
	
	void pong(int remaining)
	{
		active_fn([=]{
			std::cout << "Pong\n";
			if( remaining>0 ) ping(remaining-1);
		});
	}	
};

int main()
{
	PingPong pp;
	pp.ping(10);
	active::run();
}
