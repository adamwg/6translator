#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

enum cl_state { UNUSED, READ, WRITE4, WRITE6 };

#define BUF_SZ 4096
struct cl {
	int sock6;
	struct sockaddr_in6 addr6;
	unsigned int addr6len;
	int sock4;
	char buf[BUF_SZ];
	int sz;
	int bp;
	enum cl_state state;
};

#define N_CLIENTS 1024
struct cl clients[N_CLIENTS];

struct cl *get_free_cl() {
	int i;
	for(i = 0; i < N_CLIENTS; i++) {
		if(clients[i].state == UNUSED) {
			return(&clients[i]);
		}
	}

	return(NULL);
}

void do_read4(int cn) {
	struct cl *cl = &clients[cn];

	cl->sz = read(cl->sock4, cl->buf, BUF_SZ);
	cl->bp = 0;

	cl->state = WRITE6;
}

void do_read6(int cn) {
	struct cl *cl = &clients[cn];

	cl->sz = read(cl->sock6, cl->buf, BUF_SZ);
	cl->bp = 0;

	cl->state = WRITE4;
}

void do_write4(int cn) {
	struct cl *cl = &clients[cn];

	cl->bp += write(cl->sock4, cl->buf + cl->bp, cl->sz - cl->bp);
	
	if(cl->bp == cl->sz) {
		cl->state = READ;
	}
}

void do_write6(int cn) {
	struct cl *cl = &clients[cn];

	cl->bp += write(cl->sock6, cl->buf + cl->bp, cl->sz - cl->bp);
	if(cl->bp == cl->sz) {
		cl->state = READ;
	}
}
	
int main(int argc, char **argv) {

	struct cl *c;
	int i;

	int port6 = -1;
	int port4 = -1;

	int mysock;
	socklen_t sin6_size = sizeof(struct sockaddr_in6);
	struct sockaddr_in6 myaddr6;
	struct sockaddr_in srvaddr;
	int srvaddrlen;

	fd_set rfds, wfds;
	int st, ms;

	if(argc != 3) {
		printf("Usage: %s <ipv6 port> <ipv4 port>\n", argv[0]);
		exit(EX_USAGE);
	}

	port6 = atoi(argv[1]);
	port4 = atoi(argv[2]);

	myaddr6.sin6_family = AF_INET6;
	myaddr6.sin6_port = htons(port6);
	myaddr6.sin6_addr = in6addr_any;

	srvaddrlen = sizeof(struct sockaddr_in);
	memset(&srvaddr, 0, srvaddrlen);
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_port = htons(port4);
	srvaddr.sin_addr.s_addr = inet_addr("localhost");

	mysock = socket(AF_INET6, SOCK_STREAM, 0);
	bind(mysock, (struct sockaddr *)&myaddr6, sin6_size);
	listen(mysock, N_CLIENTS);

	while(1) {
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(mysock, &rfds);
		ms = mysock;
		
		for(i = 0; i < N_CLIENTS; i++) {
			if(clients[i].state != UNUSED) {
				if(clients[i].state == READ) {
					FD_SET(clients[i].sock4, &rfds);
					FD_SET(clients[i].sock6, &rfds);
					if(c->sock6 > ms) {
						ms = c->sock6;
					}
				} else if(clients[i].state == WRITE6) {
					FD_SET(clients[i].sock6, &wfds);
					if(c->sock6 > ms) {
						ms = c->sock6;
					}
				} else if(clients[i].state == WRITE4) {
					FD_SET(clients[i].sock4, &wfds);
					if(c->sock4 > ms) {
						ms = c->sock4;
					}
				}
			}
		}

		st = select(ms + 1, &rfds, &wfds, NULL, NULL);
		if(st == -1) {
			continue;
		}
		if(FD_ISSET(mysock, &rfds)) {
			c = get_free_cl();
			if(c == NULL) {
				/* Can't accept more connections right now */
				continue;
			}

			c->bp = 0;

			c->state = READ;
			c->sock6 = accept(mysock, (struct sockaddr *)&c->addr6, &c->addr6len);
			c->sock4 = socket(AF_INET, SOCK_STREAM, 0);
			
			connect(c->sock4, (struct sockaddr *)&srvaddr, srvaddrlen);
		}

		for(i = 0; i < N_CLIENTS; i++) {
			if(clients[i].state == READ && FD_ISSET(clients[i].sock4, &rfds)) {
				do_read4(i);
			} else if(clients[i].state == READ && FD_ISSET(clients[i].sock6, &rfds)) {
				do_read6(i);
			} else if(clients[i].state == WRITE4 && FD_ISSET(clients[i].sock4, &wfds)) {
				do_write4(i);
			} else if(clients[i].state == WRITE6 && FD_ISSET(clients[i].sock6, &wfds)) {
				do_write6(i);
			}
		}
	}

	return(EX_OK);
}
