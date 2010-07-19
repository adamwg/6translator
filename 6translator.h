#ifndef __6TRANSLATOR_H
#define __6TRANSLATOR_H

#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG
#define DPRINTF(x, vargs...) if(fg) { fprintf(stderr, x, ## vargs); } else { syslog(LOG_DEBUG, x, ## vargs); }
#else
#define DPRINTF(x, vargs...) 
#endif

#define LOG(level, x, vargs...) if(fg) { printf(x, ## vargs); } else { syslog(level, x, ## vargs); }

/* We start in READ.  When we get a message from the IPv4 socket, we go into
 * WRITE6 to send it to the IPv6 socket.  When we get a message from the IPv6
 * socket, we go into WRITE4 to send it to the IPv4 socket.  When we're done
 * sending a message we return to READ.
 */
enum cl_state { CL_UNUSED, CL_READ, CL_WRITE4, CL_WRITE6 };

#define BUF_SZ 32768
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

int fg;

/* Print the usage and exit with EX_USAGE */
void exit_usage();

/* Get the first unused client in the list of clients */
struct cl *get_free_cl();
/* Disconnect a client and remove it from the list */
void del_client();
/* Read from or write to the IPv4 and IPv6 sockets */
void do_read4(int);
void do_read6(int);
void do_write4(int);
void do_write6(int);


#endif /* __6TRANSLATOR_H */
