#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 80

int socket_fd;

void worker() {
	int client_fd;
	struct sockaddr_in client;
	socklen_t client_len;
	char buf[BUFFER_SIZE];
	int buf_len;
	
	client_len = sizeof(client);

	while(1) {
		client_fd = accept(socket_fd, (struct socketaddr*)&client, &client_len);
		if(client_fd < 0) {
			perror("accept");
			continue;
		}
		fprintf(stderr, "accepted");

		while((buf_len = read(client_fd, buf, BUFFER_SIZE)) > 0) {
			write(client_fd, buf, buf_len);
		}

		if(buf_len < 0) {
			perror("read");
		}

		close(client_fd);
		fprintf(stderr, "closed");
	}
}

int main() {
	struct sockaddr_in server;
	
	/* create socket*/
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0) {
		perror("socket");
		return 1;
	}

	/* prepare to bind */
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(58015);

	/* bind */
	if(bind(socket_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
		perror("bind");
		return 2;
	}

	/* listen */
	listen(socket_fd, 5);

	/* magic { */

	/* } */

	worker();
	return 0;
}
