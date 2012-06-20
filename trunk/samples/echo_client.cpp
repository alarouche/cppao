#include "active_socket.hpp"
#include <iostream>
#include <vector>
#include <cassert>

#ifdef WIN32
#include <Ws2tcpip.h>
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif

struct connected
{
    active::socket::ptr sock;
};

struct echo_client :
    public active::shared<echo_client>,
    active::sink<connected>,
    active::sink<active::socket::connect_response>
{
    // 1) Connect to server
    struct init
    {
        std::string host;
        int port;
        active::select::ptr select;
        active::sink<connected>::ptr response;
    };

    echo_client() : m_sock( new active::socket(AF_INET, SOCK_STREAM, 0) )
    {
    }

    ACTIVE_METHOD(init)
    {
        m_select = init.select;

        active::socket::connect_in sc;

        sc.sa.sin_family = AF_INET;
        sc.sa.sin_port = htons(init.port);
        inet_pton(AF_INET, init.host.c_str(), &sc.sa.sin_addr);

        sc.response = shared_from_this();

        (*m_sock)(sc);
        assert( init.response );
        m_notify_connection = init.response;
        m_connected = false;
    }

    typedef active::socket::connect_response connect_response;

    // 2a) Connected
    ACTIVE_METHOD( connect_response )
    {
        if( connect_response.error )
        {
            std::cout << "Connection failed: " << connect_response.error << "\n";
        }
        else
        {
            m_connected = true;
            connected c = { m_sock };
            (*m_notify_connection)( c );

            if( m_pipe )
            {
                // Start the pipe
                (*m_pipe)(active::pipe::start());
            }
        }
    };

    // 2b) Something else connected to us
    // However we don't know which will happen first:
    ACTIVE_METHOD( connected )
    {
        m_pipe.reset( new active::pipe( m_sock, connected.sock, m_select ) );

        if( m_connected )
        {
            // Start the pipe
            (*m_pipe)(active::pipe::start());
        }
    }

private:
    bool m_connected;
    active::socket::ptr m_sock, m_sink;
    active::sink<connected>::ptr m_notify_connection;
    active::select::ptr m_select;
    active::pipe::ptr m_pipe;
};


struct input : public active::object,
    public active::sink<connected>
{
    input( active::select::ptr sel ) : m_socket(new active::socket(0)), m_select(sel)
    {
    }

    ACTIVE_METHOD( connected )
    {
        // !! Watch out exceptions ???
        m_pipe.reset( new active::pipe(m_socket, connected.sock, m_select ) );
        (*m_pipe)(active::pipe::start());
    }
private:
    active::socket::ptr m_socket;
    active::select::ptr m_select;
    active::pipe::ptr m_pipe;
};


int main(int argc, char**argv)
{
#if WIN32
    WSADATA wsaData;
    WSAStartup( MAKEWORD(2, 2), &wsaData );
#endif

    const int port = argc<2 ? 12345 : atoi(argv[1]);
    const int num_clients = argc<3 ? 10 : atoi(argv[2]);
    const int num_threads = argc<4 ? 5 : atoi(argv[3]);

    active::select::ptr select(new active::select());

    input::ptr in(new input(select));
    active::socket::ptr out(new active::socket(1));

    std::vector<std::shared_ptr<echo_client>> client_list( num_clients );
    for(int c=0; c<num_clients; ++c)
    {
        client_list[c].reset( new echo_client() );
        echo_client::init init = { "127.0.0.1", port, select };
        if( c==0 ) init.response = in;
        else init.response = client_list[c-1];
        (*client_list[c])(init);
    }

    connected c = { out };

    if( num_clients>0 )
        (*client_list[num_clients-1])(c);
    else
        (*in)(c);

    active::run(num_threads);
}
