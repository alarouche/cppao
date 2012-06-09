#include "active_object.hpp"
#include <iostream>

class PingPong : public active::object
{
public:
	struct ping
	{
		int remaining;
	};
	
	struct pong
	{
		int remaining;
	};
	
	ACTIVE_METHOD( ping )
	{
		std::cout << "Ping\n";
		if( ping.remaining>0 )
		{
			pong p = { ping.remaining-1 };
			(*this)(p);
		}
	}
	
	ACTIVE_METHOD( pong )
	{
		std::cout << "Pong\n";
		if( pong.remaining>0 )
		{
			ping p = { pong.remaining-1 };
			(*this)(p);
		}
	}
};

int main()
{
	PingPong pp;
	PingPong::ping message = { 10 };
	pp(message);
	active::run();
	return 0;
}
