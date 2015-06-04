/*
=====================================
shhchat - simple encrypted linux chat
=====================================
chat server
=====================================
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
#include "../lib/shhchat_ssl.h"
//#include <libwebsockets.h>
//#include "shhchat_ws.h"

#define BACKLOG 100
#define BUFFER_MAX 1024
#define default_port 9956
#define RED  "\033[22;31m"
#define RESET_COLOR "\e[m"

#ifdef DEBUG
#define debug 1
#else
#define debug 0 
#endif

struct client {
   int port;
   SSL *ssl;
   char username[10];
   unsigned int sessionId;
   struct client *next;
   char su;
};

/*
TODO #37 - Superuser actions
su privs:
===================================================
level       name        prefix      actions
===================================================
    0       (none)      n/a         n/a
    1       (mod)       +           kick users
    2       (admin)     @           change user db
===================================================
*/

typedef struct client *ptrtoclient;
typedef ptrtoclient clients;
typedef ptrtoclient addr;
void disconnectAllClients();
clients ClientList(clients h);
void removeClient(int port, SSL *ssl_fd, clients h);
void addClient(int port, SSL *ssl_fd, char*, clients h, addr a);
void removeAllClients(clients h);
void displayConnected(const clients h);
bool checkConnected(const clients h, char *username);
void *closeServer();
void *server(void * arg);
void showConnected();
void xor_encrypt(char *key, char *string, int n);
void printDebug(char *string);
void *webInit(void * arg);
bool checkUser(char *string);
int populateDbUsers(char *msg);
char username[10];
int sf2, n, count;
clients h;
char buffer[BUFFER_MAX];
bool debugsOn = false;
char plain[] = "Hello";
char key[] = "123456";
SSL_CTX *ssl_context;
SSL *ssl, *ssl2;
bool sslon = false;

static void start_daemon() {
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

    for (x = sysconf(_SC_OPEN_MAX); x>0; x--) {
        close(x);
    }

   openlog("shhchatd", 0, LOG_USER);
   syslog(LOG_INFO, "%s", "Server daemon started.");
}

