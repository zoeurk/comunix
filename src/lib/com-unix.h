#ifndef COMUNIX_H
#define COMUNIX_H
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <poll.h>

#include <err.h>
#include <errno.h>

#include <unistd.h>
#ifndef STDLIB
	#ifdef __USE_XOPEN_EXTENDED
		#undef __USE_XOPEN_EXTENDED
	#endif
	#ifdef __USE_XOPEN2K8
		#undef __USE_XOPEN2K8
	#endif
#endif
#include <stdlib.h>

#ifndef __WORDSIZE
	#include <limitinfo.h>
#endif

#ifndef STRING
	void *_memset(void *_d, const char c, const unsigned long int size);
	void *_memcpy(void *_dst, const void *_src, const unsigned long int size);
	char *_strcpy(char *str1, const char *str2);
	int _strcmp(const char *str1, const char *str2);
	int _strncmp(const char *str1, const char *str2, unsigned long int size);
	unsigned long int _strlen(const char *str);
	#define MEMSET _memset
	#define MEMCPY _memcpy
	#define STRCPY _strcpy
	#define STRCMP _strcmp
	#define STRNCMP _strncmp
	#define STRLEN _strlen
#else
	#include <string.h>
	#define MEMSET memset
	#define MEMCPY memcpy
	#define STRCPY strcpy
	#define STRNCMP strncmp
	#define STRCMP strcmp
	#define STRLEN strlen
#endif

enum TYPE{
	DGRAM,
	SEQPACKET,
	STREAM
};
enum PROTOCOLS{
	ZERO,
	TCP,
	UDP
};
struct accept{
	char *id;
	int fd;
	int policy;
	struct accept *next;
};
struct connect{
	int inc;
	int backlog;
	size_t ncli;
	size_t curcli;
	size_t climax;
	int **clifd;
};
struct socket_opt{
	int opt;
	struct socket_opt *next;
};
struct sockets{
	void *addr;
	size_t addrlen;
	int protocol;	/*0*/
	int op;
	void *data;
	void (*destroy_data)(void *);
	int fd_sck;
	int fd;
	int domain;	/*AF_UNIX*/
	int type;	/*DGRAM, SEQPACKET, STREAM*/
	struct socket_opt *opt;
	struct connect *c;
	struct accept *a;
	int default_policy;
	int w_flags;
	int r_flags;
	int server;
	void (*mainfn)(void *, struct sockets *);
	void *p;
	struct sockets *next;
};
struct comunix{
	struct sockets *s;
	struct pollfd *pfds;
	struct pollfd *r_pfds;
	int time;
	int output;
	nfds_t nfds;
	ssize_t szr;
	size_t buflen;
	char *buffer;
};
#define COMUNIX(c_ptr) \
	((struct comunix *)c_ptr)

struct perform{
	int (*init)(struct sockets *);
	void (*new_con)(struct comunix *, struct sockets *);
	ssize_t (*r_on_local)(struct comunix *, struct sockets *);
	void (*on_sck_out)(struct comunix *, struct sockets *);
	ssize_t (*on_sck_in)(struct comunix *, struct sockets *);
	void (*w_on_local)(struct comunix *, struct sockets *);
};
struct sockets *find_sck(int fd, struct sockets *s);
int binding(struct sockets *);
int listening(struct sockets *);
int connecting(struct sockets *);
void Server_dgram(void *, struct sockets *);
void Client_dgram(void *, struct sockets *);
void Server_void(void *, struct sockets *);
void Server(void *, struct sockets *);
void Client(void *, struct sockets *);
void destroy_comunix(struct comunix *);
#endif
