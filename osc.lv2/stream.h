/*
 * Copyright (c) 2015-2016 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef LV2_OSC_STREAM_H
#define LV2_OSC_STREAM_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <net/if.h>
#	include <netinet/tcp.h>
#	include <netinet/in.h>
#	include <netdb.h>
#	include <termios.h>
#	include <limits.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>

#include <osc.lv2/osc.h>

#if !defined(LV2_OSC_STREAM_SNDBUF)
#	define LV2_OSC_STREAM_SNDBUF 0x100000 // 1 M
#endif

#if !defined(LV2_OSC_STREAM_RCVBUF)
#	define LV2_OSC_STREAM_RCVBUF 0x100000 // 1 M
#endif

#if !defined(LV2_OSC_STREAM_REQBUF)
#	define LV2_OSC_STREAM_REQBUF 1024
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *
(*LV2_OSC_Stream_Write_Request)(void *data, size_t minimum, size_t *maximum);

typedef void
(*LV2_OSC_Stream_Write_Advance)(void *data, size_t written);

typedef const void *
(*LV2_OSC_Stream_Read_Request)(void *data, size_t *toread);

typedef void
(*LV2_OSC_Stream_Read_Advance)(void *data);

typedef struct _LV2_OSC_Address LV2_OSC_Address;
typedef struct _LV2_OSC_Driver LV2_OSC_Driver;
typedef struct _LV2_OSC_Stream LV2_OSC_Stream;

struct _LV2_OSC_Address {
	socklen_t len;
	union {
		struct sockaddr_in in4;
		struct sockaddr_in6 in6;
	};
};

struct _LV2_OSC_Driver {
	LV2_OSC_Stream_Write_Request write_req;
	LV2_OSC_Stream_Write_Advance write_adv;
	LV2_OSC_Stream_Read_Request read_req;
	LV2_OSC_Stream_Read_Advance read_adv;
};

struct _LV2_OSC_Stream {
	int socket_family;
	int socket_type;
	int protocol;
	bool server;
	bool slip;
	bool serial;
	bool connected;
	int sock;
	int fd;
	LV2_OSC_Address self;
	LV2_OSC_Address peer;
	const LV2_OSC_Driver *driv;
	void *data;
	uint8_t tx_buf [0x4000];
	uint8_t rx_buf [0x4000];
	size_t rx_off;
	char url [PATH_MAX];
};

typedef enum _LV2_OSC_Enum {
	LV2_OSC_NONE = 0x000000,

	LV2_OSC_SEND = 0x800000,
	LV2_OSC_RECV = 0x400000,
	LV2_OSC_CONN = 0x200000,

	LV2_OSC_ERR  = 0x00ffff
} LV2_OSC_Enum;

static const char *udp_prefix = "osc.udp://";
static const char *tcp_prefix = "osc.tcp://";
static const char *tcp_slip_prefix = "osc.slip.tcp://";
static const char *tcp_prefix_prefix = "osc.prefix.tcp://";
static const char *ser_prefix = "osc.serial://";
//FIXME serial


static int
_lv2_osc_stream_interface_attribs(int fd, int speed)
{
	struct termios tty;

	if(tcgetattr(fd, &tty) < 0)
	{
		return -1;
	}

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;         /* 8-bit characters */
	tty.c_cflag &= ~PARENB;     /* no parity bit */
	tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
	tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

	/* setup for non-canonical mode */
	tty.c_iflag &= ~(IGNCR | ONLCR | IXON);
	tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tty.c_oflag &= ~OPOST;

	/* fetch bytes as they become available */
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;

	if(tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		return -1;
	}

	return 0;
}

#define LV2_OSC_STREAM_ERRNO(EV, ERRNO) ( (EV & (~LV2_OSC_ERR)) | (ERRNO) )

static void
_close_socket(int *fd)
{
	if(fd)
	{
		if(*fd >= 0)
		{
			close(*fd);
		}

		*fd = -1;
	}
}

static int
lv2_osc_stream_deinit(LV2_OSC_Stream *stream)
{
	_close_socket(&stream->fd);
	_close_socket(&stream->sock);

	return 0;
}

