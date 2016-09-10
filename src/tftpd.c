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

int main(int argc, char *argv[])
{
	unsigned int sockfd, op_code, mode_pointer, buffer;
	struct sockaddr_in server, client;
	char message[516], reply[516];
	char stream_path[100], error[100];
	char* filename;
	char* mode;
	long datablock;
	unsigned long block_code, block_reply;
	int i;
	unsigned int port_number = strtol(argv[1], NULL, 10);
	FILE *data;

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
    

	for(;;) {

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
    		    	//fprintf(stdout, "File not found\n");
    		    	reply[0] = (5 >> 8) & 0xff;
    		    	reply[1] = 5 & 0xff;
				   	reply[2] = (1 >> 8) &0xff;
				   	reply[3] = 1 &0xff;
				   	memset(error, 0, strlen(error));
				   	strcpy(error, "File not found\0");
    		    	for (i = 0; (error[i+1] =!'\0'); i++) {
    		    		fprintf(stdout, "%c",error[i]);
    		    		reply[4+i] = error[i];
    		    		buffer++;
					}
					fprintf(stdout, "\n");
    		    }
    		    else {
    		    	fprintf(stdout, "File found\n");
    		    	block_code = 1;
    		    	buffer = 0;
					reply[0] = (3 >> 8) & 0xff;
				   	reply[1] = 3 & 0xff;
				   	reply[2] = (block_code >> 8) &0xff;
				   	reply[3] = block_code &0xff;
				   	while (((datablock = fgetc(data)) != EOF) && (buffer < 512)) {
				   		reply[buffer+4] = datablock & 0xff;
				        buffer++;
				   	}
				   	puts(reply);
    		    }
    		    fprintf(stdout, "buffer = %d\n", buffer);
			   	fprintf(stdout, "Size of reply: %lu\n", sizeof(reply));
    		    sendto(sockfd, reply, buffer+4, 0, 
    		    	  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break;
    		case 2:
    		//Function to test case 3.
    		/*	reply[0] = (4 >> 8) & 0xff;
   		    	reply[1] = 4 & 0xff;
			   	reply[2] = (0 >> 8) &0xff;
			   	reply[3] = 0 &0xff;
			   	sendto(sockfd, reply, 0+4, 0, 
    		    	  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break;
    		*/
    			reply[0] = (5 >> 8) & 0xff;
   		    	reply[1] = 5 & 0xff;
			   	reply[2] = (2 >> 8) &0xff;
			   	reply[3] = 2 &0xff;
			   	memset(error, 0, strlen(error));
				strcpy(error, "Access Violation. Uploading not allowed.\0");
   		    	for (i = 0; (error[i+1] =!'\0'); i++) {
   		    		fprintf(stdout, "%c",error[i]);
   		    		reply[4+i] = error[i];
   		    		buffer++;
				}
				fprintf(stdout, "\n");
				sendto(sockfd, reply, buffer+4, 0, 
    		    	  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break;
    		case 3:
    			reply[0] = (5 >> 8) & 0xff;
   		    	reply[1] = 5 & 0xff;
			   	reply[2] = (2 >> 8) &0xff;
			   	reply[3] = 2 &0xff;
			   	memset(error, 0, strlen(error));
			  	strcpy(error, "Illegal TFTP operation. Uploading not allowed\0");
   		    	for (i = 0; (error[i+1] =!'\0'); i++) {
   		    		fprintf(stdout, "%c",error[i]);
   		    		reply[4+i] = error[i];
   		    		buffer++;
				}
				fprintf(stdout, "\n");
				sendto(sockfd, reply, buffer+4, 0, 
    		    	  (struct sockaddr *) &client, (socklen_t) sizeof(client));
    		break;
    		case 4:
    			block_reply = (message[2] << 8) + message[3];
    			fprintf(stdout, "Blockode reply = %lu\n", block_reply);
       			if (block_code == block_reply) {
    				fprintf(stdout, "Data recieved\n");
    				fflush(stdout);
    				memset(reply, 0, strlen(reply));
    		    
    				block_code++;

    		    	fprintf(stdout, "Blockode to be sent is: %lu\n", block_code);
    		    	buffer = 0;
					reply[0] = (3 >> 8) & 0xff;
					reply[1] = 3 & 0xff;
				   	reply[2] = (block_code >> 8) &0xff;
				   	reply[3] = block_code &0xff;
				   	if (data == NULL) {
				   	}
				   	else {
					   	while (((datablock = fgetc(data)) != EOF) && (buffer < 512)) {
					   		reply[buffer+4] = datablock & 0xff;
					        buffer++;
					   	}
					   	puts(reply);
					   	fprintf(stdout, "buffer = %d\n", buffer);
	    		    	fprintf(stdout, "Size of reply: %lu\n", sizeof(reply));
	    		    }
    		    	sendto(sockfd, reply, buffer+4, 0, 
    		    		(struct sockaddr *) &client, (socklen_t) sizeof(client));
    			}
    			else {
    				sleep(1);
    				fprintf(stdout, "Resend last package\n");
    				sendto(sockfd, reply, buffer+4, 0, 
    		    		(struct sockaddr *) &client, (socklen_t) sizeof(client));
    			}
    		break;
    		case 5:
    			fprintf(stdout, "Error code %d: %d\n", message[4], message[5]);
    		break;
    	}
		
        /* Close the connection. */
        //shutdown(connfd, SHUT_RDWR);
        //close(connfd);
	}

	return 0;
}