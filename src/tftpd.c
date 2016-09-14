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

typedef enum {
	UNKNOWN=0,
	FILE_NOT_FOUND,
	ACCESS_VIOLATION,
	DISK_FULL,
	ILLEGAL_OPERATION,
	UNKNOWN_TRANSFER_ID,
	FILE_EXISTS,
	NO_SUCH_USER
} ErrorCode;

const char * const err_msg[] = {
	"Unknown error.",
	"File not found.",
	"Access violation.",
	"Disk full or allocation exceeded.",
	"Illegal TFTP operation.",
	"Unknown transfer ID.",
	"File already exists.",
	"No such user."
};

struct sockaddr_in server, client;
char reply[516];
unsigned int sockfd, buffer_size, iterator;
int datablock;
unsigned long block_code;
FILE *data;

//Write a data package to client.
void send_data_packet() {
	memset(reply, 0, sizeof(reply)); // Clean reply
	buffer_size = 0;
	//Add operation code 3
	reply[0] = (3 >> 8) & 0xff;
	reply[1] = 3 & 0xff;
	//Add plock code
	reply[2] = (block_code >> 8) &0xff;
	reply[3] = block_code &0xff;
	while (buffer_size < 512) {
		if ((datablock = fgetc(data)) == EOF) {//Check if file has reached the end.
			break;
		}
		reply[buffer_size+4] = (char)datablock;
		buffer_size++;
	}
	//puts(reply);
	fprintf(stdout, "buffer size = %d\n", buffer_size);
	fprintf(stdout, "Size of reply: %d\n", buffer_size+4);

	sendto(sockfd, reply, buffer_size+4, 0, (struct sockaddr *) &client, (socklen_t) sizeof(client));
	if (buffer_size < 512) {
		fprintf(stdout, "Last package. Closing stream.\n");
		fclose(data);
		data = NULL;
	}
}

//Send an error back to client.
void send_error_response(ErrorCode error_code) {

	memset(reply, 0, sizeof(reply));
	//Add operation code 5
	reply[0] = (5 >> 8) & 0xff;
	reply[1] = 5 & 0xff;
	// Add error code.
	reply[2] = (error_code >> 8) & 0xff;
	reply[3] = error_code & 0xff;
	//Add appropriate string.

	strcpy(&reply[4], err_msg[error_code]);

	printf("Sending ERROR message: \"%s\"\n", err_msg[error_code]);

	// 4 bytes + string + '\0'
	sendto(sockfd, reply, 4+strlen(err_msg[error_code])+1, 0, (struct sockaddr *) &client, (socklen_t) sizeof(client));

	if (data != NULL) { //Close datastream, if file is open.
		fclose(data);
		data = NULL;
	}
}

// Check if string contains a certain prefix.
bool is_prefix(const char *prefix, const char *string) {
    return strncmp(prefix, string, strlen(prefix)) == 0;
}

// Signal handler function.
void sig_handler(int signal_n) {
	if (signal_n == SIGINT) {
		printf("\nShutting down...\n");
	}

	/* Close the connection. */
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
	exit(0);
}

// change all occurances "\r\n" to "\r\0\n"
void convert_to_nvt_ascii(FILE *source, FILE *destination) {
	int input_char;
	printf("CONVERTING FILE TO NETASCII\n");
	while ((input_char = fgetc(source)) != EOF) {
		if (input_char == '\r') {
			fputc('\r', destination);
			fputc('\0', destination);
		}
		else if (input_char == '\n'){// EOF occurs
			fputc('\r', destination);
			fputc('\n', destination);
		}
		else {
			fputc(input_char, destination);
		}
	}
}