static int
_lv2_osc_stream_reinit(LV2_OSC_Stream *stream)
{
	LV2_OSC_Enum ev = LV2_OSC_NONE;
	lv2_osc_stream_deinit(stream);

	char *dup = strdup(stream->url);
	if(!dup)
	{
		ev = LV2_OSC_STREAM_ERRNO(ev, ENOMEM);
		goto fail;
	}

	char *ptr = dup;
	char *tmp;

	if(strncmp(ptr, udp_prefix, strlen(udp_prefix)) == 0)
	{
		stream->slip = false;
		stream->socket_family = AF_INET;
		stream->socket_type = SOCK_DGRAM;
		stream->protocol = IPPROTO_UDP;
		ptr += strlen(udp_prefix);
	}
	else if(strncmp(ptr, tcp_prefix, strlen(tcp_prefix)) == 0)
	{
		stream->slip = true;
		stream->socket_family = AF_INET;
		stream->socket_type = SOCK_STREAM;
		stream->protocol = IPPROTO_TCP;
		ptr += strlen(tcp_prefix);
	}
	else if(strncmp(ptr, tcp_slip_prefix, strlen(tcp_slip_prefix)) == 0)
	{
		stream->slip = true;
		stream->socket_family = AF_INET;
		stream->socket_type = SOCK_STREAM;
		stream->protocol = IPPROTO_TCP;
		ptr += strlen(tcp_slip_prefix);
	}
	else if(strncmp(ptr, tcp_prefix_prefix, strlen(tcp_prefix_prefix)) == 0)
	{
		stream->slip = false;
		stream->socket_family = AF_INET;
		stream->socket_type = SOCK_STREAM;
		stream->protocol = IPPROTO_TCP;
		ptr += strlen(tcp_prefix_prefix);
	}
	else if(strncmp(ptr, ser_prefix, strlen(ser_prefix)) == 0)
	{
		stream->slip = true;
		stream->serial = true;
		ptr += strlen(ser_prefix);
	}
	else
	{
		ev = LV2_OSC_STREAM_ERRNO(ev, ENOPROTOOPT);
		goto fail;
	}

	if(ptr[0] == '\0')
	{
		ev = LV2_OSC_STREAM_ERRNO(ev, EDESTADDRREQ);
		goto fail;
	}

	if(stream->serial)
	{
		stream->sock = open(ptr, O_RDWR | O_NOCTTY | O_NDELAY);
		if(stream->sock < 0)
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			goto fail;
		}

		if(fcntl(stream->sock, F_SETFL, FNDELAY) == -1) //FIXME
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			goto fail;
		}

		if(_lv2_osc_stream_interface_attribs(stream->sock, B115200) == -1)
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			goto fail;
		}

		stream->connected = true;
	}
	else // !stream->serial
	{
		const char *node = NULL;
		const char *iface = NULL;
		const char *service = NULL;

		// optional IPv6
		if(ptr[0] == '[')
		{
			stream->socket_family = AF_INET6;
			++ptr;
		}

		node = ptr;

		// optional IPv6
		if( (tmp = strchr(ptr, '%')) )
		{
			if(stream->socket_family != AF_INET6)
			{
				ev = LV2_OSC_STREAM_ERRNO(ev, EPROTOTYPE);
				goto fail;
			}

			ptr = tmp;
			ptr[0] = '\0';
			iface = ++ptr;
		}

		// optional IPv6
		if( (tmp = strchr(ptr, ']')) )
		if(ptr)
		{
			if(stream->socket_family != AF_INET6)
			{
				ev = LV2_OSC_STREAM_ERRNO(ev, EDESTADDRREQ);
				goto fail;
			}

			ptr = tmp;
			ptr[0] = '\0';
			++ptr;
		}

		// mandatory IPv4/6
		ptr = strchr(ptr, ':');
		if(!ptr)
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, EDESTADDRREQ);
			goto fail;
		}

		ptr[0] = '\0';

		service = ++ptr;

		if(strlen(node) == 0)
		{
			node = NULL;
			stream->server = true;
		}

		stream->sock = socket(stream->socket_family, stream->socket_type,
			stream->protocol);

		if(stream->sock < 0)
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			goto fail;
		}

		if(fcntl(stream->sock, F_SETFL, O_NONBLOCK) == -1)
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			goto fail;
		}

		const int sendbuff = LV2_OSC_STREAM_SNDBUF;
		const int recvbuff = LV2_OSC_STREAM_RCVBUF;
		const int reuseaddr = 1;

		if(setsockopt(stream->sock, SOL_SOCKET,
			SO_SNDBUF, &sendbuff, sizeof(sendbuff)) == -1)
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			goto fail;
		}

		if(setsockopt(stream->sock, SOL_SOCKET,
			SO_RCVBUF, &recvbuff, sizeof(recvbuff)) == -1)
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			goto fail;
		}

		if(setsockopt(stream->sock, SOL_SOCKET,
			SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1)
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			goto fail;
		}

		if(stream->socket_family == AF_INET) // IPv4
		{
			if(stream->server)
			{
				// resolve self address
				struct addrinfo hints;
				memset(&hints, 0x0, sizeof(struct addrinfo));
				hints.ai_family = stream->socket_family;
				hints.ai_socktype = stream->socket_type;
				hints.ai_protocol = stream->protocol;

				struct addrinfo *res;
				if(getaddrinfo(node, service, &hints, &res) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}
				if(res->ai_addrlen != sizeof(stream->peer.in4))
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, EPROTOTYPE);
					goto fail;
				}

				stream->self.len = res->ai_addrlen;
				memcpy(&stream->self.in4, res->ai_addr, res->ai_addrlen);
				stream->self.in4.sin_addr.s_addr = htonl(INADDR_ANY);

				freeaddrinfo(res);

				if(bind(stream->sock, (struct sockaddr *)&stream->self.in4,
					stream->self.len) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}
			}
			else // client
			{
				stream->self.len = sizeof(stream->self.in4);
				stream->self.in4.sin_family = stream->socket_family;
				stream->self.in4.sin_port = htons(0);
				stream->self.in4.sin_addr.s_addr = htonl(INADDR_ANY);

				if(bind(stream->sock, (struct sockaddr *)&stream->self.in4,
					stream->self.len) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}

				// resolve peer address
				struct addrinfo hints;
				memset(&hints, 0x0, sizeof(struct addrinfo));
				hints.ai_family = stream->socket_family;
				hints.ai_socktype = stream->socket_type;
				hints.ai_protocol = stream->protocol;

				struct addrinfo *res;
				if(getaddrinfo(node, service, &hints, &res) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}
				if(res->ai_addrlen != sizeof(stream->peer.in4))
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, EPROTOTYPE);
					goto fail;
				}

				stream->peer.len = res->ai_addrlen;
				memcpy(&stream->peer.in4, res->ai_addr, res->ai_addrlen);

				freeaddrinfo(res);
			}

			if(stream->socket_type == SOCK_DGRAM)
			{
				const int broadcast = 1;

				if(setsockopt(stream->sock, SOL_SOCKET, SO_BROADCAST,
					&broadcast, sizeof(broadcast)) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}

				//FIXME handle multicast
			}
			else if(stream->socket_type == SOCK_STREAM)
			{
				const int flag = 1;

				if(setsockopt(stream->sock, stream->protocol,
					TCP_NODELAY, &flag, sizeof(flag)) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}

				if(setsockopt(stream->sock, SOL_SOCKET,
					SO_KEEPALIVE, &flag, sizeof(flag)) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}

				if(stream->server)
				{
					if(listen(stream->sock, 1) != 0)
					{
						ev = LV2_OSC_STREAM_ERRNO(ev, errno);
						goto fail;
					}
				}
				else // client
				{
					if(connect(stream->sock, (struct sockaddr *)&stream->peer.in4,
						stream->peer.len) == 0)
					{
						stream->connected = true;
					}
				}
			}
			else
			{
				ev = LV2_OSC_STREAM_ERRNO(ev, EPROTOTYPE);
				goto fail;
			}
		}
		else if(stream->socket_family == AF_INET6) // IPv6
		{
			if(stream->server)
			{
				// resolve self address
				struct addrinfo hints;
				memset(&hints, 0x0, sizeof(struct addrinfo));
				hints.ai_family = stream->socket_family;
				hints.ai_socktype = stream->socket_type;
				hints.ai_protocol = stream->protocol;

				struct addrinfo *res;
				if(getaddrinfo(node, service, &hints, &res) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}
				if(res->ai_addrlen != sizeof(stream->peer.in6))
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, EPROTOTYPE);
					goto fail;
				}

				stream->self.len = res->ai_addrlen;
				memcpy(&stream->self.in6, res->ai_addr, res->ai_addrlen);
				stream->self.in6.sin6_addr = in6addr_any;
				if(iface)
				{
					stream->self.in6.sin6_scope_id = if_nametoindex(iface);
				}

				freeaddrinfo(res);

				if(bind(stream->sock, (struct sockaddr *)&stream->self.in6,
					stream->self.len) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}
			}
			else // client
			{
				stream->self.len = sizeof(stream->self.in6);
				stream->self.in6.sin6_family = stream->socket_family;
				stream->self.in6.sin6_port = htons(0);
				stream->self.in6.sin6_addr = in6addr_any;
				if(iface)
				{
					stream->self.in6.sin6_scope_id = if_nametoindex(iface);
				}

				if(bind(stream->sock, (struct sockaddr *)&stream->self.in6,
					stream->self.len) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}

				// resolve peer address
				struct addrinfo hints;
				memset(&hints, 0x0, sizeof(struct addrinfo));
				hints.ai_family = stream->socket_family;
				hints.ai_socktype = stream->socket_type;
				hints.ai_protocol = stream->protocol;

				struct addrinfo *res;
				if(getaddrinfo(node, service, &hints, &res) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}
				if(res->ai_addrlen != sizeof(stream->peer.in6))
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, EPROTOTYPE);
					goto fail;
				}

				stream->peer.len = res->ai_addrlen;
				memcpy(&stream->peer.in6, res->ai_addr, res->ai_addrlen);

				if(iface)
				{
					stream->peer.in6.sin6_scope_id = if_nametoindex(iface);
				}

				freeaddrinfo(res);
			}

			if(stream->socket_type == SOCK_DGRAM)
			{
				// nothing to do
			}
			else if(stream->socket_type == SOCK_STREAM)
			{
				const int flag = 1;

				if(setsockopt(stream->sock, stream->protocol,
					TCP_NODELAY, &flag, sizeof(flag)) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}

				if(setsockopt(stream->sock, SOL_SOCKET,
					SO_KEEPALIVE, &flag, sizeof(flag)) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					goto fail;
				}

				if(stream->server)
				{
					if(listen(stream->sock, 1) != 0)
					{
						ev = LV2_OSC_STREAM_ERRNO(ev, errno);
						goto fail;
					}
				}
				else // client
				{
					if(connect(stream->sock, (struct sockaddr *)&stream->peer.in6,
						stream->peer.len) == 0)
					{
						stream->connected = true;
					}
				}
			}
			else
			{
				ev = LV2_OSC_STREAM_ERRNO(ev, EPROTOTYPE);
				goto fail;
			}
		}
		else
		{
			ev = LV2_OSC_STREAM_ERRNO(ev, EPROTOTYPE);
			goto fail;
		}
	}

	free(dup);

	return ev;

