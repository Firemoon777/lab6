#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

int socket_fd;

void sighandler(int signo) {
	printf("\rClosing...\n");
	close(socket_fd);
	_exit(0);
}

void worker() {
	int client_fd;
	struct sockaddr_in client;
	socklen_t client_len = sizeof(client);
	char buf[BUFFER_SIZE];
	int buf_len;
	DIR* dir;
	struct dirent* dirent;
	size_t d_name_len, i;

	while(1) {
		client_fd = accept(socket_fd, 
				(struct socketaddr*)&client,
			 	&client_len);
		if(client_fd < 0) {
			perror("accept");
			continue;
		}
		fprintf(stderr, "accepted\n");

		/* buffered input? */
		/* one dirent per one line */
		buf_len = read(client_fd, buf, BUFFER_SIZE);
		if(buf_len < 0) {
			perror("read");
			close(client_fd);
			continue;
		}
		if(buf[buf_len-1] == '\n') 
			buf_len--;
		if(buf[buf_len-1] == '\r') 
			buf_len--;
		buf[buf_len] = '\0';
		fprintf(stderr, "request: \"%s\"\n", buf);

		/* try to open dir */
		dir = opendir(buf);
		if(dir == NULL) {
			/* error :C */
			char* str = strerror(errno);
			write(client_fd, str, strlen(str));
			write(client_fd, "\r\n", 2);
			close(client_fd);
			continue;
		}
		/* success */
		write(client_fd, "OK\r\n", 4);

		/* reading dirents */
		/* set errno to detect end of dir */
		errno = 0;
		while(1) {
			dirent = readdir(dir);
			/* error and end of dir handling */
			if(dirent == NULL) {
				if(errno == 0) {
					/* end of dir found */
					break;
				} else {
					/* WTF */
					/* error handling */

					/* continue reading */
					errno = 0;
					continue;
				}
			}
			/* output dirent name */
			/* remove \n symbols */
			d_name_len = strlen(dirent->d_name);
			for(i = 0; i < d_name_len; i++) {
				if(dirent->d_name[i] == '\n') {
					dirent->d_name[i] = '?';
				}
			}
			
			write(client_fd, dirent->d_name, d_name_len);
			write(client_fd, "\r\n", 2);

		}
	

		/* close everything */
		closedir(dir);
		close(client_fd);
		fprintf(stderr, "closed\n");
	}
}

int main() {
	struct sockaddr_in server;
	struct sigaction act;

	/* setup SIGINT handler*/
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = sighandler;
	sigaction(SIGINT, &act, NULL);
	
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
	if(bind(socket_fd, 
			(struct sockaddr*)&server, 
			sizeof(server)) < 0) {
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
