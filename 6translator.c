/*******************************************************************************
 * 6translator - Accepts connections on IPv6, connects to an IPv4 service,     *
 *               shuffles data between the connections.  That's all!           *
 *                                                                             *
 * Copyright (c) 2010 Adam Wolfe Gordon <awg@xvx.ca>                           *
 *                                                                             *
 * Licensed under the MIT license.  See the COPYING file for details.          *
 *                                                                             *
 *******************************************************************************/

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#include "6translator.h"

int main(int argc, char **argv) {

	struct cl *c;
	int i;

	int opt;
	char *p;
	
	char *host6 = NULL;
	char *host4 = NULL;
	int port6 = -1;
	int port4 = -1;

	int mysock;
	struct sockaddr_in srvaddr;
	struct sockaddr_in6 myaddr;
	int srvaddrlen;
	struct addrinfo hints;
	struct addrinfo *v4addr;
	struct addrinfo *v6addr;
	struct addrinfo *ai;

	fd_set rfds, wfds;
	int st, ms;

	/* Get the addresses set up */
	srvaddrlen = sizeof(struct sockaddr_in);
	memset(&srvaddr, 0, srvaddrlen);
	srvaddr.sin_family = AF_INET;

	/* Daemonize unless we're told otherwise */
	fg = 0;

	/* Options: -6, -4, -f */
	while((opt = getopt(argc, argv, "4:6:f")) != -1) {
		switch(opt) {
		case '6':
			if((p = strchr(optarg, ',')) != NULL) {
				*p = '\0';
				p += 1;
				port6 = atoi(p);
				host6 = optarg;
			} else {
				port6 = atoi(optarg);
			}
			break;
			
		case '4':
			if((p = strchr(optarg, ',')) != NULL) {
				*p = '\0';
				p += 1;
				port4 = atoi(p);
				host4 = optarg;
			} else {
				port4 = atoi(optarg);
			}
			break;

		case 'f':
			fg = 1;
			break;

		default:
			exit_usage();
		}
	}

	/* Check that we got the required arguments */
	if(port4 == -1 || port6 == -1) {
		exit_usage();
	}

	printf("Listening on port %d (IPv6), connecting on port %d (IPv4)\n", port6, port4);

	/* Daemonize */
	if(!fg) {
		if(fork()) {
			exit(EX_OK);
		}
		setsid();
		if(fork()) {
			exit(EX_OK);
		}
		chdir("/");
		close(0);
		close(1);
		close(2);
	}

	/* Start up syslog */
	openlog("6translator", 0, LOG_USER);

	memset(&myaddr, 0, sizeof(struct sockaddr_in6));
	if(host6 == NULL) {
		if((mysock = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
			LOG(LOG_ERR, "Error: could not create listening socket\n");
			exit(EX_OSERR);
		}

		myaddr.sin6_family = AF_INET6;
		myaddr.sin6_addr = in6addr_any;
		myaddr.sin6_port = htons(port6);
		if(bind(mysock, (struct sockaddr *)&myaddr, sizeof(struct sockaddr_in6)) != 0) {
			LOG(LOG_ERR, "Error: could not bind listening socket\n");
			exit(EX_OSERR);
		}
	} else {
		hints.ai_flags = 0;
		hints.ai_family = AF_INET6;
		hints.ai_protocol = 0;
		hints.ai_socktype = SOCK_STREAM;
		if((st = getaddrinfo(host6, NULL, &hints, &v6addr)) != 0) {
			LOG(LOG_ERR, "Error: could not resolve v6 host: %s\n", gai_strerror(st));
			exit(EX_OSERR);
		}

		if((mysock = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
			LOG(LOG_ERR, "Error: could not create listening socket\n");
			exit(EX_OSERR);
		}
		for(ai = v6addr; ai != NULL; ai = ai->ai_next) {
			((struct sockaddr_in6 *)ai->ai_addr)->sin6_port = htons(port6);
			if(bind(mysock, ai->ai_addr, ai->ai_addrlen) == 0) {
				break;
			}
			close(mysock);
		}

		if(ai == NULL) {
			LOG(LOG_ERR, "Error: could not bind listening socket\n");
			exit(EX_OSERR);
		}

		freeaddrinfo(v6addr);
	}

	if(host4 == NULL) {
		srvaddr.sin_family = AF_INET;
		srvaddr.sin_port = htons(port4);
		srvaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	} else {
		hints.ai_flags = 0;
		hints.ai_family = AF_INET;
		hints.ai_protocol = 0;
		hints.ai_socktype = SOCK_STREAM;
		if((st = getaddrinfo(host4, NULL, &hints, &v4addr)) != 0) {
			LOG(LOG_ERR, "Error: could not resolve v4 host: %s\n", gai_strerror(st));
			exit(EX_OSERR);
		}

		if(v4addr == NULL) {
			LOG(LOG_ERR, "Error: could not resolve v4 host\n");
			exit(EX_OSERR);
		}

		memcpy(&srvaddr, v4addr->ai_addr, sizeof(struct sockaddr_in));
		srvaddr.sin_family = AF_INET;
		srvaddr.sin_port = htons(port4);
		freeaddrinfo(v4addr);
	}

	DPRINTF("Ready to listen...\n");

	if(listen(mysock, N_CLIENTS) == -1) {
		LOG(LOG_ERR, "Error: could not listen\n");
		exit(EX_OSERR);
	}

	DPRINTF("Listening.\n");

	while(1) {
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(mysock, &rfds);
		ms = mysock;
		
		for(i = 0; i < N_CLIENTS; i++) {
			if(clients[i].state != CL_UNUSED) {
				if(clients[i].state == CL_READ) {
					FD_SET(clients[i].sock4, &rfds);
					FD_SET(clients[i].sock6, &rfds);
					if(c->sock6 > ms) {
						ms = c->sock6;
					}
					if(c->sock4 > ms) {
						ms = c->sock4;
					}
				} else if(clients[i].state == CL_WRITE6) {
					FD_SET(clients[i].sock6, &wfds);
					if(c->sock6 > ms) {
						ms = c->sock6;
					}
				} else if(clients[i].state == CL_WRITE4) {
					FD_SET(clients[i].sock4, &wfds);
					if(c->sock4 > ms) {
						ms = c->sock4;
					}
				}
			}
		}

		DPRINTF("fd_sets ready... selecting\n");

		st = select(ms + 1, &rfds, &wfds, NULL, NULL);
		if(st == -1) {
			DPRINTF("Select returned -1\n");
			continue;
		}
		if(FD_ISSET(mysock, &rfds)) {
			DPRINTF("Got connection\n");
			c = get_free_cl();
			if(c == NULL) {
				/* Can't accept more connections right now */
				continue;
			}

			c->bp = 0;

			c->state = CL_READ;
			c->sock6 = accept(mysock, (struct sockaddr *)&c->addr6, &c->addr6len);
			c->sock4 = socket(AF_INET, SOCK_STREAM, 0);
			
			if(connect(c->sock4, (struct sockaddr *)&srvaddr, srvaddrlen) == -1) {
				DPRINTF("Could not connect\n");
			}
		}

		for(i = 0; i < N_CLIENTS; i++) {
			if(clients[i].state == CL_READ && FD_ISSET(clients[i].sock4, &rfds)) {
				DPRINTF("Read 4\n");
				do_read4(i);
			} else if(clients[i].state == CL_READ && FD_ISSET(clients[i].sock6, &rfds)) {
				DPRINTF("Read 6\n");
				do_read6(i);
			} else if(clients[i].state == CL_WRITE4 && FD_ISSET(clients[i].sock4, &wfds)) {
				DPRINTF("Write 4\n");
				do_write4(i);
			} else if(clients[i].state == CL_WRITE6 && FD_ISSET(clients[i].sock6, &wfds)) {
				DPRINTF("Write 6\n");
				do_write6(i);
			}
		}
	}

	closelog();
	return(EX_OK);
}

void exit_usage() {
	fprintf(stderr, "Usage: 6translator [-f] -6 [<ipv6 host>,]<ipv6 port> -4 [<ipv4 host>,]<ipv4 port>\n\n");
	fprintf(stderr, "  -6 <ipv6 host>,<ipv6 port>  Specifies the (IPv6) host and port to listen on.\n");
	fprintf(stderr, "     Default host: any\n\n");
	fprintf(stderr, "  -4 <ipv4 host>,<ipv4 port>  Specifies the (IPv4) host and port to connect to.\n");
	fprintf(stderr, "     Default host: localhost\n\n");
	fprintf(stderr, "  -f Stay in foreground.\n");
	
	exit(EX_USAGE);
}

struct cl *get_free_cl() {
	int i;
	for(i = 0; i < N_CLIENTS; i++) {
		if(clients[i].state == CL_UNUSED) {
			return(&clients[i]);
		}
	}

	return(NULL);
}

void del_client(int cn) {
	struct cl *cl = &clients[cn];

	shutdown(cl->sock4, SHUT_RDWR);
	shutdown(cl->sock6, SHUT_RDWR);
	close(cl->sock4);
	close(cl->sock6);

	cl->state = CL_UNUSED;
}

void do_read4(int cn) {
	struct cl *cl = &clients[cn];

	cl->sz = recv(cl->sock4, cl->buf, BUF_SZ, 0);
	if(cl->sz == -1) {
		LOG(LOG_WARNING, "Error reading sock4\n");
		return;
	} else if(cl->sz == 0) {
		DPRINTF("Close from sock4\n");
		del_client(cn);
		return;
	}
	
	cl->bp = 0;
	cl->state = CL_WRITE6;
}

void do_read6(int cn) {
	struct cl *cl = &clients[cn];

	cl->sz = recv(cl->sock6, cl->buf, BUF_SZ, 0);
	if(cl->sz == -1) {
		LOG(LOG_WARNING, "Error reading sock6\n");
		return;
	} else if(cl->sz == 0) {
		DPRINTF("Close from sock6\n");
		del_client(cn);
		return;
	}
	
	cl->bp = 0;
	cl->state = CL_WRITE4;
}

void do_write4(int cn) {
	struct cl *cl = &clients[cn];
	int rd;

	rd = send(cl->sock4, cl->buf + cl->bp, cl->sz - cl->bp, 0);
	if(rd == -1) {
		LOG(LOG_WARNING, "Error writing sock4\n");
		del_client(cn);
		return;
	}
	cl->bp += rd;

	if(cl->bp == cl->sz) {
		DPRINTF("Done writing, now in CL_READ\n");
		cl->state = CL_READ;
	}
}

void do_write6(int cn) {
	struct cl *cl = &clients[cn];
	int rd;

	rd = send(cl->sock6, cl->buf + cl->bp, cl->sz - cl->bp, 0);
	if(rd == -1) {
		LOG(LOG_WARNING, "Error writing sock6\n");
		del_client(cn);
		return;
	}
	cl->bp += rd;

	if(cl->bp == cl->sz) {
		DPRINTF("Done writing, now in CL_READ\n");
		cl->state = CL_READ;
	}
}
