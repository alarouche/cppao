#include <active/object.hpp>
#include <cstdio>

/* A slightly more sophisticated example.
 * In this case, each node punts a message to its next node in a loop.
 * To make things interesting, we add lots of messages concurrently.
 */

struct RoundRobin : public active::object<RoundRobin>
{
   typedef int packet;

	RoundRobin * next;
	
	void send(int packet)
	{
		active_fn([=]{
			printf("Received packet %d\n", packet);
			if(packet>0) next->send(packet-1);
		});
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
	for(int i=0; i<Count; ++i) nodes[i].send(10);

	active::run();
}
