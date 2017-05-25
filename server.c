#include "util.h"
#include "common.h"

#include <errno.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

//maxlen for full address
#define MAX_ADDR_LEN (INET6_ADDRSTRLEN + MAX_SERVICE_LEN)
#define SA struct sockaddr

static void process_request(int sfd);
static void sigchld_handler(int unused);

int main(void)
{
	struct sigaction sa;
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	socklen_t srv_addrlen = 0;
	int srv_sfd = inet_listen(PORT_SRV, BACKLOG, &srv_addrlen);
	if (srv_sfd < 0) {
		fprintf(stderr, "could not create socket\n");
		exit(EXIT_FAILURE);
	}

	printf("waiting for connections...\n");
	struct sockaddr_storage client_addr;
	while(1) {

		socklen_t ca_size = sizeof(client_addr);
		int client_fd = accept(srv_sfd, (SA *)&client_addr, &ca_size);
		if (client_fd == -1) {
			perror("accept");
			continue;
		}

		char addr_str[MAX_ADDR_LEN] = {0};
		inet_addr_str((SA *)&client_addr, ca_size,
		              addr_str, sizeof(addr_str));
		printf("\ngot connection from %s\n", addr_str);

		char sbuf[MAX_SERVICE_LEN] = {0};
		socklen_t addrlen = 0;
		int req_sfd = inet_listen("0", BACKLOG, &addrlen);

		if ( req_sfd < 0 ||
		     socket_service(req_sfd, sbuf, sizeof(sbuf)) < 0 ) {
			fprintf(stderr, "could not create req_sfd\n");
			continue;
		} else {
			if (send(client_fd, sbuf, sizeof(sbuf), 0) == -1)
				perror("send");
		}

		switch(fork()) {
		case -1:
			perror("fork");
			break;
		case 0:
			close(srv_sfd);
			close(client_fd);
			process_request(req_sfd);
			close(req_sfd);
			_exit(0);
			break;
		default:
			break;
		}

		close(req_sfd);
		close(client_fd);
	}

	exit(EXIT_SUCCESS);
}

static void process_request(int sfd)
{
	pid_t pid = getpid();
	printf("process %d started\n", pid);

	enum states {
		wait_st, recieve_st, reply_st
	} state = wait_st;

	struct sockaddr_storage client_addr;
	socklen_t ca_size = sizeof(client_addr);

	int client_fd;
	char addr_str[MAX_ADDR_LEN] = {0};
	char msgbuf[MAXDSIZE] = {0};

	int processing = 1;
	while (processing) {
		switch(state) {
		case wait_st:
			memset(&client_addr, 0, ca_size);
			client_fd = accept(sfd, (SA *)&client_addr, &ca_size);
			if (client_fd == -1) {
				perror("accept");
				close(client_fd);
			} else {
				memset(addr_str, 0, sizeof(addr_str));
				inet_addr_str( (SA *)&client_addr, ca_size,
				               addr_str, sizeof(addr_str));
				printf("process %d got connection from %s\n",
				       pid, addr_str);

				state = recieve_st;
			}
			break;
		case recieve_st:
			memset(msgbuf, 0, sizeof(msgbuf));
			int nbytes = recv(client_fd, msgbuf, MAXDSIZE - 1, 0);
			if (nbytes == -1) {
				perror("recv");
			} else {
				msgbuf[nbytes] = '\0';
				printf("process %d got message '%s' from %s\n",
				       pid, msgbuf, addr_str);
				state = reply_st;
			}
			break;
		case reply_st:
			if (send(client_fd, MSG_SRV, sizeof(MSG_SRV), 0) == -1)
				perror("send");
			else
				processing = 0; //done
			break;

		default:
			printf("error? unknown state %d\n", state);
			break;
		}
	}
}

static void sigchld_handler(int unused)
{
	(void)unused;

	// waitpid() might overwrite errno
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

