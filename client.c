#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define PORT 58015
#define BUFFER_SIZE 80

int main(int argc, char* argv[]) {
	int socket_fd, i;
	struct sockaddr_in server_sock;
	struct hostent* server;
	int buf_len;
	char buf[BUFFER_SIZE];

	if(argc < 2) {
		fprintf(stderr, "Usage: %s server path...\n", argv[0]);
		return -1;
	}

	for(i = 2; i < argc; i++) {
		/* create socket */
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if(socket_fd < 0) {
			perror("socket");
			return 1;
		}
	
		server = gethostbyname(argv[1]);
		if(server == NULL) {
			perror("gethostbyname");
			return 3;
		}
		
		bzero((char *) &server_sock, sizeof(server_sock));
		server_sock.sin_family = AF_INET;
		bcopy((char *)server->h_addr,
		      (char *)&server_sock.sin_addr.s_addr,
			        server->h_length);
		server_sock.sin_port = htons(PORT);


		if(connect(socket_fd, (struct sockaddr*)&server_sock, sizeof(struct sockaddr)) < 0) {
			perror("connect");
			return 2;
		}
		write(socket_fd, argv[i], strlen(argv[i]));
		write(socket_fd, "\n", 1);

		while((buf_len = read(socket_fd, buf, BUFFER_SIZE)) > 0) {
			write(STDOUT_FILENO, buf, buf_len);
		}
		if(buf_len < 0) {
			perror("read");
		}

	}


	return 0;
}
