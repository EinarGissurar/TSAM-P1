#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>

#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK	4
#define ERROR 5

#define ERROR_CODE_0 0
#define ERROR_CODE_1 1
#define ERROR_CODE_2 2
#define ERROR_CODE_3 3
#define ERROR_CODE_4 4
#define ERROR_CODE_5 5
#define ERROR_CODE_6 6
#define ERROR_CODE_7 7


#define ERROR_0 "Unknown error.\0"
#define ERROR_1 "File not found.\0"
#define ERROR_2 "Access violation.\0"
#define ERROR_3 "Disk full or allocation exceeded.\0"
#define ERROR_4 "Illegal TFTP operation.\0"
#define ERROR_5 "Unknown transfer ID.\0"
#define ERROR_6 "File already exists.\0"
#define ERROR_7 "No such user.\0"

struct sockaddr_in server, client;

char message[516], reply[516], stream_path[255];
char* filename;
char* mode;
int error_code;
unsigned int sockfd, op_code, mode_pointer, buffer_size, i;
int datablock;
unsigned long block_code, block_reply;
FILE *data;

void write_reply() {
	memset(reply, 0, sizeof(reply)); // Clean reply

	buffer_size = 0;
	reply[0] = (3 >> 8) & 0xff;
	reply[1] = 3 & 0xff;
	reply[2] = (block_code >> 8) &0xff;
	reply[3] = block_code &0xff;
	while (buffer_size < 512) {
		if ((datablock = fgetc(data)) == EOF) {
			break;
		}
		reply[buffer_size+4] = (char)datablock;
		buffer_size++;
	}
	puts(reply);
	fprintf(stdout, "buffer size = %d\n", buffer_size);
	fprintf(stdout, "Size of reply: %d\n", buffer_size+4);
}

void write_error() {
	memset(reply, 0, sizeof(reply));
	reply[0] = (5 >> 8) & 0xff;
	reply[1] = 5 & 0xff;
	reply[2] = (error_code >> 8) & 0xff;
	reply[3] = error_code & 0xff;
	if (error_code == ERROR_CODE_0) {
		buffer_size = sizeof(ERROR_0);
		strncpy(&reply[4], ERROR_0, buffer_size);
	}
	else if (error_code == ERROR_CODE_1) {
		buffer_size = sizeof(ERROR_1);
		strncpy(&reply[4], ERROR_1, buffer_size);
	}
	else if (error_code == ERROR_CODE_2) {
		buffer_size = sizeof(ERROR_2);
		strncpy(&reply[4], ERROR_2, buffer_size);
	}
	else if (error_code == ERROR_CODE_3) {
		buffer_size = sizeof(ERROR_3);
		strncpy(&reply[4], ERROR_3, buffer_size);
	}
	else if (error_code == ERROR_CODE_4) {
		buffer_size = sizeof(ERROR_4);
		strncpy(&reply[4], ERROR_4, buffer_size);
	}
	else if (error_code == ERROR_CODE_5) {
		buffer_size = sizeof(ERROR_5);
		strncpy(&reply[4], ERROR_5, buffer_size);
	}
	else if (error_code == ERROR_CODE_6) {
		buffer_size = sizeof(ERROR_6);
		strncpy(&reply[4], ERROR_6, buffer_size);
	}
	else if (error_code == ERROR_CODE_7) {
		buffer_size = sizeof(ERROR_7);
		strncpy(&reply[4], ERROR_7, buffer_size);
	}
	//puts(reply);
	printf("Sending ERROR message: \"");
	for (i = 0; i < buffer_size; i++) {
		printf("%c",reply[i+4]);
	}
	printf("\"\n");
}


bool prefix(const char *prefix, const char *string) {
    return strncmp(prefix, string, strlen(prefix)) == 0;
}

void sig_handler(int signal_n) {
	if (signal_n == SIGINT)
		printf("\nShutting down...\n");

	/* Close the connection. */
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);

	exit(0);
}