fail:
	if(dup)
	{
		free(dup);
	}

	_close_socket(&stream->sock);

	return ev;
}

static int
lv2_osc_stream_init(LV2_OSC_Stream *stream, const char *url,
	const LV2_OSC_Driver *driv, void *data)
{
	memset(stream, 0x0, sizeof(LV2_OSC_Stream));

	strncpy(stream->url, url, sizeof(stream->url) - 1);
	stream->driv = driv;
	stream->data = data;
	stream->sock = -1;
	stream->fd = -1;

	return _lv2_osc_stream_reinit(stream);
}

#define SLIP_END					0300	// 0xC0, 192, indicates end of packet
#define SLIP_ESC					0333	// 0xDB, 219, indicates byte stuffing
#define SLIP_END_REPLACE	0334	// 0xDC, 220, ESC ESC_END means END data byte
#define SLIP_ESC_REPLACE	0335	// 0xDD, 221, ESC ESC_ESC means ESC data byte

// SLIP encoding
static size_t
lv2_osc_slip_encode_inline(uint8_t *dst, size_t len)
{
	if(len == 0)
		return 0;

	const uint8_t *end = dst + len;

	// estimate new size
	size_t size = 2; // double ended SLIP
	for(const uint8_t *from=dst; from<end; from++, size++)
	{
		if( (*from == SLIP_END) || (*from == SLIP_ESC))
			size ++;
	}

	// fast track if no escaping needed
	if(size == len + 2)
	{
		memmove(dst+1, dst, len);
		dst[0] = SLIP_END;
		dst[size-1] = SLIP_END;

		return size;
	}

	// slow track if some escaping needed
	uint8_t *to = dst + size - 1;
	*to-- = SLIP_END;
	for(const uint8_t *from=end-1; from>=dst; from--)
	{
		if(*from == SLIP_END)
		{
			*to-- = SLIP_END_REPLACE;
			*to-- = SLIP_ESC;
		}
		else if(*from == SLIP_ESC)
		{
			*to-- = SLIP_ESC_REPLACE;
			*to-- = SLIP_ESC;
		}
		else
			*to-- = *from;
	}
	*to-- = SLIP_END;

	return size;
}

