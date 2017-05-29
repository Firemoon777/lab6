#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define N 3
#define K 5
#define MAX 100

enum status {
	STATUS_IDLE = 0,
	STATUS_BUZY = 1,
	STATUS_KILL = 2,
	STATUS_DEAD = 3
};

struct p_t {
	pid_t pid;
	int status;
};

struct msg_t {
	long uid;
};

struct server_t {
	struct p_t process[MAX];
	int process_count;
	int process_count_in_use;
};

int socket_fd;
int mem_id;
int msg_id;
int process_uid = 0;
struct server_t* server_data;

void sighandler(int signo) {
	printf("\rClosing...\n");
	close(socket_fd);
	msgctl(msg_id, IPC_RMID, NULL);
	shmctl(mem_id, IPC_RMID, NULL);
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
	struct msg_t msg;
	
	msg.uid = process_uid+1;

	while(1) {
		if(server_data->process[process_uid].status == STATUS_KILL) {
			server_data->process[process_uid].status = STATUS_DEAD;
			msgsnd(msg_id, &msg, sizeof(msg), 0);
			_exit(0);
		}
		if(server_data->process[process_uid].status == STATUS_BUZY) {
			server_data->process[process_uid].status = STATUS_IDLE;
			msgsnd(msg_id, &msg, sizeof(msg), 0);
		}
		client_fd = accept(socket_fd, 
				(struct socketaddr*)&client,
			 	&client_len);
		if(client_fd < 0) {
			perror("accept");
			continue;
		}
		server_data->process[process_uid].status = STATUS_BUZY;
		msgsnd(msg_id, &msg, sizeof(msg), 0);
		fprintf(stderr, "[%3i]accepted\n", process_uid);

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
		fprintf(stderr, "[%3i]request: \"%s\"\n", process_uid, buf);

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
		fprintf(stderr, "[%3i]closed\n", process_uid);
	}
}

void fork_worker(int k) {
	int i, j;
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	/* fork-magic { */
	for(i = 0; i < k; i++) {
		int pid;
		for(j = 0; j < server_data->process_count; j++) {
			if(server_data->process[j].status == STATUS_DEAD) {
				break;
			}
		}
		process_uid = j;
		pid = fork();
		switch(pid) {
			case -1:
				/* oops */
				perror("fork");
				_exit(6);
			case 0:
				/* set default handler */ 
				sigaction(SIGINT, &act, NULL);
				/* launch service */
				worker();
				break;			
			default:
				/* Okay, one more process forked */
				printf("fork %i success\n", process_uid);
				/* write our pid */
				server_data->process[process_uid].pid = pid;
				server_data->process_count++;
				break;
		}
	}
}

int main() {
	int free_p, i;
	struct sockaddr_in server;
	struct sigaction act;

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

	/* setup shared memory */
	mem_id = shmget(IPC_PRIVATE, 
			sizeof(struct server_t), 
			IPC_CREAT | 0600);
	if(mem_id < 0) {
		perror("shmget");
		return 3;
	}
	server_data = (struct server_t*)shmat(mem_id, NULL, 0);
	if(server_data == NULL) {
		perror("shmat");
		return 4;
	}
	server_data->process_count = 0;
	server_data->process_count_in_use = 0;

	/* setup message queue */
	msg_id = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
	if(msg_id < 0) { 
		perror("msgget");
		return 5;
	}
	
	fork_worker(K - 1);

	/* setup SIGINT handler*/
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = sighandler;
	sigaction(SIGINT, &act, NULL);

	printf("Main process idle\n");
	free_p = server_data->process_count;

	while(1) {
		struct msg_t msg;
		msgrcv(msg_id, &msg, sizeof(struct msg_t), 0, 0);
		msg.uid--;
		printf("Msg from %li\n", msg.uid);
		if(server_data->process[msg.uid].status == STATUS_BUZY) {
			server_data->process_count_in_use++;
		}
		if(server_data->process[msg.uid].status == STATUS_IDLE) {
			server_data->process_count_in_use--;
		}
		free_p = server_data->process_count - server_data->process_count_in_use;

		if(free_p < N) {
			printf("Time to fork!\n");
			fork_worker(K - N);
		}

		if(free_p > K) {
			printf("Time to kill!\n");

		}

		printf("All: %i\n", server_data->process_count);
		printf("in_use: %i\n", server_data->process_count_in_use);
		for(i = 0; i < server_data->process_count; i++) {
			printf("> %i = status%i\n", i, server_data->process[i].status);
		}
		
	}

	return 0;
}
