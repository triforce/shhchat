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
#include <signal.h>
#include <sys/stat.h>

#define BACKLOG 100
#define BUFFER_MAX 1024
#define default_port 9956

// Colours
#define RED  "\033[22;31m"
#define RESET_COLOR "\e[m"

#ifdef DEBUG
#define debug 1
#else
#define debug 0 
#endif

struct client {
   int port;
   char username[10];
   struct client *next;
};
typedef struct client *ptrtoclient;
typedef ptrtoclient clients;
typedef ptrtoclient addr;
void disconnectAllClients();
clients ClientList(clients h);
void removeClient(int port, clients h);
void addClient(int port,char*,clients h,addr a);
void removeAllClients(clients h);
void displayConnected(const clients h);
void *closeServer();
void *server(void * arg);
void showConnected();
void xor_encrypt(char *key, char *string, int n);
void printDebug(char *string);
bool checkUser (char *string);
char username[10];
int sf2,n,count;
clients h;
char buffer[BUFFER_MAX];
bool debugsOn = false;
char plain[] = "Hello";
char key[] = "123456";

static void start_daemon()
{
    pid_t pid;
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    if (setsid() < 0)
        exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);

    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>0; x--)
    {
        close (x);
    }
   openlog("shhchatd", 0, LOG_USER);
   syslog(LOG_INFO, "%s", "Server started OK.");
}

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

    if (debug == 1)
	debugsOn = true;

    if (debugsOn)
	printDebug("Debugs on, not starting as a daemon.");	
    	// don't start daemon
    else
	start_daemon();

    while (1)
    {

    if (argc == 2)
        portnum = atoi(argv[1]);
    else
        portnum = default_port;

    printDebug("Reading contents of config file.");

    // Try and open key file
    fp = fopen("cfg/key", "r");
    if (fp == NULL) {
        printDebug("Failed to read key file.");
        exit(EXIT_FAILURE);
    }

    // Read contents of key file
    while ((read = getline(&line, &len, fp)) != -1) {
        printDebug("Key found.");
        strncpy(key, line, sizeof(line));
	if (debugsOn)
            printf("%s", key);
	haveKey = true;
        break;
    }
    free(line);

    if (!haveKey) {
        printDebug("Failed to read key file.");
        exit(EXIT_FAILURE);
     }

    if (debugsOn) {    
        printf("Testing encryption key on string: '%s'\n", plain);
        n = strlen(plain);
        xor_encrypt(key,plain , n);
        printf("Encrypting...%s\n", plain);

        n = strlen(plain);
        xor_encrypt(key,plain , n);
        printf("Decrypted '%s'\nIf the two do no match then your encryption hasn't worked.\n", plain);
    }

    openlog("shhchatd", 0, LOG_USER);
    syslog(LOG_INFO, "%s", "Server started successfully");

    // Clear out list of clients
    h = ClientList(NULL);

    // Open socket and listen for connections 
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(portnum);
    
    if (debugsOn)
        printf("IP Address: %s\n",inet_ntoa(server_addr.sin_addr));

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        syslog(LOG_INFO, "%s", "Socket error, closing...");
        exit(1);
    } else
        syslog(LOG_INFO, "%s", "Socket created.");
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
        syslog(LOG_INFO, "%s", "Socket error.");
        exit(1);
    }

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))==-1) {
        syslog(LOG_INFO, "%s", "Failed to bind.");
        exit(1);
    }
    else
        syslog(LOG_INFO, "%s", "Port binded.");

    listen(socket_fd, BACKLOG);
    if (signal(SIGINT,(void *)closeServer)==0)
        if (signal(SIGTSTP, showConnected)==0)
            while(1) {
                cli_size=sizeof(struct sockaddr_in);
                new_fd = accept(socket_fd, (struct sockaddr *)&client_addr,&cli_size);
                a = h;
                bzero(username,10);
		bzero(buffer,BUFFER_MAX);
                if (recv(new_fd,username,sizeof(username),0)>0);
                n = strlen(username);
	        xor_encrypt(key, username, n);

		username[strlen(username)-1]=':';
                sprintf(buffer,"%s connected",username);
                addClient(new_fd,username, h, a);
                a = a->next;
                a = h;
                do {
                    a = a->next;
                    sf2 = a->port;
                    if(sf2!=new_fd) {
			n = sizeof(buffer);
	                xor_encrypt(key, buffer, n);
			send(sf2,buffer ,sizeof(buffer),0);
			}
                } while (a->next != NULL);
		if (debugsOn)
                    printf("Connection made from %s\n\n",inet_ntoa(client_addr.sin_addr));
                struct client args;
                args.port=new_fd;
                strncpy(args.username,username,sizeof(username));
                pthread_create(&thr,NULL,server,(void*)&args);
                pthread_detach(thr);
            }
    removeAllClients(h);
    close(socket_fd);
    }
}