int main(int argc, char *argv[]) {
    int socket_fd, new_fd, portnum, cli_size, buf_size;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    pthread_t thr;
    int yes = 1;
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
    else
	    start_daemon();

    while (1) {

        if (argc == 2)
            portnum = atoi(argv[1]);
        else
            portnum = default_port;

        printDebug("Reading contents of config file.");

        // TODO #11 - Create cfg if it doesn't exist

        // Try and open key file
        fp = fopen("cfg/key", "r");

        if (fp == NULL) {
            printDebug("Failed to read key file in standard build dir, checking in /etc/shhchat...");
            fp = fopen("/etc/shhchat/key", "r");

            if (fp == NULL) {
                printDebug("Failed to read key file...Exiting");
                exit(EXIT_FAILURE);
            }
        }

        // Read contents of key file
        while ((read = getline(&line, &len, fp)) != -1) {
            printDebug("Key found.");
            buf_size = sizeof(line);
            strncpy(key, line, buf_size);

            if (debugsOn)
                printf("%s", key);

            haveKey = true;
            break;
        }

        fclose(fp);
        free(line);

        if (!haveKey) {
            printDebug("Failed to read key file.");
            exit(EXIT_FAILURE);
         }

        if (debugsOn) {
            printf("Testing encryption key on string: '%s'\n", plain);
            n = strlen(plain);
            xor_encrypt(key, plain, n);
            printf("Encrypting...%s\n", plain);

            n = strlen(plain);
            xor_encrypt(key, plain, n);
            printf("Decrypted '%s'\nIf the two do no match then your encryption hasn't worked.\n", plain);
        }

        openlog("shhchatd", 0, LOG_USER);
        syslog(LOG_INFO, "%s", "Server setup successful.");

  
        // Setup SSL
        initSSL();

        ssl_context = SSL_CTX_new(SSLv3_server_method());

        if (!ssl_context) {
            fprintf (stderr, "SSL_CTX_new ERROR\n");
            // ERR_print_errors_fp(stderr);
        }

        if (!SSL_CTX_use_certificate_file(ssl_context, "certificate.pem", SSL_FILETYPE_PEM)) {
            fprintf (stderr, "SSL_CTX_use_certificate_file ERROR\n");
            // ERR_print_errors_fp(stderr);
        }
        else {
            sslon = true;
        }

        SSL_CTX_use_PrivateKey_file(ssl_context, "key.pem", SSL_FILETYPE_PEM);

        if (!SSL_CTX_check_private_key(ssl_context)) {
            sslon = false;
        }
        
        // Clear out list of clients
        h = ClientList(NULL);

        // Open socket and listen for connections
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(portnum);

        if (debugsOn)
            printf("IP Address: %s\n", inet_ntoa(server_addr.sin_addr));

        socket_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (socket_fd == -1) {
            syslog(LOG_INFO, "%s", "Socket error, closing...");
            exit(1);
        } else
            syslog(LOG_INFO, "%s", "Socket created.");

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            syslog(LOG_INFO, "%s", "Socket error.");
            exit(1);
        }

        if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
            syslog(LOG_INFO, "%s", "Failed to bind.");
            exit(1);
        } else
            syslog(LOG_INFO, "%s", "Port binded.");

        listen(socket_fd, BACKLOG);

        // TODO #29 - Create a WebSocket thread
        // pthread_create(&thr, NULL, webInit, portnum);
        // pthread_detach(thr);

        if (signal(SIGINT, (void *)closeServer) == 0)
            if (signal(SIGTSTP, showConnected) == 0)
                while (1) {
                    cli_size = sizeof(struct sockaddr_in);
                    new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, (socklen_t*)&cli_size);

                    if (sslon) {
                        ssl = SSL_new(ssl_context);
                        SSL_set_fd(ssl, new_fd);
                        int ssl_done = SSL_accept(ssl);
                        // printf("%i",ssl_done);
                        if (ssl_done <= 0) {
                            fprintf (stderr, "SSL handshake failed.\n");
                            ERR_print_errors_fp(stderr);

                            return EXIT_FAILURE;
                        } else {
                            printf("SSL handshake succeeded.\n");
                        }
                    }

                    a = h;
                    bzero(username, 10);
                    bzero(buffer, BUFFER_MAX);

                    if ( (!sslon && (recv(new_fd, username, sizeof(username), 0) > 0)) || (sslon && (SSL_read(ssl, username, sizeof(username)) > 0)) )  {
                        n = strlen(username);
                        xor_encrypt(key, username, n);
                        username[strlen(username)-1] = ':';
                        sprintf(buffer, "%s logged in\n", username);

                        // Check user hasn't already logged in
                        if (checkConnected(h, username)) {
                            char shutdown[] = "!!shutdown";
                            n = strlen(shutdown);
                            xor_encrypt(key, shutdown, n);

                            if (sslon)
                                SSL_write(ssl, shutdown, 10);
                            else
                                send(new_fd, shutdown, 10, 0);
                        }

                        addClient(new_fd, ssl, username, h, a);

                        a = a->next;
                        unsigned int tmp = a->sessionId;
                        a = h;

                        do {
                            a = a->next;
                            sf2 = a->port;
                            ssl2 = a->ssl;

                            if ((!sslon && sf2 != new_fd) || (sslon && ssl2 != ssl)) {
                                n = strlen(buffer);
                                xor_encrypt(key, buffer, n);

                                if (sslon)
                                    SSL_write(ssl2, buffer, n);
                                else
                                    send(sf2, buffer, n, 0);
                            }
                        } while (a->next != NULL);

                        if (debugsOn)
                            printf("Connection made from %s\n\n", inet_ntoa(client_addr.sin_addr));

                        struct client args;

                        if (sslon) {
                            args.ssl = ssl;
                        } else {
                            args.port = new_fd;
                        }
                        
                        args.sessionId = tmp;
                        strncpy(args.username, username, sizeof(username));
                        pthread_create(&thr, NULL, server, (void*)&args);
                        pthread_detach(thr);
                    }
                }
                removeAllClients(h);
                close(socket_fd);
    }
}

