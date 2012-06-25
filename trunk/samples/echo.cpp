
#include "active_socket.hpp"

int main(int argc, char**argv)
{
	active::socket::ptr input( new active::socket(0));
	active::socket::ptr output( new active::socket(1));
	active::select::ptr sel(new active::select());
	active::pipe::ptr p(new active::pipe(input, output, sel));

	(*p)(active::pipe::start());
	active::run();
}
