#include <active_object.hpp>
#include <iostream>

/* A slightly more sophisticated example.
 * In this case, each node punts a message to its next node in a loop.
 * To make things interesting, we add lots of messages concurrently
 * and run in 16 threads.
 */

struct RoundRobin : public active::object
{
	typedef int packet;
	
	RoundRobin * next;
	
	ACTIVE_METHOD( packet )
	{
		std::cout << "Received packet " << packet << "\n";
		if( packet>0 ) (*next)(packet-1);
	}
};

int main()
{	
	// Create 1000 nodes.
	const int Count=1000;
	RoundRobin nodes[Count];
	
	// Link them together
	for(int i=0; i<Count-1; ++i) nodes[i].next = nodes+i+1;
	nodes[Count-1].next=nodes;
	
	// Send each node a packet.
	for(int i=0; i<Count; ++i) nodes[i](10);
		
	// Run all objects in 16 threads.
	active::run(16);
	return 0;
}
