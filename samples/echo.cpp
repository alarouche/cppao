
#include "active_socket.hpp"
/*
This example reads a file from stdin and sends it to the echo server.
Then it reads the response and sends it to output.
However, this loop can be repeated a specified number of times, with
data making n round-trips to the server.
*/

int main(int argc, char**argv)
{
	active::socket::ptr input( new active::socket(0));
	active::socket::ptr output( new active::socket(1));
	active::select::ptr sel(new active::select());
	active::pipe::ptr p(new active::pipe(input, output, sel));

	(*p)(active::pipe::start());
	active::run(10);
}