/*
shhchat server
alpha
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#define BACKLOG 100
#define BUFFER_MAX 256
#define default_port 9956

struct client {
   int port;
   char username[10];
   struct client *next;
};
typedef struct client *ptrtoclient;
typedef ptrtoclient clients;
typedef ptrtoclient addr;
void disconnectAllClients();
clients clearClientList(clients h);
void removeClient(int port, clients h);
void addClient(int port,char*,clients h,addr a);
void removeAllClients(clients h);
void displayConnected(const clients h);
void *closeServer();
void *server(void * arg);
void showConnected();
void xor_encrypt(char *key, char *string, int n);
void printDebug(char *string);
char username[10];
int sf2,n,count;
clients h;
char buffer[BUFFER_MAX];
bool debugsOn = true;
char plain[] = "Hello";
char key[] = "abcdefg";

int main (int argc, char *argv[]) {
    int socket_fd,new_fd;
    int portnum;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int cli_size,z;
    pthread_t thr;
    int yes=1;
    addr a;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    bool haveKey = false;

    if (argc == 2)
        portnum = atoi(argv[1]);
    else
        portnum = default_port;

    printf("Reading contents of config file.\n");

    // Try and open key file
    fp = fopen("cfg/key", "r");
    if (fp == NULL) {
        printf("Failed to read key file.\n");
        exit(EXIT_FAILURE);
    }

    // Read contents of key file
    while ((read = getline(&line, &len, fp)) != -1) {
        // printf("Key found %zu :\n", read);
        printf("Key found.");
        // printf("%s", line);
        haveKey = true;
        break;
    }
    free(line);

    if (!haveKey) {
        printf("Failed to read key file.\n");
        exit(EXIT_FAILURE);
     }

    printf("Testing encryption key on string: '%s'\n", plain);
    n = strlen(plain);
    xor_encrypt(key,plain , n);
    printf("Encrypting...%s\n", plain);

    n = strlen(plain);
    xor_encrypt(key,plain , n);
    printf("Decrypted '%s'\nIf the two do no match then your encryption hasn't worked.\n", plain);

    // TODO As the server should act as a daemon it will write to syslog for all future logging
    openlog("shhchatd", 0, LOG_USER);
    syslog(LOG_INFO, "%s", "Server started successfully");

    // Clear out list of clients
    h = clearClientList(NULL);

    // Open socket and listen for connections 
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(portnum);
    printf("IP Address: %s\n",inet_ntoa(server_addr.sin_addr));

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        syslog(LOG_INFO, "%s", "Socket error, closing...");
        exit(1);
    } else
        syslog(LOG_INFO, "%s", "Socket created.");
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
        printf("Socket error.\n");
        exit(1);
    }

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))==-1) {
        printf("Socket bind failed.\n");
        exit(1);
    }
    else
        printf("Port binded successfully.\n\n");

    listen(socket_fd, BACKLOG);
    printf("Now accepting client connections.\n");
    if (signal(SIGINT,(void *)closeServer)==0)
        if (signal(SIGTSTP, showConnected)==0)
            while(1) {
                cli_size=sizeof(struct sockaddr_in);
                new_fd = accept(socket_fd, (struct sockaddr *)&client_addr,&cli_size);
                a =h ;
                bzero(username,10);
                if (recv(new_fd,username,sizeof(username),0)>0);
                username[strlen(username)-1]=':';
                printf("\t%d->%s connected \n",new_fd,username);
                sprintf(buffer,"%s is currently connected\n",username);
                addClient(new_fd,username, h, a);
                a = a->next;
                a = h ;
                do {
                    a = a->next;
                    sf2 = a->port;
                    if(sf2!=new_fd) {
			printf("\nSending who has currently connected data to all clients pre xor %s", buffer);
			 n = sizeof(buffer);
	                xor_encrypt(key, buffer, n);
			printf("\nSending who has currently connected data to all clients post xor %s", buffer);
                        send(sf2,buffer ,sizeof(buffer),0);
			}
                } while (a->next != NULL);
                printf("Connection made from %s & %d\n\n",inet_ntoa(client_addr.sin_addr),new_fd);
                struct client args;
                args.port=new_fd;
                strcpy(args.username,username);
                pthread_create(&thr,NULL,server,(void*)&args);
                pthread_detach(thr);
            }
    removeAllClients(h);
    close(socket_fd);
}

// Server side handling for each connected client 
void *server(void * arguments) {
    struct client *args=arguments;
    char buffer[BUFFER_MAX],ubuf[50],uname[10];
    char *strp;
    char *msg = (char *) malloc(BUFFER_MAX);
    int ts_fd,x,y;
    int sfd,msglen;
    ts_fd = args->port;
    strcpy(uname,args->username);
    addr a;

    a =h ;
    do {
        a = a->next;
        sprintf(ubuf," %s is online.\n",a->username);
		printf("\nSending data to client contents of ubuf pre xor- %s", ubuf);
		n = strlen(ubuf);
        xor_encrypt(key, ubuf, n);
		printf("\nSending data to client contents of ubuf post xor %s", ubuf);
        send(ts_fd,ubuf,strlen(ubuf),0);
    } while(a->next != NULL);

    while(1) {
        bzero(buffer,256);
        y=recv(ts_fd,buffer,BUFFER_MAX,0);
        if (y==0)
            goto cli_dis;

	// Clear
	buffer[y] = '\0';

	n = strlen(buffer);
	// TODO Some characters are lost from the end of the message received, not being cleared somewhere?
	printf("\nData received into server pre xor - %s", buffer);
 	xor_encrypt(key, buffer, n);
	printf("\nData received into server post xor -%s", buffer);

        if (strncmp(buffer, "quit", 4) == 0) {
cli_dis:
            printf("%d ->%s disconnected\n",ts_fd,uname);
            sprintf(buffer,"%s has disconnected\n",uname);
            addr a = h ;
            do {
                a = a->next;
                sfd = a->port;
                if(sfd == ts_fd)
                    removeClient(sfd, h);
                if(sfd != ts_fd) {
	 	    // Encrypt message
		    printf("\nSending data from server pre xor- %s", buffer);
		    n = strlen(buffer);
                    xor_encrypt(key, buffer, n);
		    printf("\nSending data from server post xor- %s", buffer);
                    send(sfd,buffer,BUFFER_MAX,0);
		}
            } while (a->next != NULL);
            displayConnected(h);
            close(ts_fd);
            free(msg);
            break;
        }
        printf("\nData in buffer after disconnect check: %s %s\n",uname,buffer);
        strcpy(msg,uname);
        x=strlen(msg);
        strp = msg;
        strp+= x;
        strcat(strp,buffer);
        msglen=strlen(msg);
        addr a = h ;
        do {
            a = a->next;
            sfd = a->port;
            if(sfd != ts_fd) {
		// Handles sending client messages to all connected clients
		printf("\nSending data to client contents of msg pre xor - %s", msg);
		n = strlen(msg);
		xor_encrypt(key, msg, n);
		printf("\nSending data to client contents of msg post xor - %s", msg);
                send(sfd,msg,msglen,0);
	     }
        } while(a->next != NULL);
        displayConnected(h);
        bzero(msg,BUFFER_MAX);
    }
    return 0;
}

// TODO all debug type messages logged using this
void printDebug (char *string) {
    if (debugsOn)
        printf(string);
}

clients clearClientList (clients h) {
    if(h != NULL)
        removeAllClients(h);
    h = malloc(sizeof(struct client));
    if(h == NULL)
        printf("Out of memory.");
    h->next = NULL;
    return h;
}

void removeAllClients (clients h) {
    addr a, Tmp;
    a = h->next;
    h->next = NULL;
    while(a != NULL) {
        Tmp = a->next;
        free(a);
        a = Tmp;
    }
}

void addClient (int port,char *username, clients h, addr a) {
    addr TmpCell;
    TmpCell = malloc(sizeof(struct client));
    if (TmpCell == NULL)
        printf("Cannot accept anymore connections.");
    TmpCell->port = port;
    strcpy(TmpCell->username,username);
    TmpCell->next = a->next;
    a->next = TmpCell;
}

void displayConnected (const clients h) {
    addr a =h ;
    if (h->next == NULL)
        printf("No clients currently connected.\n");
    else {
        do {
            a = a->next;
            printf( "%s \t",a->username);
        } while(a->next != NULL);
        printf( "\n" );
    }
}

void removeClient (int port, clients h) {
    addr a, TmpCell;
    a = h;
    while(a->next != NULL && a->next->port != port)
        a = a->next;
    if (a->next != NULL) {
        TmpCell = a->next;
        a->next = TmpCell->next;
        free(TmpCell);
    }
}

void *closeServer() {
    printf("\nServer shutting down.\n");
    disconnectAllClients();
    exit(0);
}

void disconnectAllClients () {
    int sfd;
    addr a = h ;
    int i=0;
    if( h->next == NULL ) {
        printf("Closing as no clients connected.\n\n");
        exit(0);
    } else {
        do {
            i++;
            a = a->next;
            sfd = a->port;
	    char shutdown[] = "shutdown";
 	    n = sizeof(shutdown);
	    xor_encrypt(key, shutdown, n);
            send(sfd,shutdown,13,0);
        } while( a->next != NULL );
        printf("%d clients closed.\n\n",i);
    }
}

void showConnected () {
    printf("\rConnected clients:\n\n");
    displayConnected(h);
}

// Encryption routine
void xor_encrypt (char *key, char *string, int n) {
    int i;
    int keyLength = strlen(key);
    for(i = 0 ; i < n ; i++) {
        string[i]=string[i]^key[i%keyLength];
    }
}
