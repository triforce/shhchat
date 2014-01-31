/*
shhchat client
alpha
*/

#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdint.h>
#define default_port 9956 
#define BUFFER_MAX 256

int sockfd;
int n,x;
struct sockaddr_in serv_addr;
char buffer[BUFFER_MAX];
char buf[10];
void *interrupt_Handler();
void *chat_write(int);
void *chat_read(int);
void *zzz();
void *xor_encrypt(char *key, char *string, int n);
char plain[] = "";
char key[] = "abcdefg";
int y,count;

int main (int argc, char *argv[]) {
    pthread_t thr1,thr2;
    unsigned short port = default_port;

    if (argc < 2) {
        printf("Usage: ./client <server_ip> <optional_port>\n");
        exit(0);
    }
    if (argc == 3) {
        port = atoi(argv[2]);
    }

    //TODO Read key from file
    printf("Reading contents of config file");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        printf ("Failed to open socket - Is the port already in use?\n");
    else
        printf("Socket opened.\n");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    bzero(buf,10);
    printf("\nChoose username for this chat session: ");
    fgets(buf,10,stdin);
    __fpurge(stdin);
    buf[strlen(buf)-1]=':';

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))==-1) {
        printf("Failed to connect - Is the server running?\n");
        exit(0);
    }
    else
        printf("%s connected.\n",buf);

    send(sockfd,buf,strlen(buf),0);
    // intptr_t fixes int return of different size 
    pthread_create(&thr2,NULL,(void *)chat_write,(void *)(intptr_t)sockfd);
    pthread_create(&thr1,NULL,(void *)chat_read,(void *)(intptr_t)sockfd);

    pthread_join(thr2,NULL);
    pthread_join(thr1,NULL);
    return 0;
}

// Read data from socket, updating chat
void *chat_read (int sockfd) {
    if (signal(SIGINT,(void *)interrupt_Handler)==0)
        if (signal(SIGTSTP, (void *)zzz)==0)
            while(1) {
                n=recv(sockfd,buffer,BUFFER_MAX-1,0);
                if (n==0) {
                    printf("\nLost connection to the server.\n\n");
                    exit(0);
                }
                if (n>0) {
		    // Decrypt message
		    y = strlen(buffer);
		    printf("\nMessage from server pre xor - %s",buffer);
		    xor_encrypt(key, buffer, y);
		    printf("\nMessage from server post xor - %s",buffer);
		    if (strncmp(buffer,"shutdown",8)==0) {
	                exit(0);
		    }

		    printf("\nReceived data from server - %s", buffer);
                    bzero(buffer,BUFFER_MAX);
                }
            }
}

void *chat_write (int sockfd) {
    while(1) {
	// Continuosly read from buffer
        fgets(buffer,BUFFER_MAX-1,stdin);
        if (strlen(buffer)-1>sizeof(buffer)) {
            printf("Buffer full, please do not exceed %d characters.\n",sizeof(buffer));
            bzero(buffer,BUFFER_MAX);
            __fpurge(stdin);
        }
	// Encrypt message
        y = strlen(buffer);
        printf("\nSending data to server while connected pre xor - %s", buffer);
	y = strlen(buffer);
        xor_encrypt(key, buffer, y);
	printf("\nSending data to server while connected post xor - %s",buffer);
        n=send(sockfd,buffer,strlen(buffer),0);

        if (strncmp(buffer,"quit",4)==0) {
            exit(0);
	}

        bzero(buffer,BUFFER_MAX);
    }
}

void *interrupt_Handler () {
    printf("\rType 'quit' to exit.\n");
}
void *zzz () {
    printf("\rType 'quit' to exit.\n");
}

// Encryption routine
void *xor_encrypt (char *key, char *string, int n) {
    int i;
    int keyLength = strlen(key);
    for (i = 0 ; i < n ; i++) {
        string[i]=string[i]^key[i%keyLength];
    }
}

