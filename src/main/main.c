#include "../lib/com-unix.h"
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
static void connection(struct comunix *c, struct sockets *s);
static void onsend(struct comunix *c, struct sockets *s);
static void writing(struct comunix *c, struct sockets *s);
static ssize_t onrecv(struct comunix *c, struct sockets *s);
/*Servers only*/
static ssize_t dgramEcho(struct comunix *c, struct sockets *s);
static void SendTo(struct comunix *c, struct sockets *s);
static ssize_t RecvFrom(struct comunix *c, struct sockets *s);
static ssize_t reading(struct comunix *c, struct sockets *s);
static int connect_send(struct sockets *s);
static void Client_SendTo(struct comunix *c, struct sockets *s);
/*Servers And Clients Operations*/
static void SimpleOnRecvFrom(struct comunix *c, struct sockets *s);
/*Servers Only Operations*/
static void DispachOnRecvFrom(struct comunix *c, struct sockets *s);
static void EchoOnRecvFrom(struct comunix *c, struct sockets *s);

const char *path = "./unix";
struct comunix *c = NULL;
union proxy_addr{
	struct {
		char buffer[104];
	}v1hdr;
	struct{
		uint8_t sig[12];
		uint8_t ver_cmd;
		uint8_t fam;
		uint16_t len;
		union {
			struct {        /* for TCP/UDP over IPv4, len = 12 */
				uint32_t src_addr;
				uint32_t dst_addr;
				uint16_t src_port;
				uint16_t dst_port;
			}ipv4_addr;
			struct {        /* for TCP/UDP over IPv6, len = 36 */
				uint8_t  src_addr[16];
				uint8_t  dst_addr[16];
				uint16_t src_port;
				uint16_t dst_port;
			}ipv6_addr;
			struct {        /* for AF_UNIX sockets, len = 216 */
				uint8_t src_addr[108];
				uint8_t dst_addr[108];
			}unix_addr;
		}addr;
	}v2hdr;
};
char *const type[] = {
	"DGRAM",
	"SEQPACKET",
	"STREAM",
	NULL
};
char *const proto[] = {
	"0",
	"TCP",
	"UDP",
	NULL
};
char *const servers_opt[] = {
	"default",
	"Echo",
	"Dispacher",
	NULL
};
char *_strchr(char *str, char c){
	for(;*str != c && *str != 0;str++);
	if(*str)
		return str;
	return NULL;
}

int getsubopt(char *subopt, char *const *tokens){
	int i = 0;
	for(;*tokens && STRCMP(subopt, *tokens); tokens++, i++);
	return i;
}

char *str_split(char *str, char *sep, char **r){
	static char *s, *ps, *psep;
	if(str == NULL && *r == NULL)
		return NULL;
	if(str)
		s = ps = *r = str;
	else
		*r = ps = ++s;
	for(;*s != 0;
		ps++, s++){
		if(*s == '\\'){
			switch(*(s+1)){
				case '\\':
					break;
				default:
					for(psep = sep; *psep != 0; psep++){
						if(*(s+1) == *sep){
							*ps = *(s + 1);
							s+=2;
							ps++;
							/**s = *ps = 0;*/
							break;
						}
					}
			}
		}
		for(psep = sep; *psep != 0; psep++){
			if(*s == *sep){
				*s = *ps = 0;
				return *r;
			}
		}
		*ps = *s;
	}
	*ps = *s;
	ps = *r;
	*r = NULL;
	return ps;
}
/*char *strchar(char *str, char c){
	for(;*str != c && *str;str++);
	return str;
}*/
#include <signal.h>
void end_of_socket(int signum){
	const char *sig = NULL;
	switch(signum){
		case SIGINT:
			sig = "SIGINT";
			break;
		case SIGTERM:
			sig = "SIGTERM";
			break;
		case SIGQUIT:
			sig = "SIGQUIT";
			break;
		case SIGPIPE:
			sig = "SIGPIPE";
			break;
	}
	if(sig)
		warnx("Receving signal: %s, closing socket.", sig);
	else
		warnx("End, on unkown signal: %i.", signum);
	destroy_comunix(c);
	exit(0);
}

void signals(int server){
	if(signal (SIGINT, end_of_socket) == SIG_IGN)
		signal (SIGINT, SIG_IGN);
	if(signal (SIGTERM, end_of_socket) == SIG_IGN)
		signal (SIGTERM, SIG_IGN);
	if(signal (SIGQUIT, end_of_socket) == SIG_IGN)
		signal (SIGQUIT, SIG_IGN);
	if(server)
		signal (SIGPIPE, SIG_IGN);
	else
		if(signal (SIGPIPE, end_of_socket) == SIG_IGN)
			signal (SIGPIPE, SIG_IGN);
}