// SLIP decoding
static size_t 
lv2_osc_slip_decode_inline(uint8_t *dst, size_t len, size_t *size)
{
	const uint8_t *src = dst;
	const uint8_t *end = dst + len;
	uint8_t *ptr = dst;

	bool whole = false;

	if( (src < end) && (*src == SLIP_END) )
	{
		 whole = true;
		 src++;
	}

	while(src < end)
	{
		if(*src == SLIP_ESC)
		{
			if(src == end-1)
				break;

			src++;
			if(*src == SLIP_END_REPLACE)
				*ptr++ = SLIP_END;
			else if(*src == SLIP_ESC_REPLACE)
				*ptr++ = SLIP_ESC;
			src++;
		}
		else if(*src == SLIP_END)
		{
			src++;

			*size = whole ? ptr - dst : 0;
			return src - dst;
		}
		else
		{
			*ptr++ = *src++;
		}
	}

	*size = 0;
	return 0;
}

static LV2_OSC_Enum
_lv2_osc_stream_run_udp(LV2_OSC_Stream *stream)
{
	LV2_OSC_Enum ev = LV2_OSC_NONE;

	// send everything
	if(stream->peer.len) // has a peer
	{
		const uint8_t *buf;
		size_t tosend;

		while( (buf = stream->driv->read_req(stream->data, &tosend)) )
		{
			const ssize_t sent = sendto(stream->sock, buf, tosend, 0,
				(struct sockaddr *)&stream->peer.in6, stream->peer.len);

			if(sent == -1)
			{
				if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
				{
					// full queue
					break;
				}

				ev = LV2_OSC_STREAM_ERRNO(ev, errno);
				break;
			}
			else if(sent != (ssize_t)tosend)
			{
				ev = LV2_OSC_STREAM_ERRNO(ev, EIO);
				break;
			}

			stream->driv->read_adv(stream->data);
			ev |= LV2_OSC_SEND;
		}
	}

	// recv everything
	{
		uint8_t *buf;
		size_t max_len;

		while( (buf = stream->driv->write_req(stream->data,
			LV2_OSC_STREAM_REQBUF, &max_len)) )
		{
			struct sockaddr_in6 in;
			socklen_t in_len = sizeof(in);

			memset(&in, 0, in_len);
			const ssize_t recvd = recvfrom(stream->sock, buf, max_len, 0,
				(struct sockaddr *)&in, &in_len);

			if(recvd == -1)
			{
				if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
				{
					// empty queue
					break;
				}

				ev = LV2_OSC_STREAM_ERRNO(ev, errno);
				break;
			}
			else if(recvd == 0)
			{
				// peer has shut down
				break;
			}

			stream->peer.len = in_len;
			memcpy(&stream->peer.in6, &in, in_len);

			stream->driv->write_adv(stream->data, recvd);
			ev |= LV2_OSC_RECV;
		}
	}

	return ev;
}

