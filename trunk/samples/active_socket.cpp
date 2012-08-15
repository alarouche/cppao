#include "active_socket.hpp"
#include <iostream>
#include <cassert>

#ifdef WIN32
	#include <io.h>
	#define SHUT_WR SD_SEND
	#define SHUT_RD SD_RECEIVE
	#define SHUT_RDWR SD_BOTH
	#undef errno
	#define errno WSAGetLastError()
	#define ENABLE_SELECT 0
	#undef max
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#define _close close
	#define _write write
	#define _read read
	#define ENABLE_SELECT 1
	#define closesocket close
#endif

active::socket::socket(int fd) : m_fd(fd), m_reader(fd), m_writer(fd)
{
}

active::socket::~socket()
{
	if( m_fd != -1 )
	{
		::closesocket(m_fd);
	}
}

active::socket::socket(int domain, int type, int protocol) :
	m_fd( ::socket( domain, type, protocol ) ), m_reader(m_fd), m_writer(m_fd)
{
	if( m_fd == -1 ) throw std::runtime_error("Could not create socket");
}

void active::socket::active_method( connect_in connect_in )
{
	connect_response response;
	int result = ::connect( m_fd, (sockaddr*)&connect_in.sa, sizeof(connect_in.sa) );
	response.error = result ? errno : 0;

	if( connect_in.response )
	{
		connect_in.response->send(response);
	}
};

void active::socket::active_method( bind_in bind_in )
{
	sockaddr_in response = bind_in.sa;
	int result = ::bind( m_fd, (sockaddr*)&response, sizeof( bind_in.sa ) );

	if( bind_in.response )
	{
		bind_response br = { result, errno };
		bind_in.response->send(br);
	}
}

void active::socket::active_method( listen listen )
{
	int result = ::listen( m_fd, listen.backlog );

	if( listen.response )
	{
		listen_result result_msg = { result == -1 ? 0 : errno };
		listen.response->send(result_msg);
	}
}

void active::socket::active_method( accept accept )
{
	accept_response response;
	response.fd = ::accept( m_fd, 0, 0 );
	response.error = response.fd>=0 ? 0 : errno;

	if( accept.response )
	{
		accept.response->send(response);
	}
}

void active::socket::active_method( write write )
{
	m_writer(write);
}

void active::socket::writer::active_method( write write )
{
	active::socket::write_response response;

	int bytes = m_fd<=2 ?
		::_write( m_fd, write.buffer, write.buffer_size ) :
		::send( m_fd, (const char*)write.buffer, write.buffer_size, 0 );

	if( bytes <= 0 )
	{
		response.buffer = write.buffer;
		response.buffer_size = write.buffer_size;
		response.error = bytes==0 ? -1 : errno;
	}
	else
	{
		response.buffer_size = write.buffer_size - bytes;
		response.buffer = (char*)write.buffer + bytes;
		response.error = 0;
	}
	if( write.response )
		write.response->send(response);
}

void active::socket::active_method( read read )
{
	m_reader(read);
}

void active::socket::reader::active_method( read read )
{
	active::socket::read_response response = { 0 };

	response.buffer = read.buffer;

	response.bytes_read = m_fd <=2 ?
		::_read( m_fd, read.buffer, read.buffer_size ) :
		::recv( m_fd, (char*)read.buffer, read.buffer_size, 0 );

	if( response.bytes_read>0 )
		response.error = 0;
	else if( response.bytes_read==0 )
		response.error = -1;
	else
		response.error = errno;

	if( read.response )
		read.response->send(response);
}

void active::socket::active_method( shutdown shutdown )
{
	::shutdown( m_fd, shutdown.mode );
}

active::socket::reader::reader(int fd) : m_fd(fd)
{
}

active::socket::writer::writer(int fd) : m_fd(fd)
{
}

/////////////////////////////////////////////////////////////////////
// Select

active::select::select() :
#if ENABLE_SELECT
	m_loop( (::pipe(m_pipe) >=0 ? m_pipe[0] : -1) )
#else
	m_loop( -1 )
#endif
{
}

void active::select::active_method( read read )
{
#if ENABLE_SELECT
	// Note: We must queue the write BEFORE we interrupt the select
	m_loop(read);
	::_write( m_pipe[1], "X", 1 );
#else
	read.response->send(read_ready());
#endif
}

void active::select::active_method( write write )
{
#if ENABLE_SELECT
	m_loop(write);
	::_write( m_pipe[1], "X", 1 );
#else
	write.response->send(write_ready());
#endif
}

active::select::~select()
{
#if ENABLE_SELECT
	::_close(m_pipe[0]);
	::_close(m_pipe[1]);
#endif
}

active::select::select_loop::select_loop(int fd) : m_interrupt_fd(fd)
{
}