#include <stdio.h>
#define TIME 500
void check_proxy(int fd_out, int fd_sck){
	struct sockaddr_in saddr;
	/*socklen_t saddr_len = sizeof(struct sockaddr_in);*/
	socklen_t saddr_len = sizeof(struct sockaddr_in6);
	struct addrinfo hints, *res, *pres;
	struct pollfd pfd = { 0, POLLIN, 0 };
	union proxy_addr *addr;
	ssize_t szr;
	int errset;
	char buffer[104],buf[256], /**save = NULL,*/
		sigv2[12] = {0xD, 0xA, 0xD, 0xA, 0x0, 0xD, 0xA, 0x51, 0x55, 0x49, 0x54, 0xA },
		*ver_cmd[2] = { "LOCAL", "PROXY" },
		*domain[4] = { "AF_UNSPEC", "AF_INET", "AF_INET6", "AF_UNIX" },
		*transport[3] = {"UNSPEC", "STREAM", "DGRAM" },
		address_src[INET6_ADDRSTRLEN],
		address_dst[INET6_ADDRSTRLEN];
	int fam, ret, len = 16;
	pfd.fd = fd_sck;
	if(getsockname(fd_sck, (struct sockaddr *)&saddr, &saddr_len) < 0)
		err(255, "getsockname()");
	switch(((struct sockaddr_in *)&saddr)->sin_family){
		case AF_INET:
			printf("getsockname() =  %s:%i\n",inet_ntop(((struct sockaddr_in *)&saddr)->sin_family, &((struct sockaddr_in *)&saddr)->sin_addr, address_src, saddr_len),ntohs(((struct sockaddr_in *)&saddr)->sin_port));
			break;
		case AF_INET6:
			printf("getsockname() =  %s:%i\n",inet_ntop(((struct sockaddr_in6 *)&saddr)->sin6_family, &((struct sockaddr_in6 *)&saddr)->sin6_addr, address_src, saddr_len),ntohs(((struct sockaddr_in *)&saddr)->sin_port));
			break;
	}
	if(getpeername(fd_sck, (struct sockaddr *)&saddr, &saddr_len) < 0)
		err(255, "getpeername()");
	switch(((struct sockaddr_in *)&saddr)->sin_family){
		case AF_INET:
			printf("getpeername() =  %s:%i\n",inet_ntop(((struct sockaddr_in *)&saddr)->sin_family, &((struct sockaddr_in *)&saddr)->sin_addr, address_src, saddr_len),ntohs(((struct sockaddr_in *)&saddr)->sin_port));
			break;
		case AF_INET6:
			printf("getpeername() =  %s:%i\n",inet_ntop(((struct sockaddr_in6 *)&saddr)->sin6_family, &((struct sockaddr_in6 *)&saddr)->sin6_addr, address_src, saddr_len),ntohs(((struct sockaddr_in6 *)&saddr)->sin6_port));
			break;
	}
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	if((errset = getaddrinfo(NULL, "1234", &hints, &res)) != 0){
	 			err(255, "getaddrinfo():%s", gai_strerror(errset));
	}
	for(pres = res;pres;pres = pres->ai_next){
		printf("get_addrinfo: %s:%i\n", 
			inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, address_src, INET_ADDRSTRLEN), ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port));
	}
	freeaddrinfo(res);
	if((ret = poll(&pfd, 1, -1)) < 0)
		err(255, "poll()");
	else
		if(pfd.revents > 0){
			MEMSET(buffer, 0, 104);
			addr = (union proxy_addr *)buffer;
			if((szr = recv(fd_sck, buffer, sizeof(union proxy_addr), 0)) < 0)
				err(255, "read()");
			if(STRNCMP(buffer,sigv2, 12) == 0){
				sprintf(buf, "version = %i (sig good)\n", addr->v2hdr.ver_cmd >> 4);
				write(fd_out, buf, STRLEN(buf));
				sprintf(buf, "CMD = %s, domain = %s, transport = %s, LEN = %i\n",
						ver_cmd[(addr->v2hdr.ver_cmd&15)], domain[fam = (addr->v2hdr.fam >> 4)],
						transport[addr->v2hdr.fam&15], len += ntohs(addr->v2hdr.len)
				);
				write(fd_out, buf, STRLEN(buf));
				switch(fam){
					case 0:/*AF_UNSPEC*/
						break;
					case 1:/*AF_INET*/
						sprintf(buf, "src = %s:%i dst = %s:%i\n",
							inet_ntop(	AF_INET,
									&addr->v2hdr.addr.ipv4_addr.src_addr,
									address_src,
									INET_ADDRSTRLEN
							),
							ntohs(addr->v2hdr.addr.ipv4_addr.src_port),
							inet_ntop(	AF_INET,
									&addr->v2hdr.addr.ipv4_addr.dst_addr,
									address_dst,
									INET_ADDRSTRLEN
							),
							ntohs(addr->v2hdr.addr.ipv4_addr.dst_port)
						);
						break;
					case 2:/*AF_INET6*/
						sprintf(buf, "src = %s:%i dst = %s:%i\n",
							inet_ntop(	AF_INET6,
									&addr->v2hdr.addr.ipv6_addr.src_addr,
									address_src,
									INET6_ADDRSTRLEN
							),
							ntohs(addr->v2hdr.addr.ipv6_addr.src_port),
							inet_ntop(	AF_INET6,
									&addr->v2hdr.addr.ipv6_addr.dst_addr,
									address_dst,
									INET6_ADDRSTRLEN
							),
							ntohs(addr->v2hdr.addr.ipv6_addr.dst_port)
						);
						break;
					case 3:/*AF_UNIX*/
						break;
				}
				write(fd_out, buf, STRLEN(buf));
				write(c->output, &buffer[len], szr-len);
			}else{
				if(STRNCMP("PROXY", buffer, 5) == 0){
					write(c->output, "PROXY v1\n", 9);
					write(c->output, addr->v1hdr.buffer, szr);
				}else
					write(c->output, buffer, szr);
			}
	}
}

#include <sys/inotify.h>
/*struct files{
	int main_fd;
	int fd;
	int file_fd;
	char *filename;
};*/
struct files{
	int main_fd;
	int fd;
	int file_fd;
	char *filename;
	ssize_t *szr;
	size_t *buflen;
	char **buffer;
};
void close_file(void *data){
	/*if(inotify_rm_watch(((struct files *)data)->main_fd, ((struct files *)data)->fd) < 0){
		warn("inotify_rm_watch()");
	}*/
	if(close(((struct files *)data)->fd) < 0)
		warn("close()");
	if(close(((struct files *)data)->file_fd) < 0)
		warn("close()");
	free(data);
}
#include <argp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
const char *argp_program_version = "comunix 0.1~1";
const char *argp_program_bug_address = "<zoeurk@gmail.com>";
static char doc[] = "Comunic through unix sockets";
static char args_doc[] = "";
static struct argp_option options[] = {
	{ NULL, 0, NULL, 0, "Global:", 0},
	{ "buffer", 'b', "size", 0, "Set buffer size (default: 512)", 1 },
	{ "time", 't', "milliseconds", 0, "Set buffer size (default: 0)", 1 },
	{ NULL, 0, NULL, 0, "Unix socket:", 2},
	{ "unix", 'u', ":OPTIONS:path/unix/socket", 0, "Create unix socket (see man 1 comunix)", 3},
	{ "ipv4", 'i', ":OPTIONS:address/port", 0, "Create ipv4 socket (see man 1 comunix)", 3},
	{ "ipv6", 'I', ":OPTIONS:address/port", 0, "Create ipv6 socket (see man 1 comunix)", 3},
	{ NULL, 0, NULL, 0, "Help", 4},
	{ NULL, '?', NULL, 0, "Alias for --usage", 5},
	{ NULL, 'h', NULL, 0, "Alias for --help", 5},
	{ 0 }
};
int toint(char *s){
	int ret, i, neg;
	char *s_ = s;
	if(*s == '-'){
		s_++;
		neg = -1;
	}else
		neg = 1;
	for(ret = 0; *s_ != 0; s_++){
		i = *s_ - (3 << 4);
		if(i < 0 || i > 9){
			errx(255, "value invalid: '%s'", s);
		}
		ret *= 10;
		ret += i;
	}
	ret *= neg;
	return ret;
}
int stdin_set = -1;
#define CREATE_SOCKET \
 if(c->s == NULL){ \
  if((ps = c->s = malloc(sizeof(struct sockets))) == NULL){ \
   err(255, "malloc()"); \
  } \
 }else{ \
  for(ps = c->s; ps->next != NULL; ps = ps->next); \
  if((ps->next = malloc(sizeof(struct sockets))) == NULL){ \
   err(255, "malloc()"); \
  } \
  ps = ps->next; \
 } \
 MEMCPY(ps, &s_init, sizeof(struct sockets));
