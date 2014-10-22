/*
shhchat client
alpha
*/

#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <stdbool.h>
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
#define BUFFER_MAX 1024

// Colours
#define RED  "\033[22;31m"
#define RESET_COLOR "\e[m"

int sockfd;
int n,x;
struct sockaddr_in serv_addr;
char buffer[BUFFER_MAX];
char buf[10];
void *interrupt_Handler();
void *chat_write(int);
void *chat_read(int);
void *zzz();
void printDebug(char *string);
void writeLog(FILE *fp, char *str);
void addYou();
void *xor_encrypt(char *key, char *string, int n);
bool debugsOn = false;
char plain[] = "";
char key[] = "123456";
int y,count;
FILE *fp_l;

int main (int argc, char *argv[]) {
    pthread_t thr1,thr2;
    unsigned short port = default_port;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    bool haveKey = false;

    if (argc < 2) {
        printf("Usage: ./client <server_ip> <optional_port>\n");
        exit(0);
    }
    if (argc == 3) {
        port = atoi(argv[2]);
    }

    // Open log file
    fp_l = fopen("shh_log", "a");
    if (fp_l == NULL)
    {
        printf("Couldn't open log file\n");
        return 1;
    }

    printDebug("Reading contents of config file\n");

    // Try and open key file
    fp = fopen("cfg/key", "r");
    if (fp == NULL) {
        printDebug("Failed to read key file\n");
        exit(EXIT_FAILURE);
    }


    // Read contents of key file
    while ((read = getline(&line, &len, fp)) != -1) {
        // printf("Key found %zu :\n", read);
        printDebug("Key found\n");
        strncpy(key, line, sizeof(line));
	// Don't print key out!
	// printf("%s", key);
        haveKey = true;
        break;
    }
    free(line);

    if (!haveKey) {
        printDebug("Failed to read key file\n");
        exit(EXIT_FAILURE);
     }

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
    printf("Choose username for this chat session: ");
    fgets(buf,10,stdin);
    __fpurge(stdin);
    buf[strlen(buf)-1]=':';

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))==-1) {
        printf("Failed to connect - Is the server running?\n");
        exit(0);
    }
    else
        printf("%s connected.\n",buf);
	addYou();

    y = strlen(buf);
    xor_encrypt(key, buf, y);
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
                bzero(buffer,BUFFER_MAX);
		fflush(stdout);
                n=recv(sockfd,buffer,sizeof(buffer),0);
                if (n==0) {
                    printf("Lost connection to the server.\n\n");
                    exit(0);
                }
                if (n>0) {
		    // Decrypt message
		    y = strlen(buffer);
		    xor_encrypt(key, buffer, y);
		    if (strncmp(buffer,"shutdown",8)==0) {
			printf("\n");
	                exit(0);
		    }
		    printf("\n: %s \n", buffer);
		    addYou();
                }
            }
}

void *chat_write (int sockfd) {
    while(1) {
	// Continuosly read from stdin
        bzero(buffer,BUFFER_MAX);
        __fpurge(stdin);
	fgets(buffer,sizeof(buffer),stdin);
        if (strlen(buffer)-1>sizeof(buffer)) {
            printf("Buffer full, reduce size of message.\n");
            bzero(buffer,BUFFER_MAX);
            __fpurge(stdin);
        }
	if (strlen(buffer)>1) {
	    // Encrypt message
            y = strlen(buffer);
            xor_encrypt(key, buffer, y);
            n=send(sockfd,buffer,y,0);

            if (strncmp(buffer,"quit",4)==0) {
                exit(0);
	    }
	}
	else
	    __fpurge(stdin);
        bzero(buffer,BUFFER_MAX);
	addYou();
	
    }
}

void *interrupt_Handler () {
    printf("\rType 'quit' to exit.\n");
}
void *zzz () {
    printf("\rType 'quit' to exit.\n");
}

void printDebug (char *string) {
// If debugs not on, write to log file
   if (debugsOn)
	printf(string);
   else
	writeLog(fp_l,string);
}

void writeLog(FILE *fp, char *str) {
    fprintf(fp, "%s", str);
}

void addYou() {
    printf("You: ");
}

// Encryption routine
void *xor_encrypt (char *key, char *string, int n) {
    int i;
    int keyLength = strlen(key);
    for (i = 0 ; i < n ; i++) {
        string[i]=string[i]^key[i%keyLength];
    }
}

