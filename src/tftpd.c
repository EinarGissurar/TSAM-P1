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

#define ERROR0 "Unknown error.\0"
#define ERROR1 "File not found.\0"
#define ERROR2 "Access violation.\0"
#define ERROR3 "Disk full or allocation exceeded.\0"
#define ERROR4 "Illegal TFTP operation.\0"
#define ERROR5 "Unknown transfer ID.\0"
#define ERROR6 "File already esists.\0"
#define ERROR7 "No such user.\0"

struct sockaddr_in server, client;

unsigned char message[516], reply[516];
char stream_path[100];
char* filename;
char* mode;
int error_code;
unsigned int sockfd, port_number, op_code, mode_pointer, buffer, i;
long datablock;
unsigned long block_code, block_reply;
FILE *data;

void write_reply() {
	memset(reply, 0, strlen(reply)); 		    
	buffer = 0;
	reply[0] = (3 >> 8) & 0xff;
	reply[1] = 3 & 0xff;
	reply[2] = (block_code >> 8) &0xff;
	reply[3] = block_code &0xff;
	while ((datablock = fgetc(data)) != EOF && buffer < 512) {
   		reply[buffer+4] = datablock & 0xff;
        buffer++;
   	}
   	puts(reply);
   	fprintf(stdout, "buffer = %d\n", buffer);
   	fprintf(stdout, "Size of reply: %d\n", buffer+4);
}

void write_error() {
	memset(reply, 0, strlen(reply));
	reply[0] = (5 >> 8) & 0xff;
   	reply[1] = 5 & 0xff;
   	reply[2] = (error_code >> 8) & 0xff;
   	reply[3] = error_code & 0xff;
   	if (error_code == 0) {
   		strcat(reply, ERROR0);
   		buffer = sizeof(ERROR0);
   	}
   	else if (error_code == 1) {
   		strcat(reply, ERROR1);
   		buffer = sizeof(ERROR1);
   	}
   	else if (error_code == 2) {
   		strcat(reply, ERROR2);
   		buffer = sizeof(ERROR2);
   	}
   	else if (error_code == 3) {
   		strcat(reply, ERROR3);
   		buffer = sizeof(ERROR3);
   	}
   	else if (error_code == 4) {
   		strcat(reply, ERROR4);
   		buffer = sizeof(ERROR4);
   	}
   	else if (error_code == 5) {
   		strcat(reply, ERROR5);
   		buffer = sizeof(ERROR5);
   	}
   	else if (error_code == 6) {
   		strcat(reply, ERROR6);
   		buffer = sizeof(ERROR6);
   	}
   	else if (error_code == 7) {
   		strcat(reply, ERROR7);
   		buffer = sizeof(ERROR7);
   	}
   	puts(reply);
   	for (i = 0; i < buffer; i++) {
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
    		case 1:
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
    		    	error_code = 1;
    		    	write_error();
    		    }
    		    else {
    		    	fprintf(stdout, "File found\n");
    		    	block_code = 1;
    		    	write_reply();
    		    }
    		    sendto(sockfd, reply, buffer+4, 0, 
			    	  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break;
    		case 2:
    			//Function to test case 3.
    			/*	
    			reply[0] = (4 >> 8) & 0xff;
   		    	reply[1] = 4 & 0xff;
			   	reply[2] = (0 >> 8) &0xff;
			   	reply[3] = 0 &0xff;
			   	sendto(sockfd, reply, 4, 0, 
    		    	  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break; */

    			error_code = 2;
    			write_error();
				sendto(sockfd, reply, buffer+4, 0, 
    		    	  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break;
    		case 3:
    			error_code = 4;
    			write_error();
    			sendto(sockfd, reply, buffer+4, 0, 
					  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break;
    		case 4:
    			block_reply = (message[2] << 8) + message[3];
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
    			sendto(sockfd, reply, buffer+4, 0, 
		    		  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break;
    		case 5:
    			//recieved_error = &message[4];
    			fprintf(stdout, "Error code %d: %s\n", message[3], &message[4]);
    		break;
    	}
		
        /* Close the connection. */
        //shutdown(connfd, SHUT_RDWR);
        //close(connfd);
	}
	return 0;
}