#define FILES \
 if(stdin_set > 0)\
 	errx(255, "Multiple 'file or 'send'"); \
 if(*argument == '-' && *(argument+1) == 0){\
  ps->data = NULL;\
  ps->fd = STDIN_FILENO;\
  stdin_set = 1;\
 }else{\
  stdin_set = (stdin_set == 1) ? stdin_set : 0;\
  if(*argument == '\\'){\
   file = argument +1;\
  }else{\
   file = argument;\
  }\
  if(stat(file, &st) < 0)\
   err(255, "stat()");\
  if(S_ISFIFO(st.st_mode)){\
   if((ps->fd = open((char *)file,\
    O_RDWR | O_NONBLOCK\
   )) < 0\
   ){\
    err(255, "open()");\
   }\
   ps->data = NULL;\
  }else{\
   if(S_ISREG(st.st_mode)){\
    if((ps->data = malloc(sizeof(struct files))) == NULL){\
     err(255, "malloc()");\
    }\
    ((struct files *)ps->data)->filename = file;\
    if((((struct files *)ps->data)->main_fd = ps->fd = inotify_init1(IN_NONBLOCK)) < 0){\
     err(255, "inotify_init1()");\
    }\
    ((struct files *)ps->data)->filename = file;\
    if((((struct files *)ps->data)->fd = inotify_add_watch(ps->fd, ((struct files *)ps->data)->filename, IN_MODIFY)) < 0){\
     err(255, "inotify_add_watch()");\
    }\
    if((((struct files *)ps->data)->file_fd = open(((struct files *)ps->data)->filename, O_RDWR | O_NONBLOCK)) < 0){\
     err(255, "open()");\
    } \
    DO_FILE \
    ps->destroy_data = &close_file;\
   }else{\
    errx(255, "%s: Invalid file", file);\
   }\
  }\
 }