void active::select::select_loop::active_method( do_select do_select )
{
	if( m_readers.empty() && m_writers.empty()) return;

	int max_fd=m_interrupt_fd;

	fd_set rfds, wfds;

	FD_ZERO( &rfds );
	FD_ZERO( &wfds );

	FD_SET( m_interrupt_fd, &rfds );

	for( std::list<read>::const_iterator r=m_readers.begin(); r!=m_readers.end(); ++r)
	{
		assert( r->fd>=0 );	// Could happen if we select a deleted/closed socket
		assert( r->response );	// Semantic bug
		FD_SET( r->fd, &rfds );
		max_fd = std::max( max_fd, r->fd );
	}

	for( std::list<write>::const_iterator w=m_writers.begin(); w!=m_writers.end(); ++w )
	{
		assert( w->fd>=0 );
		assert( w->response );
		FD_SET( w->fd, &wfds );
		max_fd = std::max( max_fd, w->fd );
	}

#if DEBUG_SELECT
	std::cout << "Selecting " << (m_readers.size()) << " readers, " << m_writers.size() << " writers\n";
	int result =
#endif
		::select( max_fd+1, &rfds, &wfds, NULL, NULL );
#if DEBUG_SELECT
	std::cout << "Select complete: " << result << std::endl;
	if( result<0 ) std::cerr << "Select error code: " << errno << std::endl;
#endif

	for( std::list<read>::iterator r = m_readers.begin(); r!=m_readers.end(); )
	{
		if( FD_ISSET( r->fd, &rfds ) )
		{
			r->response->send(read_ready());
			r = m_readers.erase(r);
		}
		else
		{
			++r;
		}
	}

	for( std::list<write>::iterator w=m_writers.begin(); w!=m_writers.end(); ++w )
	{
		if( FD_ISSET( w->fd, &wfds ) )
		{
			w->response->send(write_ready());
			w = m_writers.erase(w);
		}
		else
		{
			++w;
		}
	}

	if( FD_ISSET( m_interrupt_fd, &rfds ) )
	{
		char ch[16];
		::_read( m_interrupt_fd, ch, sizeof(ch) );
	}

	if( !m_readers.empty() || !m_writers.empty() )
		(*this)(select::do_select());
}


void active::select::select_loop::active_method( read read )
{
	if( m_readers.empty() && m_writers.empty() )
		(*this)(do_select());
	m_readers.push_back(read);
}

void active::select::select_loop::active_method( write write )
{
	//std::cout << "select_loop::write\n";
	if( m_readers.empty() && m_writers.empty() )
		(*this)(do_select());
	m_writers.push_back(write);
}


////////////////////////////////////////////////////////////////////////
// Pipe

active::pipe::pipe(socket::ptr input,
				   socket::ptr output,
				   select::ptr sel,
				   sink<closed>::sp closed_response ) :
	m_input(input), m_output(output), m_select(sel), m_closed_response(closed_response)
{
}

void active::pipe::active_method( start start )
{
	select::read read = { m_input->m_fd, shared_from_this() };
	(*m_select)(read);
}

void active::pipe::active_method( read_ready read_ready )
{
	socket::read read = { m_buffer, sizeof(m_buffer), shared_from_this() };
	(*m_input)(read);
}

void active::pipe::active_method( read_response read_response )
{
	if( read_response.error )
	{
		socket::shutdown sr = { SHUT_RD };
		(*m_input)(sr);
		socket::shutdown sw = { SHUT_WR };
		(*m_output)(sw);

		if( m_closed_response )
			m_closed_response->send(closed());
	}
	else
	{
		m_write_buffer = m_buffer;
		m_write_remaining = read_response.bytes_read;
		select::write write = { m_output->m_fd, shared_from_this() };
		(*m_select)(write);
	}
}

void active::pipe::active_method( write_ready write_ready )
{
	socket::write write = { m_write_buffer, m_write_remaining, shared_from_this() };
	(*m_output)(write);
}

void active::pipe::active_method( write_response write_response )
{
	if( write_response.error )
	{
		if( m_closed_response )
			m_closed_response->send(closed());
	}
	else
	{
		if( write_response.buffer_size )
		{
			// For some reason, not all data could be written
			// So we enqueue another write
			m_write_buffer = write_response.buffer;
			m_write_remaining = write_response.buffer_size;
			select::write write = { m_output->m_fd, shared_from_this() };
			(*m_select)(write);
		}
		else
		{
			select::read read = { m_input->m_fd, shared_from_this() };
			(*m_select)(read);
		}
	}
}

void active::pipe::send(read_ready msg)
{
	(*this)(msg);
}

void active::pipe::send(write_ready msg)
{
	(*this)(msg);
}

void active::pipe::send(read_response msg)
{
	(*this)(msg);
}

void active::pipe::send(write_response msg)
{
	(*this)(msg);
}