// Server side handling for each connected client 
void *server(void * arguments) {
    struct client *args = arguments;
    char buffer[BUFFER_MAX], uname[10], keybuf[BUFFER_MAX];
    char *strp;
    char *msg = (char *) malloc(BUFFER_MAX);
    char keystr[] = "!!key";
    int ts_fd, x, y, sfd, msglen, buf_size;
    unsigned int sessionId;
    ts_fd = args->port;
    strncpy(uname, args->username, sizeof(args->username));
    addr a;
    a = h;
    sessionId = args->sessionId;
    SSL *ts_ssl, *sfd_ssl;
    ts_ssl = args->ssl;

    /*
    Server Control Commands:-
    * ??quit    - disconnects from chat session
    * ??who     - query who is connected
    * ??list    - list all users in db
    * ??sup     - gain superuser rights TODO #37
    * ?!kick    - kick user <name> (requires ??sup) TODO #37
    */

    if (!checkUser(uname)) {

        syslog(LOG_INFO, "%s", "User does not exist in db.");
	    goto cli_dis;
    }

    // Send client unique key
    bzero(keybuf, BUFFER_MAX);
    sprintf(keybuf, "%u", args->sessionId);
    char *tmp = strdup(keybuf);
    strcpy(keybuf, keystr);
    strcat(keybuf, tmp);
    n = strlen(keybuf);
    xor_encrypt(key, keybuf, n);
    fflush(stdout);

    if (sslon)
        SSL_write(ts_ssl, keybuf, n);
    else
        send(ts_fd, keybuf, n, 0);

    free(tmp);

    // Add colon back in, need to check it doesn't go over allocated length
    uname[strlen(uname)] = ':';
    uname[strlen(uname)] = ' ';

    /*
    ubuf[BUFFER_MAX]
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

    while (1) {
        bzero(buffer, BUFFER_MAX);
        if (sslon)
            y = SSL_read(ts_ssl, buffer, sizeof(buffer));
        else
            y = recv(ts_fd, buffer, sizeof(buffer), 0);

        if (y == 0)
            goto cli_dis;

        n = y;
        xor_encrypt(key, buffer, n);

        // Check message is valid
        char tmpid[BUFFER_MAX];
        snprintf(tmpid, sizeof(tmpid), "%u", sessionId);

        if (strncmp(buffer, tmpid, strlen(tmpid)) == 0) {
            char tmp[BUFFER_MAX];
            snprintf(tmp, sizeof(tmp), "%s", buffer + strlen(tmpid));
            bzero(buffer, BUFFER_MAX);
            snprintf(buffer, sizeof(buffer), "%s", tmp);
        } else {
            printf("Client %s has invalid key in message\n", uname);
            goto cli_dis;
        }

        // ??who     - query who is connected
        if (strncmp(buffer, "??who", 5) == 0) {
            addr a = h;
            bzero(msg, BUFFER_MAX);

            do {
                a = a->next;
                sfd = a->port;
                sfd_ssl = a->ssl;
                strncat(msg, a->username, strlen(a->username)-1);
                strncat(msg, ", ", 2);
            } while (a->next != NULL);

            strncat(msg, "\n", 2);
            sprintf(buffer, "%s", msg);
            n = strlen(buffer);
            xor_encrypt(key, buffer, n);

            if (sslon)
                SSL_write(ts_ssl, buffer, n);
            else
                send(ts_fd, buffer, n, 0);
            
            bzero(msg, BUFFER_MAX);
            continue;
        }

        // ??list     - list all users in db
        if (strncmp(buffer, "??list", 6) == 0) {

            if (populateDbUsers(msg) == 1) {
                strncat(msg, "\n", 2);
                sprintf(buffer, "%s", msg);
                n = strlen(buffer);
                xor_encrypt(key, buffer, n);

                if (sslon)
                    SSL_write(ts_ssl, buffer, n);
                else
                    send(ts_fd, buffer, n, 0);
            }

            bzero(msg, BUFFER_MAX);
            continue;
        }

        // ??quit    - disconnects from chat session
        if (strncmp(buffer, "??quit", 6) == 0) {
cli_dis:
            if (debugsOn) {
                printf("%d ->%s disconnected\n", ts_fd, uname);
            }

            sprintf(buffer, "%s has disconnected\n", uname);
            addr a = h;

            do {
                a = a->next;
                sfd = a->port;
                sfd_ssl = a->ssl;

                if ((!sslon && sfd == ts_fd) || (sslon && sfd_ssl == ts_ssl)) {
                    removeClient(sfd, sfd_ssl, h);
                }

                if ((!sslon && sfd != ts_fd) || (sslon && sfd_ssl != ts_ssl)) {
	 	            // Encrypt message
		            n = strlen(buffer);
                    xor_encrypt(key, buffer, n);

                    if (sslon)
                        SSL_write(sfd_ssl, buffer, n);
                    else
                        send(sfd, buffer, n, 0);


		        }
            } while (a->next != NULL);

            if (debugsOn)
                displayConnected(h);

            if (sslon)
                SSL_free(ts_ssl);
            
            close(ts_fd);

            free(msg);
            break;
        }

        if (debugsOn)
            printf("\nData in buffer after disconnect check: %s %s\n", uname, buffer);

	    strncpy(msg, uname, sizeof(uname));
        x = strlen(msg);
        strp = msg;
        strp += x;
        buf_size = sizeof(buffer);
        strncat(strp, buffer, buf_size);
        msglen = strlen(msg);
        addr a = h;

        do {
            a = a->next;
            sfd = a->port;
            sfd_ssl = a->ssl;

            if ((!sslon && sfd != ts_fd) || (sslon && sfd_ssl != ts_ssl)) {
                // Handles sending client messages to all connected clients
                n = strlen(msg);
                xor_encrypt(key, msg, n);

                if (sslon)
                    SSL_write(sfd_ssl, msg, n);
                else
                    send(sfd, msg, n, 0);
	        }
        } while (a->next != NULL);

        if (debugsOn)
            displayConnected(h);

        bzero(msg, BUFFER_MAX);
    }
    return 0;
}

void printDebug(char *string) {
    if (debugsOn)
        printf("%s\n", string);

    fflush(stdout);
}

clients ClientList(clients h) {
    if (h != NULL)
        removeAllClients(h);

    h = malloc(sizeof(struct client));

    if (h == NULL)
        syslog(LOG_INFO, "%s", "Out of memory.");

    h->next = NULL;
    return h;
}

void removeAllClients(clients h) {
    addr a, Tmp;
    a = h->next;
    h->next = NULL;

    while (a != NULL) {
        Tmp = a->next;
        free(a);
        a = Tmp;
    }
}

void addClient(int port, SSL *ssl_fd, char *username, clients h, addr a) {
    addr tmpcell;
    tmpcell = malloc(sizeof(struct client));
    unsigned int rangen = (unsigned int)time(NULL);
    int buf_size;
    srand(rangen);

    if (tmpcell == NULL)
        syslog(LOG_INFO, "%s", "Max connections reached.");

    tmpcell->su = 0;
    tmpcell->port = port;
    tmpcell->ssl = ssl_fd;
    tmpcell->sessionId = rand();
    buf_size = sizeof(username);
    strncpy(tmpcell->username, username, buf_size);
    tmpcell->next = a->next;
    a->next = tmpcell;
}

void displayConnected(const clients h) {
    addr a = h;

    if (h->next == NULL)
        printDebug("No clients currently connected.");
    else {
        do {
            a = a->next;
            printf("%s \t", a->username);
        } while (a->next != NULL);

        printf( "\n" );
    }
}

bool checkConnected(const clients h, char *username) {
    addr a = h;

    if (h->next == NULL)
        return false;
    else {
        do {
            a = a->next;

            if (strcmp(username, a->username) == 0)
                return true;

        } while (a->next != NULL);

        return false;
    }
}

void removeClient(int port, SSL *ssl_fd, clients h) {
    addr a, TmpCell;
    a = h;

    while (a->next != NULL && a->next->port != port && a->next->ssl != ssl_fd)
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

void disconnectAllClients() {
    int sfd;
    addr a = h;
    int i = 0;
    SSL *sfd_ssl;

    if (h->next == NULL) {
        syslog(LOG_INFO, "%s", "Closing as no clients connected.");
        exit(0);
    }
    else {
        do {
            i++;
            a = a->next;
            sfd = a->port;
            sfd_ssl = a->ssl;
            char shutdown[] = "!!shutdown";
            n = strlen(shutdown);
            xor_encrypt(key, shutdown, n);

            if (sslon)
                SSL_write(sfd_ssl, shutdown, 10);
            else
                send(sfd, shutdown, 10, 0);

        } while (a->next != NULL);

        printf("%d clients closed.\n\n", i);
    }
}

void showConnected() {
    printDebug("\rConnected clients:\n");
    displayConnected(h);
}

// Encryption routine
void xor_encrypt(char *key, char *string, int n) {
    int i;
    int keyLength = strlen(key);

    for (i = 0;i < n;i++) {
        string[i] = string[i]^key[i%keyLength];
    }
}

bool checkUser(char *user) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char *tmp = NULL;
    char *tempUser = NULL;
    int buf_size;

    tempUser = user;
    tempUser[strlen(tempUser) - 1] = '\0';
    fp = fopen("cfg/users", "r");

    if (fp == NULL) {
       printDebug("Failed to read users file.");
       fclose(fp);
       return false;
    }

    // Read contents of user file
    while ((read = getline(&line, &len, fp)) != -1) {
        line[strlen(line) - 1] = '\0';
        buf_size = sizeof(tempUser);

        if (strncmp(line, tempUser, buf_size) == 0) {
            // printf("Found user %s.", tempUser);
            fclose(fp);
            return true;
        }
    }
    fclose(fp);
    return false;
}

int populateDbUsers(char *msg) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen("cfg/users", "r");

    if (fp == NULL) {
       printDebug("Failed to read users file.");
       fclose(fp);
       return 0;
    }

    // Read contents of user file
    while ((read = getline(&line, &len, fp)) != -1) {
        strncat(msg, line, strlen(line)-1);
        strncat(msg, ", ", 2);
    }

    fclose(fp);
    return 1;
}

/* TODO #29 - WebSocket support
void *webInit(void * arguments) {
    int port = atoi(arguments);
    struct lws_context_creation_info info;
    const char *interface = NULL;
    struct libwebsocket_context *context;

    // SSL specific
    const char *cert_path = NULL;
    const char *key_path = NULL;

    int opts = 0;
    memset(&info, 0, sizeof info);

    info.port = port;
    info.iface = interface;
    info.protocols = protocols;
    info.extensions = libwebsocket_get_internal_extensions();
    //if (!use_ssl) {
    info.ssl_cert_filepath = NULL;
    info.ssl_private_key_filepath = NULL;
    //} else {
    //  info.ssl_cert_filepath = LOCAL_RESOURCE_PATH"/libwebsockets-test-server.pem";
    //  info.ssl_private_key_filepath = LOCAL_RESOURCE_PATH"/libwebsockets-test-server.key.pem";
    //}
    info.gid = -1;
    info.uid = -1;
    info.options = opts;

    context = libwebsocket_create_context(&info);

    if (context == NULL) {
        fprintf(stderr, "shhchat_ws server failed\n");
        exit(1);
    }

    printf("shhchat_ws server started\n");

    while (1) {
        libwebsocket_service(context, 50);
    }

    libwebsocket_context_destroy(context);
    exit(1);
}
*/