//Main function
int main(int argc, char *argv[]) {
	char message[516], stream_path[255], shared_directory_path [PATH_MAX+1], absolute_file_path [PATH_MAX+1];
	char *ptr, *mode, *filename;
	unsigned int op_code, mode_pointer;
	unsigned long recieved_block_code;

	// checking number of arguments
	if (argc != 3) {
		printf("Usage: %s <port> <folder_name>\n", argv[0]);
		return 1;
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("\nCannot catch SIGINT!\n");

	int port_number = strtol(argv[1], NULL, 10);
	char *shared_directory = argv[2];

	/* Create and bind a UDP socket */
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
	strncat(shared_directory_path, "/", 1);
	printf("shared_directory_path: %s\n", shared_directory_path);

	while(1) {

		printf("\nWaiting...\n");

		/* We first have to accept a UDP connection, connfd is a fresh
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
				//Flush reply
				memset(reply, 0, sizeof(reply));

				//Extract filename from message.
				filename = (char*)&message[2];
				//find mode
				for (mode_pointer = 2; message[mode_pointer] != '\0'; mode_pointer++) {}
				mode_pointer++;
				mode = &message[mode_pointer];

				//path to stream
				memset(stream_path, 0, sizeof(stream_path));
				strncpy(stream_path, shared_directory, strlen(shared_directory));
				strncat(stream_path, "/", 1);
				// -2 because \0 is not included and "/" from previous line
				strncat(stream_path, filename, sizeof(stream_path)-strlen(shared_directory)-2);

				// checking if req. file is inside shared directory
				printf("Filename is: %s\n", filename);
				printf("Mode is %s\n", mode);

				ptr = realpath(stream_path, absolute_file_path);
				if (ptr == NULL) { //File not found. Write error message.
					perror("Error");
					send_error_response(FILE_NOT_FOUND);
					break;
				}
				else if (!is_prefix(shared_directory_path, absolute_file_path)) {
					send_error_response(FILE_NOT_FOUND);
					printf("File is outside shared directory.\n[[ Access forbidden ]]\n");
					break;
				}
				else
					printf("Path is %s\n", absolute_file_path);

				//Open stream path.
				data = fopen(stream_path, "r");

				char *p = mode;
				for ( ; *p; ++p) *p = tolower(*p); // change mode string to lowercase, just in case

				// convert file into netascii format (NVT)
				// http://www.hw-group.com/support/nvt/index_en.html
				if (strcmp(mode, "netascii") == 0) {
					char temp_file_name[] = "/tmp/fileXXXXXX";
					int fd = mkstemp(temp_file_name);
					close(fd);
					FILE *temp = fopen(temp_file_name, "w+"); // empty file

					convert_to_nvt_ascii(data, temp);
					fseek(temp, 0, SEEK_SET);

					fclose(data); // closing original file
					data = temp; // changing original file to temporary (with edited EOL)
				}

				// Begin sending data.
				block_code = 1;
				send_data_packet();
				break;

			case WRQ:
				// Write Requests forbidden. Write error message.
				send_error_response(ACCESS_VIOLATION);
				break;

			case DATA:
				// Illegal FTTP operation. Write error message.
				send_error_response(ILLEGAL_OPERATION);
				break;

			case ACK:					
				if (reply[1] == 5) { //Check if last reply was an error message.
					fprintf(stdout, "Sent error message was recieved. Goodbye.\n");
					memset(reply, 0, sizeof(reply));
					break;
				}
				//Fetch block code from message
				recieved_block_code = (((unsigned char*)message)[2] << 8) + ((unsigned char*)message)[3];
				fprintf(stdout, "Blockode reply = %lu\n", recieved_block_code);

				if (block_code == recieved_block_code) { //Check if last package was delivered.
					fprintf(stdout, "Data recieved\n");
					fflush(stdout);
					if (data == NULL) {	//Filestream has been closed.
						fprintf(stdout, "File delivered. Goodbye.\n");
					}
					else { //Send next package.
						block_code++;
						fprintf(stdout, "Blockode to be sent is: %lu\n", block_code);
						send_data_packet();
					}
				}
				else { //Block code mismatch. Resend last package.
					fprintf(stdout, "Resend last package\n");
					sendto(sockfd, reply, buffer_size+4, 0,
						  (struct sockaddr *) &client, (socklen_t) sizeof(client));
				}
				break;

			case ERROR:
				fprintf(stdout, "Error code %d: %s\n", message[3], &message[4]);
				if (data != NULL) { //Close datastream, if file is open.
					fclose(data);
					data = NULL;
				}
				break;
		}
	}
	return 0;
}
