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
#define ERROR_6 "File already esists.\0"
#define ERROR_7 "No such user.\0"

struct sockaddr_in server, client;

char message[516], reply[516], stream_path[100];
char* filename;
char* mode;
int error_code;
unsigned int sockfd, port_number, op_code, mode_pointer, buffer_size, i;
int datablock;
unsigned long block_code, block_reply;
FILE *data;

void write_reply() {
	memset(reply, 0, strlen(reply)); // Clean reply
				
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
	memset(reply, 0, strlen(reply));
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
	puts(reply);
	for (i = 0; i < buffer_size; i++) {
		fprintf(stdout, "%c",reply[i+4]);
	}
	fprintf(stdout, "\n");
}

int main(int argc, char *argv[]) {
	port_number = strtol(argv[1], NULL, 10);

	/* Create and bind a TCP socket */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	/* Network functions need arguments in network byte order instead of 
	host byte order. The macros htonl, htons convert the values. */
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port_number);
	bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));

	fprintf(stdout, "The server is on\n");
	//fprintf(stdout, "argv[0] is: %s \n", argv[0]);
	//fprintf(stdout, "argv[1] is: %s \n", argv[1]);
	//fprintf(stdout, "argv[2] is: %s \n", argv[2]);
	//fprintf(stdout, "argv[3] is: %s \n", argv[3]);
	//fprintf(stdout, "Port number is: %i \n", port_number);
	
	fflush(stdout);
	listen(sockfd, 1);
	
	while(1) {

		fprintf(stdout, "Waiting...\n");

		/* We first have to accept a TCP connection, connfd is a fresh
		   handle dedicated to this connection. */
		socklen_t len = (socklen_t) sizeof(client);
	  
		/* Receive from connfd, not sockfd. */
		ssize_t n = recvfrom(sockfd, message, sizeof(message) - 1, 0, 
			(struct sockaddr *) &client, &len);
		fprintf(stdout, "Size of n: %zu\n", n);
		op_code = message[1];
		fprintf(stdout, "Opcode is: %d\n", op_code);

		switch(op_code) {
			case RRQ:
				//message to read/write
				filename = (char*)&message[2];

				//path to stream
				memset(stream_path, 0, strlen(stream_path));
				strcpy(stream_path, argv[2]);
				strcat(stream_path, "/");
				strcat(stream_path, filename);

				//find mode
				for (mode_pointer = 2; message[mode_pointer] != '\0'; mode_pointer++) {}
				mode_pointer++;
				mode = &message[mode_pointer];

				fprintf(stdout, "Filename is: %s\n", filename);
				fprintf(stdout, "Mode is %s\n", mode);
				fprintf(stdout, "Path is %s\n", stream_path);
				fflush(stdout);

				//Flush reply
				data = fopen(stream_path, "r");
				memset(reply, 0, strlen(reply));
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
		
		/* Close the connection. */
		//shutdown(connfd, SHUT_RDWR);
		//close(connfd);
	}
	return 0;
}