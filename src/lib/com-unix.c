#include "com-unix.h"
void *_memset(void *_d, const char c, const unsigned long int size){
	static int i;
	static unsigned long int src, *ps;
	char *s = _d;
	unsigned long int ___size___ = size%sizeof(unsigned long int),
		sz = size/sizeof(unsigned long int);
	for(;___size___; ___size___--, *s = c, s++);
	if(sz == 0)
		return _d;
	for(i = 0; src |= c, i < (int)sizeof(unsigned long int); i++)
		src <<= 8;
	ps = (unsigned long int *)s;
	for(;sz--; *ps = src, ps++);
	return _d;
}
void *_memcpy(void *_dst, const void *_src, const unsigned long int size){
	const char *d = _src;
	const unsigned long int *dd;
	char *s = _dst;
	unsigned long ___size___ = size%sizeof(unsigned long int),
		sz = size/sizeof(unsigned long int),
		*ds;
	s += ___size___;
	d += ___size___;
	if(sz){
		ds = ((unsigned long int *)s) + sz;
		dd = ((unsigned long int *)d) + sz;
		for(;sz--;ds--, dd--, *ds = *dd);
	}
	for(;___size___--;s--, d--, *s = *d);
	return _dst;
}
char *_strcpy(char *str1, const char *str2){
	char *s = str1;
	for(;*str1 = *str2, *str2 != 0; str1++, str2++);
	return s;
	
}
unsigned long int _strlen(const char *str){
	const char *s;
	for(s = str; *s != 0; s++);
	return s - str;
}
unsigned long int _strcmp(const char *str1, const char *str2){
        const char *d = str1, *s = str2;
	const unsigned long int *dd, *ds;
	unsigned long int len1 = _strlen(str1),
		len2 = _strlen(str2),
		___size___, sz;
	if(len1 > len2)
		return 1;
	if(len1 < len2)
		return -1;
	___size___ = len1%sizeof(unsigned long int), sz = len1/sizeof(unsigned long int);
	for(;--___size___ && *d == *s; d++,s++);
	if(sz == 0){
        	return *d - *s;
	}
	dd = (unsigned long int *)d;
	ds = (unsigned long int *)s;
	for(;--sz && *ds == *dd;ds++, dd++);
	return *dd - *ds;
}
int binding(struct sockets *s){
	if(bind(s->fd_sck, (struct sockaddr *)s->addr, (socklen_t)s->addrlen) < 0){
		warn("bind()");
	}
	s->op = 1;
	return 0;
}
int listening(struct sockets *s){
	if(bind(s->fd_sck, (struct sockaddr *)s->addr, (socklen_t)s->addrlen) < 0){
		warn("bind()");
		return errno;
	}
	if(listen(s->fd_sck, s->c->backlog) < 0){
		warn("listen()");
		return errno;
	}
	s->op = 1;
	return 0;
}
int connecting(struct sockets *s){
	if(connect(s->fd_sck, (struct sockaddr *)s->addr, (socklen_t)s->addrlen) < 0){
		return errno;
	}
	s->op = 1;
	return 0;
}
void Server_dgram(void *c, struct sockets *s){
		if(COMUNIX(c)->r_pfds->fd == s->fd){
			if((COMUNIX(c)->szr = STRLEN(COMUNIX(c)->buffer)) > 0 ||
					(COMUNIX(c)->szr = ((struct perform *)s->p)->r_on_local(c, s)) > 0
			){
				((struct perform *)s->p)->on_sck_out(c, s);
			}
		}else{
			if((COMUNIX(c)->szr = ((struct perform *)s->p)->on_sck_in(c, s)) > 0){
				((struct perform *)s->p)->w_on_local(c, s);
			}
		}
}
void Client_dgram(void *c, struct sockets *s){
	if(COMUNIX(c)->r_pfds->fd == s->fd){
		if((COMUNIX(c)->szr = STRLEN(COMUNIX(c)->buffer)) > 0 ||
			(COMUNIX(c)->szr = ((struct perform *)s->p)->r_on_local(c, s)) > 0
		){
			((struct perform *)s->p)->on_sck_out(c, s);
		}
	}else{
		if((COMUNIX(c)->szr = ((struct perform *)s->p)->on_sck_in(c, s)) > 0){
			((struct perform *)s->p)->w_on_local(c, s);
		}
	}
}
void Server_void(void *c, struct sockets *s){
	/*static nfds_t fds;*/
	if(COMUNIX(c)->r_pfds->fd == s->fd_sck)
		((struct perform *)COMUNIX(c)->s->p)->new_con(c, s);
	else
		if((COMUNIX(c)->szr = STRLEN(COMUNIX(c)->buffer)) > 0 ||
			(COMUNIX(c)->szr = ((struct perform *)s->p)->on_sck_in(c, s)) > 0
		){
			((struct perform *)s->p)->w_on_local(c, s);
		}
}
void Server(void *c, struct sockets *s){
	if(COMUNIX(c)->r_pfds->fd == s->fd_sck){
		((struct perform *)s->p)->new_con(c, s);
	}else{
		if(COMUNIX(c)->r_pfds->fd == s->fd){
			if((COMUNIX(c)->szr = STRLEN(COMUNIX(c)->buffer)) > 0 ||
					(COMUNIX(c)->szr = ((struct perform *)s->p)->r_on_local(c, s)) > 0
			){
				((struct perform *)s->p)->on_sck_out(c, s);
			}
		}else{
			if((COMUNIX(c)->szr = ((struct perform *)s->p)->on_sck_in(c, s)) > 0){
				((struct perform *)s->p)->w_on_local(c, s);
			}
		}
	}
}
void Client(void *c, struct sockets *s){
	if(COMUNIX(c)->r_pfds->fd == s->fd){
		if((COMUNIX(c)->szr = STRLEN(COMUNIX(c)->buffer)) > 0 ||
			(COMUNIX(c)->szr = ((struct perform *)s->p)->r_on_local(c, s)) > 0
		){
			((struct perform *)s->p)->on_sck_out(c, s);
		}
	}else{
		if((COMUNIX(c)->szr = ((struct perform *)s->p)->on_sck_in(c, s)) > 0){
			((struct perform *)s->p)->w_on_local(c, s);
		}
	}
}
void destroy_comunix(struct comunix *c){
	nfds_t i, j = c->nfds;
	struct sockets *ds, *pds;
	for(i = 0; i < j; i++){
		if(c->pfds[i].fd > 0){
			if(close(c->pfds[i].fd) < 0)
				warn("close()");
		}
	}
	free(c->pfds);
	free(c->buffer);
	c->nfds = 0;
	for(ds = c->s; ds != NULL; ds = pds){
		if(ds->destroy_data)
			(*ds->destroy_data)(ds->data);
		if(ds->server && ((struct sockaddr_un *)ds->addr)->sun_family == AF_UNIX){
			unlink(((struct sockaddr_un *)ds->addr)->sun_path);
		}
		if(ds->c){
			if(ds->c->clifd){
				free(ds->c->clifd);
			}
			free(ds->c);
		}
		free(ds->p);
		free(ds->addr);
		pds = ds->next;
		free(ds);
	}
	free(c);
}
struct sockets *find_sck(int fd, struct sockets *s){
	static nfds_t i;
	struct sockets *ps = s;
	do{
		if(ps == NULL)
			break;
		else
			if(fd == ps->fd_sck || fd == ps->fd)
				return ps;
		if(ps->type != SOCK_DGRAM && ps->server == 1)
			for(i = 0;i < ps->c->climax; i++)
				if(fd == *ps->c->clifd[i])
					return ps;
		ps = ps->next;
	}while(ps);
	return NULL;
}

