#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <string.h>
#include <semaphore.h>
#include "list.h"

// global vars
List * messages_received;
List * messages_toSend;

int sockfd_toSend;
int sockfd_toReceive;
int isOnline;
int key;
int exitStatus;
int sent;
int received;

sem_t s_send;
sem_t s_receive;

pthread_t keyboard_tid; 
pthread_t sender_tid; 
pthread_t receiver_tid; 
pthread_t output_tid; 

struct sockaddr_in server_addr_recv;
struct sockaddr_in server_addr_sendTo;

// Encryption
void encrypt(char * message) {
	int len = strlen(message);
	for (int i = 0; i < len; i++) {
		message[i] += key;
	}
}

void decrypt(char * message) {
	int len = strlen(message);
	for (int i = 0; i < len; i++) {
		message[i] -= key;
	}
}
	
// thread routines
void * keyboard_input(void * args) {
	char line[5000];
	while(1) {
		fgets(line, 5000, stdin);
		List_prepend(messages_toSend, strdup(line));
		//printf("list curr is = %s\n", (char*)List_curr(messages_toSend));
		sem_post(&s_send);		
	}
}

void * sender(void * args) {
	char * message = NULL;
	int toSendOrNotToSend;
	int sendRet;
	
	while(1) {
		// wait for a message to send
		sem_wait(&s_send);
		toSendOrNotToSend = 1;
		message = List_trim(messages_toSend);
		
		if (strcmp(message,"!status\n") == 0 ) {
			toSendOrNotToSend = 0;
			// if isOnline is already 1, print online
			if (isOnline == 1) {
				printf("Online\n");
			}
			// otherwise check if other machine is online
			else {
				encrypt(message);
				sendRet = sendto(sockfd_toSend, message, strlen(message), 0, (struct sockaddr *)&server_addr_sendTo, sizeof(server_addr_sendTo));
				if (sendRet < 0) {
					printf("Error sending message for !status.\n");
				}
				// give a second for the other machine to respond
				sleep(1);
				// if no response after 1 second print Offline
				if ( isOnline == 0) {
					printf("Offline\n");
				}
			}
		}
			
		// if message is !exit, set exitStatus to 1
		if (strcmp(message,"!exit\n") == 0 ) {
			exitStatus = 1;
		}	

		// if message, encypt then send
		if (toSendOrNotToSend == 1) {
			encrypt(message);
			sendRet = sendto(sockfd_toSend, message, strlen(message), 0, (struct sockaddr *)&server_addr_sendTo, sizeof(server_addr_sendTo));
			if (sendRet < 0) {
				printf("Error sending message.\n");
			}
		}

		free(message);
		
		// if exitStatus is 1, exit the thread
		if (exitStatus == 1) {
			pthread_exit(0);
		}
	}
}

void * receiver(void * args) {

	char message[5000];
	struct pollfd fds[1];
	//no timeout for poll
	int timeout = -1;
	
	while(1) {		
		fds[0].fd = sockfd_toReceive;
		fds[0].events = 0;
		fds[0].events |= POLLIN;
		poll(fds, 1, timeout);
		recvfrom(sockfd_toReceive, &message, sizeof(message), 0 , (struct sockaddr *)&server_addr_recv, (socklen_t*)sizeof(server_addr_recv));
		// As soon we receive a message we know it is online
		isOnline = 1;
		decrypt(message);
		//printf("received = %s\n", message);
		if (strcmp(message, "!status\n") == 0) {
			List_prepend(messages_toSend, strdup("!online\n"));
			sem_post(&s_send);
		}
		else if (strcmp(message, "!online\n") == 0) {
			isOnline = 1;
			printf("Online\n");
		}
		else if (strcmp(message, "!exit\n") == 0) {
			List_prepend(messages_toSend, strdup("!exit\n"));
			sem_post(&s_send);
		}
		else {
			List_prepend(messages_received, strdup(message));
			sem_post(&s_receive);
			
		}
		// reset message array
		memset(message, 0, 5000);
	}
}

void * output(void * args) {

	char * message = NULL;
	while(1) {
		sem_wait(&s_receive);
		message = List_trim(messages_received);
		printf("%s", message);
		fflush(stdout);
		free(message);
	}
}

int main(int argc, char *argv[]) {

	isOnline = 0;
	key = 1;
	exitStatus = 0;
	
	if (argc < 3) {
		printf("Usage:\n ./lets-talk <local port> <remote host> <remote port>\n");
	}
	
	else {
		printf("Welcome to to LetS-Talk! Please type your messages now.\n");
	}
	
	char * local_port = argv[1];
	char * remote_host = argv[2];
	char * remote_port = argv[3];
	 
	if ((sockfd_toSend = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket to send creation failed\n");
		exit(EXIT_FAILURE);
	}
	
	if ((sockfd_toReceive = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket to receive creation failed\n");
		exit(EXIT_FAILURE);
	}
	
	server_addr_recv.sin_family = AF_INET;
	server_addr_recv.sin_port = htons(atoi(remote_port));
	server_addr_recv.sin_addr.s_addr = inet_addr(remote_host);
	
	server_addr_sendTo.sin_family = AF_INET;
	server_addr_sendTo.sin_port = htons(atoi(local_port));
	server_addr_sendTo.sin_addr.s_addr = inet_addr(remote_host);

	if (bind(sockfd_toReceive, (struct sockaddr *)&server_addr_recv, sizeof(server_addr_recv)) < 0 ) {
		printf("socket bind failed for recv\n");
	}
	
	messages_received = List_create();
	messages_toSend = List_create();
	
	// initialize semaphores
	sem_init(&s_send, 0, 0);
	sem_init(&s_receive, 0, 0);
	
	// Create threads
	pthread_create(&receiver_tid, NULL, receiver, NULL);
	pthread_create(&output_tid, NULL, output, NULL);
	pthread_create(&keyboard_tid, NULL, keyboard_input, NULL);
	pthread_create(&sender_tid, NULL, sender, NULL);
	
	// wait for sender thread to terminate 
	pthread_join(sender_tid, NULL);
	pthread_detach(sender_tid);
	// cancel remaining threads
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_cancel(keyboard_tid);
	pthread_detach(keyboard_tid);
	pthread_cancel(receiver_tid);
	pthread_detach(receiver_tid);
	pthread_cancel(output_tid);
	pthread_detach(output_tid);
	
	// destroy semaphores
	sem_destroy(&s_send);
	sem_destroy(&s_receive);
	
	// close sockets
	close(sockfd_toSend);
	close(sockfd_toReceive);
	
	// free message lists
	List_free(messages_received, free);
	List_free(messages_toSend, free);	
}
