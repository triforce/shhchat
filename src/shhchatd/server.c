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
#include "../lib/shhchat_cfg.h"
//#include <libwebsockets.h>
//#include "../lib/shhchat_ws.h"

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
   char password[15];
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
void addClient(int port, SSL *ssl_fd, char*, char*, clients h, addr a);
void removeAllClients(clients h);
void displayConnected(const clients h);
bool checkConnected(const clients h, char *username);
void *closeServer();
void *server(void * arg);
void showConnected();
void xor_encrypt(char *key, char *string, int n);
void printDebug(char *string);
void *webInit(void * arg);
bool checkCredentials(char *user, char *pass);
int populateDbUsers(char *msg);
char *convertToString(char *);
char username[10];
char password[15];
char pwbuf[25];
int sf2, n, count;
clients h;
char buffer[BUFFER_MAX];
bool debugsOn = false;
char plain[] = "Hello";
char key[] = "123456";
SSL_CTX *ssl_context;
SSL *ssl, *ssl2;
bool sslon = false;
bool pw_change = false;

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
    bool have_key = false;
    struct parameters params;

    init_parameters(&params, NULL);
    parse_config(&params);

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

        // TODO #11 - Create conf if it doesn't exist

        // Try and open key file
        fp = fopen(params.server_simple_key, "r");

        if (fp == NULL) {
            syslog(LOG_INFO, "%s", "Failed to find key file in config defined location - Checking if you have a conf folder in the current directory...");
            fp = fopen("conf/key", "r");

            if (fp == NULL) {
                syslog(LOG_INFO, "%s", "Key file not found - This is the minimum encryption you can have...Exiting");
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

            have_key = true;
            break;
        }

        fclose(fp);
        free(line);

        if (!have_key) {
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

        ssl_context = SSL_CTX_new(TLSv1_2_server_method());

        if (!ssl_context) {
            fprintf (stderr, "SSL_CTX_new ERROR\n");
            // ERR_print_errors_fp(stderr);
        }

        if (!SSL_CTX_use_certificate_file(ssl_context, params.server_ssl_cert, SSL_FILETYPE_PEM)) {
            fprintf (stderr, "SSL_CTX_use_certificate_file ERROR\n");
            // ERR_print_errors_fp(stderr);
        }
        else {
            sslon = true;
        }

        SSL_CTX_use_PrivateKey_file(ssl_context, params.server_ssl_key, SSL_FILETYPE_PEM);

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
                    bzero(pwbuf, 25);
                    bzero(username, 10);
                    bzero(password, 15);
                    bzero(buffer, BUFFER_MAX);

                    if ( (!sslon && (recv(new_fd, pwbuf, sizeof(pwbuf), 0) > 0)) || (sslon && (SSL_read(ssl, pwbuf, sizeof(pwbuf)) > 0)) )  {
                        n = strlen(pwbuf);
                        xor_encrypt(key, pwbuf, n);

                        // Get credentials
                        char *result;
                        char original_username[10];
                        char original_pass[15];

                        result = strtok(pwbuf, ":");
                        strncpy(password, result, strlen(result));
                        result = strtok(NULL, ":");
                        strncpy(username, result, strlen(result));
                        username[strlen(username)] = ':';

                        // Save these for later when creating the new client
                        memcpy(original_pass, password, sizeof(password));
                        memcpy(original_username, username, sizeof(username));

                        // Run checks on the credentials
                        if (checkConnected(h, username) || !checkCredentials(username, password)) {
                            char upi[] = "!!shutdownUsername / password incorrect or your user is already connected elsewhere.\n";
                            n = strlen(upi);
                            xor_encrypt(key, upi, n);

                            if (sslon)
                                SSL_write(ssl, upi, strlen(upi));
                            else
                                send(new_fd, upi, strlen(upi), 0);
                        }

                        if (pw_change) {
                            char nopwd[] = "No password in database, setting password up.\n";
                            n = strlen(nopwd);
                            xor_encrypt(key, nopwd, n);

                            if (sslon)
                                SSL_write(ssl, nopwd, strlen(nopwd));
                            else
                                send(new_fd, nopwd, strlen(nopwd), 0);
                        }

                        pw_change = false;
                        addClient(new_fd, ssl, original_username, original_pass, h, a);

                        a = a->next;
                        unsigned int tmp = a->sessionId;
                        a = h;
                        sprintf(buffer, "%s logged in", original_username);

                        // Inform all connected clients of new arrival
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

                        if (debugsOn) {
                            printf("Connection made from %s\n\n", inet_ntoa(client_addr.sin_addr));
                        }

                        struct client args;

                        if (sslon) {
                            args.ssl = ssl;
                        } else {
                            args.port = new_fd;
                        }
                        
                        args.sessionId = tmp;
                        strncpy(args.username, original_username, sizeof(original_username));
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

    // Send client unique key
    bzero(keybuf, BUFFER_MAX);
    sprintf(keybuf, "%u", args->sessionId);
    char *tmp = strdup(keybuf);
    strcpy(keybuf, keystr);
    strcat(keybuf, tmp);
    n = strlen(keybuf);
    xor_encrypt(key, keybuf, n);

    if (sslon)
        SSL_write(ts_ssl, keybuf, n);
    else
        send(ts_fd, keybuf, n, 0);

    free(tmp);
    // Add colon back in, need to check it doesn't go over allocated length
    // uname[strlen(uname)] = ':';
    // uname[strlen(uname)] = ' ';
    // printf("username after : %s", uname);
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
        if (strcmp(buffer, "??who") == 0) {
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
        if (strcmp(buffer, "??list") == 0) {

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
        if (strcmp(buffer, "??quit") == 0) {
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

        // Add blank space after username so it reads on other clients as 'user1: ' + buffer
        uname[strlen(uname)] = ' ';
        uname[strlen(uname) + 1] = '\0';
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

void addClient(int port, SSL *ssl_fd, char *username, char *password, clients h, addr a) {
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
    buf_size = sizeof(password);
    strncpy(tmpcell->password, password, buf_size);
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

char *convertToString(char *encrypted_pass) {
    int i;
    char *md5string = (char*)malloc(33);

    for (i = 0; i < 16; ++i) {
        snprintf(&(md5string[i*2]), 16*2, "%02x", (unsigned int)encrypted_pass[i]);
    }
    return md5string;
}

bool checkCredentials(char *user, char *pass) {
    FILE *fp, *fp_o;
    char *line, *tmp = NULL;
    size_t len = 0;
    ssize_t read;
    int buf_size;
    bool found_user, pass_updated, using_etc = false;
    unsigned char encrypted_pass[16];
    char new_line[40];

    MD5(pass, strlen(pass), encrypted_pass);
    char *md5string = convertToString(encrypted_pass);

    user[strlen(user) - 1] = '\0';

    fp = fopen("conf/users", "r+");
    fp_o = fopen("conf/pwds", "w+");

    if (fp == NULL) {
        printDebug("Failed to find user file in local directory, checking in /etc/shhchat...");
        fp = fopen("/etc/shhchat/users", "a+");

        if (fp == NULL) {
            printDebug("Failed to find user file...Exiting");
            free(md5string);
            if (fp_o != NULL) {
                fclose(fp_o);
            }
            return false;
        }
        using_etc = true;
    }

    if (fp_o == NULL) {
        fp_o = fopen("/etc/shhchat/pwds", "w+");

        if (fp_o == NULL) {
            printDebug("Failed to find pwd file...Exiting");
            if (fp != NULL) {
                fclose(fp);
            }
            return false;
        }
    }

    // Read contents of user file
    while ((read = getline(&line, &len, fp)) != -1) {
        // line[strlen(line) - 1] = '\0';
        // buf_size = sizeof(temp_user);
        strncpy(new_line, line, strlen(line));
        char *user_result;
        char *pass_result;
        user_result = strtok(line, ":");

        if (strcmp(user_result, user) == 0) {
            pass_result = strtok(NULL, ":");
            found_user = true;
            
            if (strlen(pass_result) > 1) {
                if (strncmp(pass_result, md5string, 32) == 0) {
                    fseek(fp_o, 0, SEEK_END);
                    fputs(new_line, fp_o);
                } else {
                    found_user = false;
                }
            }
            else {
                pass_updated = true;
            }
        } else {
            fseek(fp_o, 0, SEEK_END);
            fputs(new_line, fp_o);
        }
    }

    if (found_user) {
        if (pass_updated) {
            fseek(fp_o, 0, SEEK_END);
            user[strlen(user)] = ':';
            strncat(user, md5string, strlen(md5string));
            user[strlen(user)] = '\n';
            user[strlen(user) + 1] = '\0';
            fputs(user, fp_o);
            pw_change = true;
        }
        free(md5string);
        free(line);
        fclose(fp_o);
        fclose(fp);

        if (using_etc) {
            rename("/etc/shhchat/pwds", "/etc/shhchat/users");
        } else {
            rename("conf/pwds", "conf/users");
        }
        return true;
    }

    syslog(LOG_INFO, "%s", "User does not exist in db or password was incorrect.");
    free(md5string);
    free(line);
    fclose(fp_o);
    fclose(fp);
    return false;
}

int populateDbUsers(char *msg) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen("conf/users", "r");

    if (fp == NULL) {
        printDebug("Failed to find user file in local directory, checking in /etc/shhchat...");
        fp = fopen("/etc/shhchat/users", "r");

        if (fp == NULL) {
            printDebug("Failed to find user file...Exiting");
            return 0;
        }
    }

    // Read contents of user file
    while ((read = getline(&line, &len, fp)) != -1) {
        char *user_result;
        user_result = strtok(line, ":");
        strncat(msg, user_result, strlen(user_result));
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