static LV2_OSC_Enum
_lv2_osc_stream_run_tcp(LV2_OSC_Stream *stream)
{
	LV2_OSC_Enum ev = LV2_OSC_NONE;

	// handle connections
	if(!stream->connected) // no peer
	{
		if(stream->server)
		{
			stream->peer.len = sizeof(stream->peer.in6);
			stream->fd = accept(stream->sock, (struct sockaddr *)&stream->peer.in6,
				&stream->peer.len);

			if(stream->fd >= 0)
			{
				const int flag = 1;
				const int sendbuff = LV2_OSC_STREAM_SNDBUF;
				const int recvbuff = LV2_OSC_STREAM_RCVBUF;

				if(fcntl(stream->fd, F_SETFL, O_NONBLOCK) == -1)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
				}

				if(setsockopt(stream->fd, stream->protocol,
					TCP_NODELAY, &flag, sizeof(flag)) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
				}

				if(setsockopt(stream->sock, SOL_SOCKET,
					SO_KEEPALIVE, &flag, sizeof(flag)) != 0)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
				}

				if(setsockopt(stream->fd, SOL_SOCKET,
					SO_SNDBUF, &sendbuff, sizeof(sendbuff)) == -1)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
				}

				if(setsockopt(stream->fd, SOL_SOCKET,
					SO_RCVBUF, &recvbuff, sizeof(recvbuff)) == -1)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
				}

				stream->connected = true; // orderly accept
			}
			else
			{
				//ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			}
		}
		else
		{
			if(stream->sock < 0)
			{
				ev = _lv2_osc_stream_reinit(stream);
			}

			if(connect(stream->sock, (struct sockaddr *)&stream->peer.in6,
				stream->peer.len) == 0)
			{
				stream->connected = true; // orderly (re)connect
			}
			else
			{
				//if(errno == EISCONN)
				//{
				//	_close_socket(&stream->sock);
				//}

				//ev = LV2_OSC_STREAM_ERRNO(ev, errno);
			}
		}
	}

	// send everything
	if(stream->connected)
	{
		int *fd = stream->server
			? &stream->fd
			: &stream->sock;

		if(*fd >= 0)
		{
			const uint8_t *buf;
			size_t tosend;

			while( (buf = stream->driv->read_req(stream->data, &tosend)) )
			{
				if(stream->slip) // SLIP framed
				{
					if(tosend <= sizeof(stream->tx_buf)) // check if there is enough memory
					{
						memcpy(stream->tx_buf, buf, tosend);
						tosend = lv2_osc_slip_encode_inline(stream->tx_buf, tosend);
					}
					else
					{
						tosend = 0;
					}
				}
				else // uint32_t prefix frames
				{
					const size_t nsize = tosend + sizeof(uint32_t);

					if(nsize <= sizeof(stream->tx_buf)) // check if there is enough memory
					{
						const uint32_t prefix = htonl(tosend);

						memcpy(stream->tx_buf, &prefix, sizeof(uint32_t));
						memcpy(stream->tx_buf + sizeof(uint32_t), buf, tosend);
						tosend = nsize;
					}
					else
					{
						tosend = 0;
					}
				}

				const ssize_t sent = tosend
					? send(*fd, stream->tx_buf, tosend, 0)
					: 0;

				if(sent == -1)
				{
					if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
					{
						// empty queue
						break;
					}

					_close_socket(fd);
					stream->connected = false;
					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					break;
				}
				else if(sent != (ssize_t)tosend)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, EIO);
					break;
				}

				stream->driv->read_adv(stream->data);
				ev |= LV2_OSC_SEND;
			}
		}
	}

	// recv everything
	if(stream->connected)
	{
		int *fd = stream->server
			? &stream->fd
			: &stream->sock;

		if(*fd >= 0)
		{
			if(stream->slip) // SLIP framed
			{
				while(true)
				{
					ssize_t recvd = recv(*fd, stream->rx_buf + stream->rx_off,
						sizeof(stream->rx_buf) - stream->rx_off, 0);

					if(recvd == -1)
					{
						if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
						{
							// empty queue
							break;
						}

						_close_socket(fd);
						stream->connected = false;
						ev = LV2_OSC_STREAM_ERRNO(ev, errno);
						break;
					}
					else if(recvd == 0)
					{
						_close_socket(fd);
						stream->connected = false; // orderly shutdown
						break;
					}

					uint8_t *ptr = stream->rx_buf;
					recvd += stream->rx_off;

					while(recvd > 0)
					{
						size_t size;
						size_t parsed = lv2_osc_slip_decode_inline(ptr, recvd, &size);

						if(size) // dispatch
						{
							uint8_t *buf;

							if( (buf = stream->driv->write_req(stream->data, size, NULL)) )
							{
								memcpy(buf, ptr, size);

								stream->driv->write_adv(stream->data, size);
								ev |= LV2_OSC_RECV;
							}
							else
							{
								parsed = 0;
								ev = LV2_OSC_STREAM_ERRNO(ev, ENOMEM);
							}
						}

						if(parsed)
						{
							ptr += parsed;
							recvd -= parsed;
						}
						else
						{
							break;
						}
					}

					if(recvd > 0) // is there remaining chunk for next call?
					{
						memmove(stream->rx_buf, ptr, recvd);
						stream->rx_off = recvd;
					}
					else
					{
						stream->rx_off = 0;
					}

					break;
				}
			}
			else // uint32_t prefix frames
			{
				uint8_t *buf;
				
				while( (buf = stream->driv->write_req(stream->data,
					LV2_OSC_STREAM_REQBUF, NULL)) )
				{
					uint32_t prefix;

					ssize_t recvd = recv(*fd, &prefix, sizeof(uint32_t), 0);
					if(recvd == sizeof(uint32_t))
					{
						prefix = ntohl(prefix); //FIXME check prefix <= max_len
						recvd = recv(*fd, buf, prefix, 0);
					}
					else if(recvd == -1)
					{
						if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
						{
							// empty queue
							break;
						}

						_close_socket(fd);
						stream->connected = false;
						ev = LV2_OSC_STREAM_ERRNO(ev, errno);
						break;
					}
					else if(recvd == 0)
					{
						_close_socket(fd);
						stream->connected = false; // orderly shutdown
						break;
					}

					stream->driv->write_adv(stream->data, recvd);
					ev |= LV2_OSC_RECV;
				}
			}
		}
	}

	if(stream->connected)
	{
		ev |= LV2_OSC_CONN;
	}

	return ev;
}