void files_seek(struct sockets *ps, char *argument){
	struct stat st;
	char *file;
#define DO_FILE \
    if(ps->data){\
     lseek(((struct files *)ps->data)->file_fd, 0, SEEK_END);\
    }
    FILES
#undef DO_FILE
}
void file_send(struct sockets *ps, char *argument){
	struct stat st;
	char *file;
#define DO_FILE \
    ((struct perform *)ps->p)->init = &connect_send; \
    ((struct files *)ps->data)->buffer = &c->buffer; \
    ((struct files *)ps->data)->buflen = &c->buflen; \
    ((struct files *)ps->data)->szr = &c->szr;
    FILES
#undef DO_FILE
}
static error_t parse_opt(int key, char *arg, struct argp_state *state){
 static const struct comunix com_init = { NULL, NULL, NULL, 0, 0, 0, STDOUT_FILENO, 512, NULL };
 static const struct sockets s_init
    = { NULL, sizeof(struct sockaddr_un), 0, 0, NULL, NULL, 0, 0, AF_UNIX, 0 , NULL, NULL, NULL, 0, 0, 0, 0, NULL, NULL, NULL };
 static const struct connect con_init = { 5, 10, 0, 0, 10, NULL};
 struct sockets *ps = NULL;
 /*struct ifaddrs *addr, *paddr;*/
 char *ret, *save, *argument, *c_addr, *c_port;
 char *const opt[] = { "clients", "backlog", "type", "send", "file", "proto", "socket" };
 int i, j, port, ret_ntop;
 state->name = state->argv[0];
 if(c == NULL){
  if((c = malloc(sizeof(struct comunix))) == NULL){
   errx(255, "malloc()");
  }
  MEMCPY(c, &com_init, sizeof(struct comunix));
 }
 switch(key){
  case '?':
   argp_state_help(state, stdout, ARGP_HELP_USAGE);
   _exit(EXIT_SUCCESS);
  case 'h':
   argp_state_help(state, stdout, ARGP_HELP_SHORT_USAGE | ARGP_HELP_DOC | ARGP_HELP_LONG | ARGP_HELP_BUG_ADDR);
   _exit(EXIT_SUCCESS);
  case 'b':
   save = NULL;
   c->buflen = strtoul(arg, &save, 0);
   if(*save != 0){
    errx(255, "value invalid: '%s'", arg);
   }
   break;
  case 't':
   c->time = toint(arg);
   break;
  case 'u':
   for(i = 0; (ret = str_split(arg, "%", &save)); arg = NULL, i++){
    switch(i){
     case 0:
      if(ret[1] == 0)
       switch(*ret){
        case 'c':
         CREATE_SOCKET;
         ps->server = 0;
         break;
        case 's':
         CREATE_SOCKET;
         ps->server = 1;
         break;
        default:
         errx(255, "invalid argument: '%s'", ret);
       }
      else
       errx(255, "invalid argument: '%s'", ret);
      break;
     case 1:
      switch(getsubopt(ret, type)){
       case DGRAM:
        ps->type = SOCK_DGRAM;
        if((ps->p = malloc(sizeof(struct perform))) == NULL){
         err(255, "malloc()");
        }
        switch(ps->server){
         case 0:
          ((struct perform *)ps->p)->init = &connecting;
          ((struct perform *)ps->p)->on_sck_out = &onsend;
          ((struct perform *)ps->p)->r_on_local = &reading;
          ps->mainfn = &Client_dgram;
          break;
         case 1:
          ((struct perform *)ps->p)->init = &binding;
          ((struct perform *)ps->p)->on_sck_in = &onrecv;
          ((struct perform *)ps->p)->w_on_local = &writing;
          ps->mainfn = &Server_dgram;
          break;
        }
        break;
       case SEQPACKET:
        ps->type = SOCK_SEQPACKET;
        if(ps->c == NULL){
         if((ps->c = malloc(sizeof(struct connect))) == NULL){
          err(255, "malloc()");
         }
         MEMCPY(ps->c, &con_init, sizeof(struct connect));
        }
        if((ps->p = malloc(sizeof(struct perform))) == NULL){
         err(255, "malloc()");
        }
        switch(ps->server){
         case 0:
          ((struct perform *)ps->p)->init = &connecting;
          ((struct perform *)ps->p)->new_con = NULL;
          ((struct perform *)ps->p)->r_on_local = &reading;
          ((struct perform *)ps->p)->w_on_local = &SimpleOnRecvFrom;
          ((struct perform *)ps->p)->on_sck_out = &Client_SendTo;
          ((struct perform *)ps->p)->on_sck_in = &RecvFrom;
          ps->mainfn = &Client;
          break;
         case 1:
          ((struct perform *)ps->p)->init = &listening;
          ((struct perform *)ps->p)->new_con = &connection;
          ((struct perform *)ps->p)->r_on_local = &reading;
          ((struct perform *)ps->p)->w_on_local = &SimpleOnRecvFrom;
          ((struct perform *)ps->p)->on_sck_out = &SendTo;
          ((struct perform *)ps->p)->on_sck_in = &RecvFrom;
          ps->mainfn = &Server;
          break;
        }
        break;
       case STREAM:
        ps->type = SOCK_STREAM;
        if(ps->c == NULL){
         if((ps->c = malloc(sizeof(struct connect))) == NULL){
          err(255, "malloc()");
         }
         MEMCPY(ps->c, &con_init, sizeof(struct connect));
        }
        if((ps->p = malloc(sizeof(struct perform))) == NULL){
         err(255, "malloc()");
        }
        switch(ps->server){
         case 0:
          ((struct perform *)ps->p)->init = &connecting;
          ((struct perform *)ps->p)->new_con = NULL;
          ((struct perform *)ps->p)->r_on_local = &reading;
          ((struct perform *)ps->p)->w_on_local = &SimpleOnRecvFrom;
          ((struct perform *)ps->p)->on_sck_out = &Client_SendTo;
          ((struct perform *)ps->p)->on_sck_in = &RecvFrom;
          ps->mainfn = &Client;
          break;
         case 1:
          ((struct perform *)ps->p)->init = &listening;
          ((struct perform *)ps->p)->new_con = &connection;
          ((struct perform *)ps->p)->r_on_local = &reading;
          ((struct perform *)ps->p)->w_on_local = &SimpleOnRecvFrom;
          ((struct perform *)ps->p)->on_sck_out = &SendTo;
          ((struct perform *)ps->p)->on_sck_in = &RecvFrom;
          ps->mainfn = &Server;
          break;
        }
        stdin_set = 0;
        break;
       default:
        errx(255, "invalid argument: '%s'", ret);
      }
      break;
     default:
      if((argument = _strchr(ret, '='))){
       *argument = 0;
       argument++;
       for(j = 0; j < 5; j++){
        if(STRCMP(ret, opt[j]) == 0){
         if(ps->type == SOCK_DGRAM){
          if((j != 3 || j != 4) && ps->server != 0)
           warnx("Argument not valid for type 'DGRAM' (%s)", opt[j]);
          else{
           switch(j){
            case 3:
             /*FILES;*/
	     file_send(ps, argument);
             break;
            case 4:
             files_seek(ps, argument);
             break;
            default:
             warnx("Argument not valid for type 'DGRAM' (%s)", opt[j]);
           }
          }
         }else{
	  if(ps->server == 0 && j < 5){
           warnx("Argument not valid for client (%s)", opt[j]);
	  }
          switch(j){
           case 0:
            save = NULL;
            ps->c->climax = strtoul(argument, &save, 0);
            if(*save != 0){
             errx(255, "value invalid: '%s'",
                argument);
            }
            break;
           case 1:
            ps->c->backlog = toint(argument);
            break;
           case 2: /*type*/
	    switch(getsubopt(argument, servers_opt)){
	     case 0:
               ((struct perform *)ps->p)->w_on_local = &SimpleOnRecvFrom;
	      break;
	     case 1:
               ((struct perform *)ps->p)->w_on_local = &EchoOnRecvFrom;
	      break;
	     case 2:
               ((struct perform *)ps->p)->w_on_local = &DispachOnRecvFrom;
	      break;
	     default:
	      errx(255, "Unknow argument for '%s' for type", argument);
	    }
            break;
           case 3: /*FILE*/
            /*FILES;*/
	    file_send(ps, argument);
            break;
           case 4: /*FILE*/
            files_seek(ps, argument);
            /*FILES;*/
            break;
          }
         }
         break;
        }
       }
       if(j == 5){
        errx(255, "invalid argument '%s'", ret);
       }
      }else{
       if(save == NULL){
        switch(ps->type){
         case SOCK_DGRAM:
          c->nfds++;
          break;
         default:
          if((ps->c->clifd =
           calloc(ps->c->climax+2, sizeof(int *))) == NULL
          ){
           err(255, "calloc()");
          }
          c->nfds += (ps->c->climax) +2;
          break;
        }
        ps->addrlen = sizeof(struct sockaddr_un);
        if((ps->addr = malloc(ps->addrlen)) == NULL){
         err(255, "malloc()");
        }
        ((struct sockaddr_un *)ps->addr)->sun_family = AF_UNIX;
        STRCPY(((struct sockaddr_un *)ps->addr)->sun_path, ret);
       }else
        errx(255, "invalid arguments, try %s --help", state->name);
      }
      break;
    }
    if(save == NULL)
     break;
   }
   break;
   #define INET_PTON(ip) \
        ps->addrlen = sizeof(struct sockaddr_in); \
        if((ps->addr = malloc(ps->addrlen)) == NULL){ \
         err(255, "malloc()"); \
        } \
        ((struct sockaddr_in *)ps->addr)->sin_family = ip; \
        ((struct sockaddr_in *)ps->addr)->sin_port = htons(port); \
	ret_ntop = inet_pton(ip, c_addr, &((struct sockaddr_in *)ps->addr)->sin_addr);
  #define INTERNET(ip) \
   for(i = 0; (ret = str_split(arg, "%", &save)); arg = NULL, i++){ \
    switch(i){ \
     case 0: \
      if(ret[1] == 0){ \
       switch(*ret){ \
        case 'c': \
         CREATE_SOCKET; \
         ps->server = 0; \
         break; \
        case 's': \
         CREATE_SOCKET; \
         ps->server = 1; \
         break; \
        default: \
         errx(255, "invalid argument: '%s'", ret); \
       } \
       ps->domain = ip; \
      }else \
       errx(255, "invalid argument: '%s'", ret); \
      break; \
     case 1: \
      switch(getsubopt(ret, type)){ \
       case DGRAM: \
        ps->type = SOCK_DGRAM; \
        if((ps->p = malloc(sizeof(struct perform))) == NULL){ \
         err(255, "malloc()"); \
        } \
        switch(ps->server){ \
         case 0: \
          ((struct perform *)ps->p)->init = &connecting; \
          ((struct perform *)ps->p)->on_sck_in = &onrecv; \
          ((struct perform *)ps->p)->on_sck_out = &onsend; \
          ((struct perform *)ps->p)->r_on_local = &reading; \
          ((struct perform *)ps->p)->w_on_local = &writing; \
          ps->mainfn = &Client_dgram; \
          break; \
         case 1: \
          ((struct perform *)ps->p)->init = &binding; \
          ((struct perform *)ps->p)->on_sck_out = &onsend; \
          ((struct perform *)ps->p)->on_sck_in = &onrecv;\
          ((struct perform *)ps->p)->w_on_local = &writing; \
          ((struct perform *)ps->p)->r_on_local = &reading; \
          ps->mainfn = &Server_dgram; \
          break; \
        } \
        stdin_set = 0; \
        break; \
       case STREAM: \
        ps->type = SOCK_STREAM; \
        if(ps->c == NULL){ \
         if((ps->c = malloc(sizeof(struct connect))) == NULL){ \
          err(255, "malloc()"); \
         } \
         MEMCPY(ps->c, &con_init, sizeof(struct connect)); \
        } \
        if((ps->p = malloc(sizeof(struct perform))) == NULL){ \
         err(255, "malloc()"); \
        } \
        switch(ps->server){ \
         case 0: \
          ((struct perform *)ps->p)->init = &connecting; \
          ((struct perform *)ps->p)->new_con = NULL; \
          ((struct perform *)ps->p)->r_on_local = &reading; \
          ((struct perform *)ps->p)->w_on_local = &SimpleOnRecvFrom; \
          ((struct perform *)ps->p)->on_sck_out = &Client_SendTo; \
          ((struct perform *)ps->p)->on_sck_in = &RecvFrom; \
          ps->mainfn = &Client; \
          break; \
         case 1: \
          ((struct perform *)ps->p)->init = &listening; \
          ((struct perform *)ps->p)->new_con = &connection; \
          ((struct perform *)ps->p)->r_on_local = &reading; \
          ((struct perform *)ps->p)->w_on_local = &SimpleOnRecvFrom; \
          ((struct perform *)ps->p)->on_sck_out = &SendTo; \
          ((struct perform *)ps->p)->on_sck_in = &RecvFrom; \
          ps->mainfn = &Server; \
          break; \
        } \
        stdin_set = 0; \
        break; \
       default: \
        errx(255, "invalid argument: '%s'", ret); \
      } \
      break; \
     default: \
      if((argument = _strchr(ret, '='))){ \
       *argument = 0; \
       argument++; \
       for(j = 0; j < 5; j++){ \
        if(STRCMP(ret, opt[j]) == 0){ \
         if(ps->server == 0 && j < 3){ \
          warnx("Argument not valid for client (%s)", opt[j]); \
	  break; \
	 } \
         if(ps->type == SOCK_DGRAM){ \
           switch(j){ \
	    case 2: \
	     switch(getsubopt(argument, servers_opt)){ \
	      case 0: /*default*/\
	       break; \
	      case 1:\
               ((struct perform *)ps->p)->on_sck_in = &dgramEcho; \
	       break; \
	      case 2:\
               warnx("Argument not valid for type 'DGRAM' (%s)", opt[j]); \
	       break; \
	      default: \
	       errx(255, "Unknow argument for '%s' for type", argument); \
	      break; \
	     } \
	     break; \
            case 3: \
             /*FILES;*/ \
	     file_send(ps, argument); \
             break; \
            case 4: \
             files_seek(ps, argument); \
             /*FILES;*/ \
             break; \
            case 5: \
             switch(getsubopt(argument, proto)){ \
              case ZERO: \
               ps->protocol = 0; \
               break; \
              /*case TCP: \
               ps->protocol = IPPROTO_TCP; \
               break; */ \
              case UDP: \
               ps->protocol = IPPROTO_UDP; \
               break; \
              default: \
               errx(255, "Value invalid: '%s'", argument); \
             } \
             break; \
            default: \
             warnx("Argument not valid for type 'DGRAM' (%s)", opt[j]); \
          } \
         }else{ \
          switch(j){ \
           case 0: \
            save = NULL; \
            ps->c->climax = strtoul(argument, &save, 0); \
            if(*save != 0){ \
             errx(255, "value invalid: '%s'", \
                argument); \
            } \
            break; \
           case 1: \
            ps->c->backlog = toint(argument); \
            break; \
           case 2: /*type*/ \
	    switch(getsubopt(argument, servers_opt)){ \
	     case 0: \
               ((struct perform *)ps->p)->w_on_local = &SimpleOnRecvFrom; \
	      break; \
	     case 1: \
               ((struct perform *)ps->p)->w_on_local = &EchoOnRecvFrom; \
	      break; \
	     case 2: \
               ((struct perform *)ps->p)->w_on_local = &DispachOnRecvFrom; \
	      break; \
	     default: \
	      errx(255, "Unknow argument for '%s' for type", argument); \
	    } \
            break; \
           case 3: /*FILE*/ \
            /*FILES;*/ \
	    file_send(ps, argument); \
            break; \
           case 4: /*FILE*/ \
            files_seek(ps, argument); \
            break; \
           case 5: \
            switch(getsubopt(argument, proto)){ \
             case ZERO: \
              ps->protocol = 0; \
              break; \
             case TCP: \
              ps->protocol = IPPROTO_TCP; \
              break; \
             /*case UDP: \
              ps->protocol = IPPROTO_UDP; \
              break;*/ \
             default: \
              errx(255, "Value invalid: '%s'", argument); \
            } \
            break; \
          } \
          break; \
         } \
        } \
        if(j > 5){ \
         errx(255, "invalid argument '%s'", ret); \
        } \
       }\
      }else{ \
       if(save == NULL){ \
	c_addr = ret; \
	c_port = _strchr(c_addr, '/'); \
	if(!c_port){ \
			errx(255, "No port given."); \
	}else{ \
		*c_port = 0; \
		c_port++; \
		if(!*c_port){ \
			errx(255, "No port given."); \
		} \
	} \
	port = toint(c_port); \
	INET_PTON(ip); \
	switch(ret_ntop){ \
	 case -1: \
	  err(255, "inet_pton()"); \
	 case 0: \
	  errx(255,"inet_pton() Not in presentation format"); \
	} \
	switch(ps->type){ \
         case SOCK_DGRAM: \
          c->nfds++; \
          break; \
         default: \
          if((ps->c->clifd = \
           calloc(ps->c->climax+2, sizeof(int *))) == NULL \
          ){ \
           err(255, "calloc()"); \
          } \
          c->nfds += (ps->c->climax) +2; \
        } \
       }else \
        errx(255, "invalid arguments, try %s --help", state->name); \
      } \
      break; \
    } \
    if(save == NULL) \
     break; \
   } \
   break;
  case 'i':
   INTERNET(AF_INET);
  case 'I':
   #undef INET_PTON
   #define INET_PTON(ip) \
        ps->addrlen = sizeof(struct sockaddr_in6); \
        if((ps->addr = calloc(1, ps->addrlen)) == NULL){ \
         err(255, "malloc()"); \
        } \
        ((struct sockaddr_in6 *)ps->addr)->sin6_family = ip; \
        ((struct sockaddr_in6 *)ps->addr)->sin6_port = htons(port); \
	ret_ntop = inet_pton(ip, c_addr, &((struct sockaddr_in6 *)ps->addr)->sin6_addr);
   INTERNET(AF_INET6);
  case ARGP_KEY_END:
   break;
  default:
   if(arg != NULL)
    errx(255, "invalid command line.");
   /*stdin_set = (stdin_set == -1) ? 1 : stdin_set;
   printf("==========\n");*/
   break;
 }
 return 0;
}
#undef CREATE_SOCKET
#undef INET_PTON
#undef INTERNET
#undef FILES
#undef SEND_CLI
#undef SEND_SRV
static struct argp argp = { options, parse_opt, args_doc, doc, NULL, NULL, NULL };
#ifdef DEBUG
void test(struct comunix *c){
	nfds_t i, k, fds;
	struct sockets *s;
	warnx("==>%lu", c->nfds);
	for(fds = 0, i = 0, k = 2, s = c->s; s && fds < c->nfds; i++, fds++){
		switch(s->type){
			case SOCK_DGRAM:
				if(s->mainfn == &Client_dgram || s->mainfn == &Server_dgram)
					warnx("DGRAM");
				if(s->server == 1){
					warnx("FD: %lu (%lu) => %i", i, fds, c->pfds[fds].fd);
					s = s->next;
					i = 0;
				}else{
					warnx("FD: %lu (%lu) => %i", i, fds, c->pfds[fds].fd);
					warnx("FD: %lu (%lu) => %i", i+1, fds+1, c->pfds[fds+1].fd);
					fds++;
					i = -1;
					k = 1;
					s = s->next;
				}
				break;
			default:
				if(s->mainfn == &Client || s->mainfn == &Server)
					warnx("STREAM");
				warnx("FD: %lu (%lu) => %i", i, fds, c->pfds[fds].fd);
				if(i == s->c->climax + k){
					s = s->next;
					k = 1;
					i = 0;
				}
		}
	}
	destroy_comunix(c);
	exit(0);
}
#endif
int main(int argc, char **argv){
	int server = 0;
	nfds_t i, fds;
	struct sockets *s;
	int inc, opt = 1, achar;
	char end[512];
	argp_parse(&argp, argc, argv, 0, 0, &c);
	if((c->buffer = calloc(c->buflen, sizeof(char))) == NULL){
		err(255, "calloc()");
	}
	if((c->pfds = calloc(c->nfds + 1, sizeof(struct pollfd))) == NULL){
		err(255, "calloc()");
	}
	if(stdin_set){
		c->nfds++;
		c->pfds->fd = STDIN_FILENO;
		c->pfds->events = POLLIN;
	}
	for(s = c->s, c->r_pfds = c->pfds + stdin_set, inc = 1; s; s = s->next, c->r_pfds+=inc, inc = 1){
		s->fd_sck = socket(s->domain, s->type, s->protocol);
		if(s->server == 1){
			server = 1;
			if(setsockopt(s->fd_sck, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0)
				err(255, "setsockopt()");
		}
		if(s->type == SOCK_DGRAM){
			switch(s->domain){
				case AF_UNIX:
					if(s->server == 1){
						s->fd = STDOUT_FILENO;
						c->r_pfds->fd = s->fd_sck;
						c->r_pfds->events = POLLIN;
					}else{
						if(s->fd == STDIN_FILENO){
							s->fd = STDIN_FILENO;
							c->nfds--;
							inc = 1;
						}else{
							c->r_pfds->fd = s->fd;
							c->r_pfds->events = POLLIN;
						}
					}
					break;
				default:
					c->r_pfds->fd = s->fd_sck;
					c->r_pfds->events = POLLIN;
					if(s->fd != STDIN_FILENO){
						c->r_pfds++;
						c->r_pfds->fd = s->fd;
						c->r_pfds->events = POLLIN;
					}
			}
		}else{
			c->r_pfds->fd = s->fd_sck;
			c->r_pfds->events = POLLIN;
			if(s->fd != STDIN_FILENO){
				c->r_pfds++;
				c->r_pfds->fd = s->fd;
				c->r_pfds->events = POLLIN;
			}
		}
	}
	for(s = c->s; s; s = s->next){
		if(s->type == SOCK_DGRAM || (s->type != SOCK_DGRAM && s->server == 0)){
			if(s->type != SOCK_DGRAM && s->server == 0){
				c->nfds-=s->c->climax;
			}
			continue;
		}else{
			if(s->fd == STDIN_FILENO)
				c->nfds--;
			for(i = 0; i < s->c->climax; i++, c->r_pfds++){
				c->r_pfds->fd = -1;
				c->r_pfds->events = POLLIN;
				s->c->clifd[i] = &c->r_pfds->fd;
			}
		}
	}
	signals(server);
	do{
		for(s = c->s; s; s = s->next){
			if(s->op == 0)
				if(((struct perform *)s->p)->init(s)){
					switch(errno){
						case ECONNREFUSED:case ENOENT:
							break;
						default:
							err(errno, "initialisation failed");
							break;
					}
				}
		}
		if(poll(c->pfds, c->nfds, c->time) < 0){
			err(255, "poll()");
		}
		for(c->r_pfds = c->pfds, fds = 0; fds < c->nfds; c->r_pfds++, fds++){
			if(c->r_pfds->revents){
				if(c->r_pfds->fd == STDIN_FILENO){
					for(s = c->s;s;s = s->next){
						if((s = find_sck(c->r_pfds->fd, s)) == NULL)
							break;
						if(s->op > 0){
							s->mainfn(c, s);
						}
					}
				}else{	if((s = find_sck(c->r_pfds->fd, c->s)) != NULL)
						if(s->op > 0){
							s->mainfn(c, s);
						}
				}
				if(c->szr > 0)
					MEMSET(c->buffer, 0, c->szr);
				break;
			}
		}
		for(s = c->s; s; s = s->next){
			if(s->fd_sck > 0){
				break;
			}
		}
	}while(s);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	while(read(STDIN_FILENO,end, 512)  > 0);
	end_of_socket(0);
	return 0;
}
void onsend(struct comunix *c, struct sockets *s){
	if(sendto(s->fd_sck, c->buffer, c->buflen, s->r_flags, (struct sockaddr *)s->addr, (socklen_t)s->addrlen) < 0){
		if(errno != ENOENT)
			err(255, "sendto()");
		else{
			warnx("Server down");
			close(s->fd_sck);
			s->fd_sck = -1;
			s->op = 0;
			if((s->fd_sck = socket(s->domain, s->type, s->protocol)) < 0){
				err(255, "socket()");
			}
		}	
	}
}
void writing(struct comunix *c, struct sockets *s){
	if(write(c->output,COMUNIX(c)->buffer, c->szr) < 0){
		err(255, "write()");
	}
}
ssize_t onrecv(struct comunix *c, struct sockets *s){
	c->szr = recvfrom(s->fd_sck, c->buffer, c->buflen, s->r_flags, (struct sockaddr *)s->addr, (socklen_t *)&s->addrlen);
	if(c->szr < 0){
		err(255, "recvfrom()");
	}
	return c->szr;
}
ssize_t dgramEcho(struct comunix *c, struct sockets *s){
	c->szr = recvfrom(s->fd_sck, c->buffer, c->buflen, s->r_flags, (struct sockaddr *)s->addr, (socklen_t *)&s->addrlen);
	if(c->szr < 0){
		err(255, "recvfrom()");
	}
	onsend(c, s);
	return c->szr;
}
ssize_t reading(struct comunix *c, struct sockets *s){
	if((c->szr = read(s->fd, c->buffer, c->buflen)) < 0){
		err(255, "read()");
	}else{
		if(s->data && (c->szr = read(((struct files *)s->data)->file_fd, c->buffer, c->buflen)) < 0){
			err(255, "read()");
		}
	}
	return c->szr;
}
int connect_send(struct sockets *s){
	struct pollfd pfd = { 0, POLLOUT, 0 };
	int optval = 0;
	unsigned long int len = sizeof(optval);
	pfd.fd = s->fd_sck;
	fcntl(s->fd_sck,F_SETFL, O_NONBLOCK);
	if(connect(s->fd_sck, (struct sockaddr *)s->addr, (socklen_t)s->addrlen) < 0)
		if(errno != EINPROGRESS)
			err(255, "connect()");
	if(poll(&pfd, 1, -1) < 0)
		err(255, "poll()");
	if(getsockopt(s->fd_sck, SOL_SOCKET, SO_ERROR, &optval, (socklen_t *)&len) < 0)
		err(255, "getsockopt()");
	if((errno = optval) != 0)
		err(255, "connect()");
	while((*((struct files *)s->data)->szr = (int)read(((struct files *)s->data)->file_fd, *((struct files *)s->data)->buffer, *((struct files *)s->data)->buflen)) > 0){
		if(send(	s->fd_sck,
				*((struct files *)s->data)->buffer,
				*((struct files *)s->data)->szr,
				s->w_flags) < 0
		){
			err(255, "send()");
		}
	}
	if(*((struct files *)s->data)->szr < 0)
		warn("read()");
	s->op = 1;
	/*exit(0);*/
	return 0;
}
void connection(struct comunix *c, struct sockets *s){
	static int error_fd;
	static struct sockets *ps;
	static nfds_t i, j;
	/*struct addrinfo hints, *pres;
	MEMSET(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;*/
	for(s->c->curcli = 0; s->c->curcli < s->c->climax; s->c->curcli++){
		if(*s->c->clifd[s->c->curcli] == -1){
			if((error_fd = accept(	s->fd_sck,
									(struct sockaddr *)s->addr,
									(socklen_t *)&s->addrlen
							)) < 0
			){
				err(255, "accept()");
			}
			warnx("New client: %i.", error_fd);
			check_proxy(c->output, error_fd);
			*s->c->clifd[s->c->curcli] = error_fd;
			s->c->ncli++;
			break;
		}
	}
	if(s->c->curcli == s->c->climax){
		warnx("too many clients !!!");
		if((error_fd  = accept(	s->fd_sck,
						(struct sockaddr *)s->addr,
						(socklen_t *)&s->addrlen
				)) < 0
		){
			err(255, "accept()");
		}
		check_proxy(c->output, error_fd);
		if((s->c->climax + s->c->inc) * sizeof(int *) < s->c->climax || (c->nfds + s->c->inc)*sizeof(struct pollfd) < c->nfds){
			warnx("Can't alloc, too many clients");
			close(error_fd);
		}else{	
			warnx("New client: %i.", error_fd);
			if((s->c->clifd = realloc(s->c->clifd, (s->c->climax + s->c->inc)*sizeof(int *))) == NULL){
				err(255, "realloc()");
			}
			if((c->pfds = realloc(c->pfds, (c->nfds + s->c->inc)*sizeof(struct pollfd))) == NULL){
				err(255, "realloc()");
			}
			s->c->climax += s->c->inc;
			for(	j = stdin_set,
				c->r_pfds = c->pfds +stdin_set,
				ps = c->s,
				i = 1;
				ps;
				i = ps->server*(1+(ps->fd != STDIN_FILENO && ps->type != SOCK_DGRAM)),
				ps = ps->next,
				c->r_pfds += i,
				j += i
			);
			for(ps = c->s; ps; ps = ps->next){
				if(ps->type != SOCK_DGRAM && ps->server == 1){
					for(i = 0; i < ps->c->climax; i++, c->r_pfds++, j++){
						if(ps == s && i >= ps->c->ncli){
							if(j < c->nfds && i == ps->c->ncli){
								MEMCPY(c->r_pfds + s->c->inc , c->r_pfds, (c->nfds - j -1)*sizeof(struct pollfd));
							}
							c->r_pfds->fd = (i == ps->c->ncli) ? error_fd : -1;
							c->r_pfds->events = POLLIN;
						}
						ps->c->clifd[i] = &c->r_pfds->fd;
					}
				}else{
					if(ps->server == 0){
						c->r_pfds++;
						j += (ps->type != SOCK_DGRAM);
					}
				}
			}
			c->nfds += s->c->inc;
			#ifdef DEBUG
				test(c);
			#endif
		}
	}
}
void SendTo(struct comunix *c, struct sockets *s){
	static nfds_t i;
	printf("Sending to: %i\n", s->fd_sck);
	for(i = 0; i < s->c->climax; i++)
		if(*s->c->clifd[i] != -1 && *s->c->clifd[i] != c->r_pfds->fd){
			if(send(*s->c->clifd[i], c->buffer,
				c->szr, s->w_flags
				) <= 0
			){
				close(*s->c->clifd[i]);
				*s->c->clifd[i] = -1;
			}
	}
}
void Client_SendTo(struct comunix *c, struct sockets *s){
	printf("Sending to: %i\n", s->fd_sck);
	if(send(	s->fd_sck,
			c->buffer, c->szr,
			s->w_flags) < 0
	){
		err(255, "send()");
	}
}

ssize_t RecvFrom(struct comunix *c, struct sockets *s){
	if(c->r_pfds->fd == 0)
		return 0;
	switch((c->szr = recvfrom(c->r_pfds->fd, c->buffer, c->buflen,
				s->r_flags, (struct sockaddr *)s->addr,
				(socklen_t *)&s->addrlen))
	){
		case -1:
			warn("recvfrom()");
		case 0:
			warnx("%s %i exited.",
				(s->server) ? "Client" : "Server",
				c->r_pfds->fd
			);
			close(c->r_pfds->fd);
			c->r_pfds->fd = -1;
			if(s->server == 0){
				warnx("Server down");
				s->op = s->fd_sck = -1;
			}else{
				s->c->ncli--;
			}
	}
	return c->szr;
}
#include <string.h>
void SimpleOnRecvFrom(struct comunix *c, struct sockets *s){
	printf("Writing from: %i\n", s->fd_sck);
	if(c->szr == 0){
		warnx("%s %i exited.",
			(s->server) ? "Client" : "Server",
			c->r_pfds->fd
		);
		close(c->r_pfds->fd);
		c->r_pfds->fd = -1;
	}else{
		if(write(c->output, c->buffer, c->szr) < 0){
			err(255, "write()");
		}
	}
}
void DispachOnRecvFrom(struct comunix *c, struct sockets *s){
	printf("Writing from: %i\n", s->fd_sck);
	if(c->szr == 0){
		warnx("%s %i exited.",
			(s->server) ? "Client" : "Server",
			c->r_pfds->fd
		);
		close(c->r_pfds->fd);
		c->r_pfds->fd = -1;
	}else{	if(write(c->output, c->buffer, c->szr) < 0){
			err(255, "write()");
		}
		((struct perform *)s->p)->on_sck_out(c, s);
	}
}
void EchoOnRecvFrom(struct comunix *c, struct sockets *s){
	printf("Writing from: %i\n", s->fd_sck);
	if(c->szr == 0){
		warnx("%s %i exited.",
			(s->server) ? "Client" : "Server",
			c->r_pfds->fd
		);
		close(c->r_pfds->fd);
		c->r_pfds->fd = -1;
	}else{	if(write(c->output, c->buffer, c->szr) < 0){
			err(255, "write()");
		}
		if(send(c->r_pfds->fd, c->buffer,
			c->szr, s->w_flags
			) <= 0
		){
			close(c->r_pfds->fd);
			c->r_pfds->fd = -1;
		}
	}
}

