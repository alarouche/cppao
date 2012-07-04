
#ifndef ACTIVE_SOCKET_INCLUDED
#define ACTIVE_SOCKET_INCLUDED

#include <active/object.hpp>

#include <list>

#ifdef WIN32
	#include <WinSock2.h>
#else
	#include <netinet/in.h>
#endif

namespace active
{
	// Perform a select() statement on any waiting sockets.
	// Note: This is bypassed on Windows because select does not work
	// with heterogenous file descriptors on Windows.
	struct select : public shared<select>
	{
		select();
		~select();

		struct read_ready { };
		struct write_ready { };

		// Enqueue socket for reading
		// Note: must be re-queued after the read.
		struct read
		{
			int fd;
			sink<read_ready>::sp response;
		};

		ACTIVE_METHOD( read );

		// Enqueue socket for writing
		// Note: must be re-queued after the write.
		struct write
		{
			int fd;
			sink<write_ready>::sp response;
		};

		ACTIVE_METHOD( write );

	private:
		struct do_select { };
		ACTIVE_METHOD( do_select );

		struct select_loop : public object
		{
			select_loop(int interrupt);
			ACTIVE_METHOD( read );
			ACTIVE_METHOD( write );
			ACTIVE_METHOD( do_select );
		private:
			int m_interrupt_fd;
			std::list<read> m_readers;
			std::list<write> m_writers;
		};

	private:
		int m_pipe[2];
		select_loop m_loop;
	};


	/* This is a fairly complete socket implementation.
	 * It wraps POSIX/Winsock sockets and file descriptors in general.
	 */
	struct socket :
		public shared<socket>
	{
		// Tell the socket to shut down
		struct shutdown { int mode; };
		ACTIVE_METHOD( shutdown );

		// Data read from socket
		struct read_response
		{
			int error;
			void * buffer;
			int bytes_read;
		};

		struct read
		{
			void * buffer;
			int buffer_size;
			sink<read_response>::sp response;
		};

		ACTIVE_METHOD( read );

		// Write data to socket
		struct write_response
		{
			int error;
			const void * buffer;	// The remaining data which didn't get sent
			int buffer_size;	// 0 Means all data sent successfully
		};

		struct write
		{
			const void * buffer;
			int buffer_size;
			sink<write_response>::sp response;
		};

		ACTIVE_METHOD( write );

		struct accept_response
		{
			int fd;
			int error;
		};

		struct accept
		{
			sink<accept_response>::sp response;
		};

		ACTIVE_METHOD( accept );

		struct listen_result
		{
			int error;
		};

		struct listen
		{
			int backlog;
			sink<listen_result>::sp response;
		};

		ACTIVE_METHOD( listen );

		socket(int fd);
		socket(int domain, int type, int protocol);
		~socket();

		// Can be public because it's const
		const int m_fd;

		struct connect_response
		{
			int error;
		};

		struct connect_in
		{
			sockaddr_in sa;
			sink<connect_response>::sp response;
		};

		ACTIVE_METHOD(connect_in);

		struct bind_response
		{
			int result;
			int err_no;
		};

		struct bind_in
		{
			sockaddr_in sa;
			sink<bind_response>::sp response;
		};

		ACTIVE_METHOD(bind_in);
	private:
		socket(const socket&); // = delete
		socket & operator=(const socket&); // = delete

		// Perform a blocking read without blocking the whole socket
		struct reader : public object
		{
			ACTIVE_METHOD( read );
			reader(int fd);
			const int m_fd;
		} m_reader;

		// Perform a blocking write without blocking the whole object.
		struct writer : public object
		{
			ACTIVE_METHOD( write );
			writer(int fd);
			const int m_fd;
		} m_writer;
	};

	struct pipe :
		public shared<pipe>,
		sink<select::read_ready>,
		sink<select::write_ready>,
		sink<socket::read_response>,
		sink<socket::write_response>
	{
		typedef std::shared_ptr<pipe> ptr;
		struct closed { };

		pipe(	socket::ptr input,
				socket::ptr output,
				select::ptr sel,
				sink<closed>::sp closed_response = sink<closed>::sp() );

		struct start { };

		ACTIVE_METHOD(start);
	private:
		typedef select::read_ready read_ready;
		typedef select::write_ready write_ready;
		typedef socket::read_response read_response;
		typedef socket::write_response write_response;

		ACTIVE_METHOD(read_ready);
		ACTIVE_METHOD(write_ready);
		ACTIVE_METHOD(read_response);
		ACTIVE_METHOD(write_response);

		char m_buffer[4096];
		const void * m_write_buffer;
		int m_write_remaining;

		socket::ptr m_input, m_output;
		select::ptr m_select;
		sink<closed>::sp m_closed_response;
	};
} // namespace active

#endif