static LV2_OSC_Enum
_lv2_osc_stream_run_ser(LV2_OSC_Stream *stream)
{
	LV2_OSC_Enum ev = LV2_OSC_NONE;

	// send everything
	{
		const int fd = stream->sock;

		if(fd >= 0)
		{
			const uint8_t *buf;
			size_t tosend;

			while( (buf = stream->driv->read_req(stream->data, &tosend)) )
			{
				if(stream->slip) // SLIP framed
				{
					if(tosend <= sizeof(stream->tx_buf)) // check if there is enough memory
					{
						memcpy(stream->tx_buf, buf, tosend);
						tosend = lv2_osc_slip_encode_inline(stream->tx_buf, tosend);
					}
					else
					{
						tosend = 0;
					}
				}
				else // uint32_t prefix frames
				{
					const size_t nsize = tosend + sizeof(uint32_t);

					if(nsize <= sizeof(stream->tx_buf)) // check if there is enough memory
					{
						const uint32_t prefix = htonl(tosend);

						memcpy(stream->tx_buf, &prefix, sizeof(uint32_t));
						memcpy(stream->tx_buf + sizeof(uint32_t), buf, tosend);
						tosend = nsize;
					}
					else
					{
						tosend = 0;
					}
				}

				const ssize_t sent = tosend
					? write(fd, stream->tx_buf, tosend)
					: 0;

				if(sent == -1)
				{
					if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
					{
						// empty queue
						break;
					}

					ev = LV2_OSC_STREAM_ERRNO(ev, errno);
					break;
				}
				else if(sent != (ssize_t)tosend)
				{
					ev = LV2_OSC_STREAM_ERRNO(ev, EIO);
					break;
				}

				stream->driv->read_adv(stream->data);
				ev |= LV2_OSC_SEND;
			}
		}
	}

	// recv everything
	{
		const int fd = stream->sock;

		if(fd >= 0)
		{
			if(stream->slip) // SLIP framed
			{
				while(true)
				{
					ssize_t recvd = read(fd, stream->rx_buf + stream->rx_off,
						sizeof(stream->rx_buf) - stream->rx_off);

					if(recvd == -1)
					{
						if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
						{
							// empty queue
							break;
						}

						stream->connected = false;
						ev = LV2_OSC_STREAM_ERRNO(ev, errno);
						break;
					}
					else if(recvd == 0)
					{
						// orderly shutdown
						break;
					}

					uint8_t *ptr = stream->rx_buf;
					recvd += stream->rx_off;

					while(recvd > 0)
					{
						size_t size;
						size_t parsed = lv2_osc_slip_decode_inline(ptr, recvd, &size);

						if(size) // dispatch
						{
							uint8_t *buf;

							if( (buf = stream->driv->write_req(stream->data, size, NULL)) )
							{
								memcpy(buf, ptr, size);

								stream->driv->write_adv(stream->data, size);
								ev |= LV2_OSC_RECV;
							}
							else
							{
								parsed = 0;
								ev = LV2_OSC_STREAM_ERRNO(ev, ENOMEM);
							}
						}

						if(parsed)
						{
							ptr += parsed;
							recvd -= parsed;
						}
						else
						{
							break;
						}
					}

					if(recvd > 0) // is there remaining chunk for next call?
					{
						memmove(stream->rx_buf, ptr, recvd);
						stream->rx_off = recvd;
					}
					else
					{
						stream->rx_off = 0;
					}

					break;
				}
			}
			else // uint32_t prefix frames
			{
				uint8_t *buf;
				
				while( (buf = stream->driv->write_req(stream->data,
					LV2_OSC_STREAM_REQBUF, NULL)) )
				{
					uint32_t prefix;

					ssize_t recvd = read(fd, &prefix, sizeof(uint32_t));
					if(recvd == sizeof(uint32_t))
					{
						prefix = ntohl(prefix); //FIXME check prefix <= max_len
						recvd = read(fd, buf, prefix);
					}
					else if(recvd == -1)
					{
						if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
						{
							// empty queue
							break;
						}

						stream->connected = false;
						ev = LV2_OSC_STREAM_ERRNO(ev, errno);
						break;
					}
					else if(recvd == 0)
					{
						// orderly shutdown
						break;
					}

					stream->driv->write_adv(stream->data, recvd);
					ev |= LV2_OSC_RECV;
				}
			}
		}
	}

	if(stream->connected)
	{
		ev |= LV2_OSC_CONN;
	}

	return ev;
}