// Server side handling for each connected client 
void *server(void * arguments) {
    struct client *args=arguments;
    char buffer[BUFFER_MAX],ubuf[BUFFER_MAX],uname[10];
    char *strp;
    char *msg = (char *) malloc(BUFFER_MAX);
    int ts_fd,x,y;
    int sfd,msglen;
    ts_fd = args->port;
    strncpy(uname,args->username,sizeof(args->username));
    addr a;
    a = h; 

    if (!checkUser(uname)) {
        syslog(LOG_INFO, "%s", "User does not exist in db.");
	goto cli_dis;
    }

    // Add colon back in, need to check it doesn't go over allocated length
    uname[strlen(uname)]=':';
    uname[strlen(uname)]=' ';
/*
	bzero(ubuf,BUFFER_MAX);
    do {
        a = a->next;
        sprintf(ubuf,"%s is online.",a->username);
	 printf("\nSending data to client contents of ubuf pre xor- %s", ubuf);
	n = strlen(ubuf);
        xor_encrypt(key, ubuf, n);
	 printf("\nSending data to client contents of ubuf post xor %s", ubuf);
	fflush(stdout);
        send(ts_fd,ubuf,n,0);
    } while(a->next != NULL);
*/
    while(1) {
        bzero(buffer,BUFFER_MAX);
        y=recv(ts_fd,buffer,sizeof(buffer),0);
        if (y==0)
            goto cli_dis;

	// Clear
	buffer[y] = '\0';

	n = strlen(buffer);
 	xor_encrypt(key, buffer, n);

        if (strncmp(buffer, "quit", 4) == 0) {
cli_dis:
            if (debugsOn)
	        printf("%d ->%s disconnected\n",ts_fd,uname);
            sprintf(buffer,"%s has disconnected",uname);
            addr a = h ;
            do {
                a = a->next;
                sfd = a->port;
                if(sfd == ts_fd)
                    removeClient(sfd, h);
                if(sfd != ts_fd) {
	 	    // Encrypt message
		    n = strlen(buffer);
                    xor_encrypt(key, buffer, n);
                    send(sfd,buffer,n,0);
		}
            } while (a->next != NULL);
            displayConnected(h);
            close(ts_fd);
            free(msg);
            break;
        }
	if (debugsOn)
            printf("\nData in buffer after disconnect check: %s %s\n",uname,buffer);
	strncpy(msg,uname,sizeof(uname));
        x=strlen(msg);
        strp = msg;
        strp+= x;
        strncat(strp,buffer,sizeof(buffer));
        msglen=strlen(msg);
        addr a = h ;
        do {
            a = a->next;
            sfd = a->port;
            if(sfd != ts_fd) {
		// Handles sending client messages to all connected clients
		n = strlen(msg);
		xor_encrypt(key, msg, n);
                send(sfd,msg,n,0);
	     }
        } while(a->next != NULL);
        displayConnected(h);
        bzero(msg,BUFFER_MAX);
    }
    return 0;
}

void printDebug (char *string) {
    if (debugsOn)
        printf("%s\n",string);
    fflush(stdout);
}

clients ClientList (clients h) {
    if(h != NULL)
        removeAllClients(h);
    h = malloc(sizeof(struct client));
    if(h == NULL)
        syslog(LOG_INFO, "%s", "Out of memory.");
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
        syslog(LOG_INFO, "%s", "Max connections reached.");
    TmpCell->port = port;
    strncpy(TmpCell->username,username,sizeof(username));
    TmpCell->next = a->next;
    a->next = TmpCell;
}

void displayConnected (const clients h) {
    addr a =h ;
    if (h->next == NULL)
        printDebug("No clients currently connected.");
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
    syslog(LOG_INFO, "%s", "Server shutting down.");
    disconnectAllClients();
    exit(0);
}

void disconnectAllClients () {
    int sfd;
    addr a = h ;
    int i=0;
    if( h->next == NULL ) {
        syslog(LOG_INFO, "%s", "Closing as no clients connected.");
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
    printDebug("\rConnected clients:\n");
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

bool checkUser (char *user) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char *tmp = NULL;
    char *tempUser = NULL;

    tempUser = user;

    tempUser[strlen(tempUser) - 1] = '\0';

    printf("Checking user %s is in db\n",tempUser);
    fp = fopen("cfg/users", "r");
    if (fp == NULL) {
       printDebug("Failed to read users file.");
    }

    // Read contents of user file
    while ((read = getline(&line, &len, fp)) != -1) {
       line[strlen(line) - 1] = '\0';
       printf("Found %s in db\n", line);
       if (strncmp(line, tempUser, sizeof(tempUser)) == 0) {
           printf("Found user %s.", tempUser);
           return true;
       }
    }
return false;
}
