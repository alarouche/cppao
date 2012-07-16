#include "active_socket.hpp"

#include <iostream>
#include <cstdio>
#ifdef WIN32
#include <Ws2tcpip.h>
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif

struct my_server :
	public active::shared<my_server>,
	active::sink<active::socket::bind_response>,
	active::sink<active::socket::listen_result>,
	active::sink<active::socket::accept_response>,
	active::sink<active::select::read_ready>
{
	typedef active::socket::bind_response bind_response;

	void active_method( bind_response bind_response )
	{
		if( bind_response.result == -1 )
		{
			perror("bind");
		}
		else
		{
			std::cout << "Accepting TCP connections on port " << m_port << "\n";

			active::socket::listen listen = { 100, shared_from_this() };
			(*m_sock)(listen);

		}
	}

	typedef active::socket::listen_result listen_result;

	void active_method( listen_result listen_result )
	{
		if( !listen_result.error )
		{
			active::select::read read = { m_sock->m_fd, shared_from_this() };
			(*m_select)(read);
		}
		else
		{
			perror("listen");
		}
	}

	typedef active::select::read_ready read_ready;

	void active_method( read_ready read_ready )
	{
		active::socket::accept accept = { shared_from_this() };
		(*m_sock)(accept);
	}

	struct connection: public active::shared<connection>
	{
		connection(int fd, active::select::ptr select) :
			m_sock(new active::socket(fd)), m_select(select)
		{
		}

		struct start { };

		void active_method( start start )
		{
			m_pipe.reset( new active::pipe(m_sock, m_sock, m_select ) );
			(*m_pipe)(active::pipe::start());
		}

	private:
		active::socket::ptr m_sock;
		active::select::ptr m_select;
		active::pipe::ptr m_pipe;
	};

	typedef active::socket::accept_response accept_response;

	void active_method( accept_response accept_response )
	{
		if( !accept_response.error )
		{
			connection::ptr conn( new connection(accept_response.fd, m_select) );
			(*conn)(connection::start());
		}

		// Wait for the socket to again be ready to accept
		active::select::read read = { m_sock->m_fd, shared_from_this() };
		(*m_select)(read);
	}

	struct start { int port; };

	my_server() :
		m_select(new active::select()),
		m_sock(new active::socket(AF_INET, SOCK_STREAM, 0))
	{
	}

	void active_method(start start)
	{
		m_port = start.port;

		active::socket::bind_in bind;

		bind.sa.sin_family = AF_INET;
		bind.sa.sin_port = htons(start.port);
		inet_pton(AF_INET, "0.0.0.0", &bind.sa.sin_addr);

		bind.response = shared_from_this();
		(*m_sock)(bind);
	}

	active::select::ptr m_select;
private:
	active::socket::ptr m_sock;
	int m_port;
};


int main(int argc, char**argv)
{
#if WIN32
	WSADATA wsaData;
	WSAStartup( MAKEWORD(2, 2), &wsaData );
#endif
	const int port = argc<2 ? 12345 : atoi(argv[1]);
	const int num_threads = argc<3 ? 5 : atoi(argv[2]);
	if( num_threads<2 ) std::cerr << "Warning: server needs at least 2 threads\n";

	my_server::ptr server(new my_server());

	my_server::start start = { port };
	(*server)(start);

	active::run r(num_threads);	// Morally: active::run{num_threads}
}
