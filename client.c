#include "util.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: client ip\n");
		exit(EXIT_FAILURE);
	}

	// recieve port from server
	int sockfd = inet_connect(argv[1], PORT_SRV, SOCK_STREAM);
	if (sockfd < 0) {
		fprintf(stderr, "socket error (%s, %s)", argv[1], PORT_SRV);
		exit(EXIT_FAILURE);
	}

	char pbuf[MAX_SERVICE_LEN] = {0};
	int numbytes = recv(sockfd, pbuf, sizeof(pbuf), 0);
	if (numbytes < 0 ) {
		perror("recv");
		exit(EXIT_FAILURE);
	}
	printf("received port '%s'\n", pbuf);

	// connecting to received port
	int sockfd2 = inet_connect(argv[1], pbuf, SOCK_STREAM);
	if (sockfd2 < 0) {
		fprintf(stderr, "socket error (%s, %s)\n", argv[1], pbuf);
		exit(EXIT_FAILURE);
	}

	if (send(sockfd2, MSG_CLIENT, sizeof(MSG_CLIENT), 0) == -1) {
		perror("send");
		exit(EXIT_FAILURE);
	}

	char buf[MAXDSIZE] = {0};
	numbytes = recv(sockfd2, buf, MAXDSIZE - 1, 0);
	if (numbytes == -1) {
		perror("recv");
		exit(EXIT_FAILURE);
	}
	buf[numbytes] = '\0';

	printf("received message '%s'\n", buf);

	exit(EXIT_SUCCESS);
}