int main(int argc, char *argv[]) {

	char shared_directory_path [PATH_MAX+1], absolute_file_path [PATH_MAX+1];
	char *ptr;

	// checking number of arguments
	if (argc != 3) {
		printf("Usage: %s <port> <folder_name>\n", argv[0]);
		return 1;
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("\nCannot catch SIGINT!\n");

	int port_number = strtol(argv[1], NULL, 10);
	char *shared_directory = argv[2];

	/* Create and bind a TCP socket */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	/* Network functions need arguments in network byte order instead of
	host byte order. The macros htonl, htons convert the values. */
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port_number);
	if (bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server)) < 0) {
		perror("Bind failed. Error");
		return 1;
	}
	listen(sockfd, 1);

	fprintf(stdout, "The server is on\n");
	ptr = realpath(shared_directory, shared_directory_path);
	if (ptr == NULL) {
		perror("Error: ");
		return(42);
	}
	// for making sure that prefix doesn't match file with similar name
	strcat(shared_directory_path, "/");
	printf("shared_directory_path: %s\n", shared_directory_path);

	while(1) {

		printf("\nWaiting...\n");

		/* We first have to accept a TCP connection, connfd is a fresh
		   handle dedicated to this connection. */
		socklen_t len = (socklen_t) sizeof(client);

		/* Receive from connfd, not sockfd. */
		ssize_t n = recvfrom(sockfd, message, sizeof(message) - 1, 0,
			(struct sockaddr *) &client, &len);
		printf("Size of received message [bytes]: %zu\n", n);
		op_code = message[1];
		printf("Opcode is: %d\n", op_code);

		switch(op_code) {
			case RRQ:
				//message to read/write
				filename = (char*)&message[2];
				//find mode
				for (mode_pointer = 2; message[mode_pointer] != '\0'; mode_pointer++) {}
				mode_pointer++;
				mode = &message[mode_pointer];

				//path to stream
				memset(stream_path, 0, sizeof(stream_path));
				strcpy(stream_path, shared_directory);
				strcat(stream_path, "/");
				strcat(stream_path, filename);
				// checking if req. file is inside shared directory

				printf("Filename is: %s\n", filename);
				printf("Mode is %s\n", mode);

				ptr = realpath(stream_path, absolute_file_path);
				if (ptr == NULL)
					perror("Error: ");
				else if (!prefix(shared_directory_path, absolute_file_path)) {
					error_code = ERROR_CODE_1;
					write_error();
					sendto(sockfd, reply, buffer_size+4, 0,
					  (struct sockaddr *) &client, (socklen_t) sizeof(client));
					printf("File is outside shared directory.\n[[ Access forbidden ]]\n");
					break;
				}
				else
					printf("Path is %s\n", absolute_file_path);

				//Flush reply
				data = fopen(stream_path, "r");
				memset(reply, 0, sizeof(reply));
				if (data == NULL) {
					error_code = ERROR_CODE_1;
					write_error();
				}
				else {
					fprintf(stdout, "File found\n");
					block_code = 1;
					write_reply();
				}
				sendto(sockfd, reply, buffer_size+4, 0,
					  (struct sockaddr *) &client, (socklen_t) sizeof(client));
				break;
			case WRQ:
				//Function to test case 3.
				/*
				reply[0] = (4 >> 8) & 0xff;
				reply[1] = 4 & 0xff;
				reply[2] = (0 >> 8) &0xff;
				reply[3] = 0 &0xff;
				sendto(sockfd, reply, 4, 0,
					  (struct sockaddr *) &client, (socklen_t) sizeof(client));
			break; */

				error_code = ERROR_CODE_2;
				write_error();
				sendto(sockfd, reply, buffer_size+4, 0,
					  (struct sockaddr *) &client, (socklen_t) sizeof(client));
				break;
			case DATA:
				error_code = ERROR_CODE_4;
				write_error();
				sendto(sockfd, reply, buffer_size+4, 0,
					  (struct sockaddr *) &client, (socklen_t) sizeof(client));
				break;
			case ACK:
				block_reply = (((unsigned char*)message)[2] << 8) + ((unsigned char*)message)[3];
				fprintf(stdout, "Blockode reply = %lu\n", block_reply);

				if (block_code == block_reply) {
					fprintf(stdout, "Data recieved\n");
					fflush(stdout);

					block_code++;
					fprintf(stdout, "Blockode to be sent is: %lu\n", block_code);
					write_reply();
				}
				else {
					sleep(1);
					fprintf(stdout, "Resend last package\n");
				}
				sendto(sockfd, reply, buffer_size+4, 0,
					  (struct sockaddr *) &client, (socklen_t) sizeof(client));
				break;
			case ERROR:
				fprintf(stdout, "Error code %d: %s\n", message[3], &message[4]);
				break;
		}
	}
	return 0;
}