static LV2_OSC_Enum
lv2_osc_stream_run(LV2_OSC_Stream *stream)
{
	LV2_OSC_Enum ev = LV2_OSC_NONE;

	switch(stream->socket_type)
	{
		case SOCK_DGRAM:
		{
			ev |= _lv2_osc_stream_run_udp(stream);
		} break;
		case SOCK_STREAM:
		{
			ev |= _lv2_osc_stream_run_tcp(stream);
		} break;
		default:
		{
			ev |= _lv2_osc_stream_run_ser(stream);
		} break;
	}

	return ev;
}

static int
lv2_osc_stream_get_file_descriptors(LV2_OSC_Stream *stream, int fds [2])
{
	if(!fds)
	{
		return 1;
	}

	fds[0] = stream->sock;
	fds[1] = stream->fd;
}

static LV2_OSC_Enum
lv2_osc_stream_pollin(LV2_OSC_Stream *stream, int timeout_ms)
{
	int fd [2];

	if(lv2_osc_stream_get_file_descriptors(stream, fd) != 0)
	{
		return LV2_OSC_STREAM_ERRNO(LV2_OSC_NONE, EBADF);
	}

	struct pollfd fds [2] = {
		[0] = {
			.fd = fd[0],
			.events = POLLIN,
			.revents = 0
		},
		[1] = {
			.fd = fd[1],
			.events = POLLIN,
			.revents = 0
		}
	};

	const int res = poll(fds, 2, timeout_ms);
	if(res < 0)
	{
		return LV2_OSC_STREAM_ERRNO(LV2_OSC_NONE, errno);
	}

#if 0
	fprintf(stderr, "++ %i: %i %i %i %i\n", res,
		fds[0].fd, (int)fds[0].revents,
		fds[1].fd, (int)fds[1].revents);
#endif

	return lv2_osc_stream_run(stream);
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LV2_OSC_STREAM_H
