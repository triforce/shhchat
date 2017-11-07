/*
=====================================
shhchat - simple encrypted linux chat
=====================================
chat client
=====================================
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
#include <time.h>
#include <pwd.h>
#include <termios.h>
#include "../lib/shhchat_ssl.h"
#include "../lib/shhchat_cfg.h"

#define default_port 9956 
#define BUFFER_MAX 1024

#ifndef VERSION
#define VERSION "_beta"
#endif

#ifdef DEBUG
#define debug 1
#else
#define debug 0 
#endif

#define RED "\e[1;31m"
#define GREEN "\e[1;32m"
#define YELLOW "\e[1;33m"
#define BLUE "\e[1;34m"
#define RESET_COLOR "\e[0m"

int sockfd, n, x, y, count;
struct sockaddr_in serv_addr;
char *homeDir;
char buffer[BUFFER_MAX];
char buf[10];
char pwbuf[25];
char plain[] = "";
char key[] = "123456";
char serverKey[BUFFER_MAX];
void *interrupt_Handler();
void *chat_write(int);
void *chat_read(int);
void *zzz();
void printDebug(char *string);
void printLog(char *string);
void writeLog(FILE *fp, char *str);
void addYou();
void outputHelp();
void *xor_encrypt(char *key, char *string, int n);
void initLog(char logname[]);
int createPaths(char log_name_default[], char log_name_new[]);
void exitLogError();
void clearLogs();
bool debugsOn = false;
bool logsOn = false;
bool addedYou = false;
bool sslon = false;
FILE *fp_l;
SSL_CTX *ssl_context;
SSL *ssl;

int main(int argc, char *argv[]) {
    pthread_t thr1, thr2;
    unsigned short port = default_port;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    bool haveKey = false;
    int buf_size;
    struct parameters params;

    if ((homeDir = getenv("HOME")) == NULL) {
        homeDir = getpwuid(getuid())->pw_dir;
    }

    if (debug == 1) {
        debugsOn = true;
    }

    init_parameters(&params, homeDir);
    parse_config(&params);

    if (argc < 2) {
        printf("Usage: ./shhclient <server_ip> <optional_port>\n");
        exit(0);
    }

    if (argc == 3) {
        port = atoi(argv[2]);
    }

    printf("%sshhchat client v%s started\n%s", GREEN, VERSION, RESET_COLOR);
    printDebug("Searching for key file...\n");

    // Find key file for use in current session
    fp = fopen(params.client_simple_key, "r");

    if (fp == NULL) {
        printf("Key file not found in '%s' - Checking in conf directory...\n", homeDir);
        fp = fopen("conf/key", "r");

        if (fp == NULL) {
            printf("%sKey file not found - This is the minimum encryption you can have...Exiting\n%s", RED, RESET_COLOR);
            exit(EXIT_FAILURE);
        }
        else {
            printf("%sKey found in conf directory.\n%s", GREEN, RESET_COLOR);
        }
    }

    // Read contents of key file
    while ((read = getline(&line, &len, fp)) != -1) {
        buf_size = sizeof(line);
        strncpy(key, line, buf_size);
        haveKey = true;
        break;
    }

    fclose(fp);
    free(line);

    if (!haveKey) {
        printDebug("Failed to read key file\n");
        exit(EXIT_FAILURE);
     }

    // Configure SSL
    initSSL();

    ssl_context = SSL_CTX_new(TLSv1_2_client_method());
         
    if (!ssl_context) {
        fprintf (stderr, "SSL_CTX_new ERROR\n");
        // ERR_print_errors_fp(stderr);
    }

    if (!SSL_CTX_use_certificate_file(ssl_context, params.client_ssl_cert, SSL_FILETYPE_PEM)) {
        // fprintf (stderr, "SSL_CTX_use_certificate_file ERROR\n");
        // ERR_print_errors_fp(stderr);
    }
    else {
        printf("%sSSL enabled.\n%s", GREEN, RESET_COLOR);
        sslon = true;
    }

    SSL_CTX_use_PrivateKey_file(ssl_context, params.client_ssl_key, SSL_FILETYPE_PEM);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
        printf ("%sFailed to open socket - Is the port already in use?\n%s", RED, RESET_COLOR);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    // Get credentials
    bzero(buf, 10);
    bzero(pwbuf, 15);
    printf("Enter username: ");
    fgets(buf, 10, stdin);
    __fpurge(stdin);
    buf[strlen(buf)-1] = ':';

    struct termios oflags, nflags;

    // Disabling echo
    tcgetattr(fileno(stdin), &oflags);
    nflags = oflags;
    nflags.c_lflag &= ~ECHO;
    nflags.c_lflag |= ECHONL;

    if (tcsetattr(fileno(stdin), TCSANOW, &nflags) != 0) {
        perror("tcsetattr");
        return EXIT_FAILURE;
    }

    enter_pw:
    printf("Enter password: ");
    fgets(pwbuf, 15, stdin);
    if (strlen(pwbuf) < 5) {
        __fpurge(stdin);
        printf("Password too short, try again..\n");
        goto enter_pw;
    }
    __fpurge(stdin);

    if (tcsetattr(fileno(stdin), TCSANOW, &oflags) != 0) {
        perror("tcsetattr");
        return EXIT_FAILURE;
    }

    pwbuf[strlen(pwbuf)-1] = ':';
    strncat(pwbuf, buf, sizeof(buf));

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        printf("%sFailed to connect - Is the server running?\n%s", RED, RESET_COLOR);
        exit(0);
    } else
        printf("%s%s connected.\n%s", GREEN, buf, RESET_COLOR);

    if (sslon) {
        ssl = SSL_new(ssl_context);
        SSL_set_fd(ssl, sockfd);
        int ssl_done = SSL_connect(ssl);

        if (ssl_done < 0) {
            fprintf (stderr, "SSL handshake failed.\n");
            ERR_print_errors_fp(stderr);

            return EXIT_FAILURE;
        } else {
            printf("%sSSL handshake succeeded.\n%s", GREEN, RESET_COLOR);
        }
    }

	addYou();

    y = strlen(pwbuf);
    xor_encrypt(key, pwbuf, y);
    
    if (sslon)
        SSL_write(ssl, pwbuf, y);
    else
        send(sockfd, pwbuf, y, 0);

    // intptr_t fixes int return of different size 
    pthread_create(&thr2,NULL, (void *)chat_write, (void *)(intptr_t)sockfd);
    pthread_create(&thr1,NULL, (void *)chat_read, (void *)(intptr_t)sockfd);

    pthread_join(thr2, NULL);
    pthread_join(thr1, NULL);
    return 0;
}

// Read data from socket, updating chat
void *chat_read(int sockfd) {
    if (signal(SIGINT, (void *)interrupt_Handler) == 0)
        if (signal(SIGTSTP, (void *)zzz) == 0)
            while (1) {
                bzero(buffer, BUFFER_MAX);
                fflush(stdout);

                if (sslon)
                    n = SSL_read(ssl, buffer, sizeof(buffer));
                else
                    n = recv(sockfd, buffer, sizeof(buffer), 0);

                if (n == 0) {
                    printf("%sLost connection to the server.\n\n%s", RED, RESET_COLOR);
                    exit(0);
                }

                if (n > 0) {
                    // Decrypt message
                    y = n;
                    xor_encrypt(key, buffer, y);

                    if (strncmp(buffer, "!!shutdown", 10) == 0) {
                        if (strlen(buffer) > 10) {
                            printf("%s\n%s%s", GREEN, buffer + 10, RESET_COLOR);
                            exit(0);
                        } else {
                            printf("%s\nConnection closed.\n%s", GREEN, RESET_COLOR);
                            exit(0);
                        }
                    }

                    // Special server commands are preceded by '!!'
                    if (strncmp(buffer, "!!key", 5) == 0) {
                        sprintf(serverKey, "%s", buffer + 5);
                        continue;
                    }

                    if (addedYou) {
                        printLog(buffer);
                        printf("\r%s%s%s\n", BLUE, buffer, RESET_COLOR);
                    } else {
                        printf("\n%s%s%s", BLUE, buffer, RESET_COLOR);
                        addedYou = false;
                    }

                    addYou();
                }
            }
    return 0;
}

void *chat_write(int sockfd) {
    bool clear = true;

    while (1) {
        // Continuously read from stdin
        clear = true;
        bzero(buffer, BUFFER_MAX);

        __fpurge(stdin);
	    fgets(buffer, sizeof(buffer), stdin);

        if (strlen(buffer)-1 > sizeof(buffer)) {
            printf("%sBuffer full, reduce size of message.%s\n", GREEN, RESET_COLOR);
            bzero(buffer, BUFFER_MAX);
            __fpurge(stdin);
        }

        if (strlen(buffer) > 1) {

            buffer[strcspn(buffer, "\n")] = 0;
            // ====================
            // Server Side Requests

            if (strcmp(buffer, "??who") == 0) {
                printf("%sUser's currently connected:%s\n", GREEN, RESET_COLOR);
                clear = false;
            }

            if (strcmp(buffer, "??list") == 0) {
                printf("%sUser's in db:%s\n", GREEN, RESET_COLOR);
                clear = false;
            }

            if (strcmp(buffer, "??quit") == 0) {
                exit(0);
            }

            // End Server Side Requests
            // ========================


            // ==============
            // Local Commands

            if (strcmp(buffer, "??loggingon") == 0) {
                logsOn = true;
                initLog ("shh_log");
                addYou();
                continue;
            }

            if (strcmp(buffer, "??loggingoff") == 0) {
                logsOn = false;
                addYou();
                continue;
            }

            if (strcmp(buffer, "??logclear") == 0) {
                clearLogs();
                addYou();
                continue;
            }

            if (strcmp(buffer, "??help") == 0) {
                outputHelp();
                addYou();
                continue;
            }

            // End Local Commands
            // ==================

            printLog(buffer);

            char *tmp = strdup(buffer);
            strncpy(buffer, serverKey, sizeof(serverKey));
            strcat(buffer, tmp);
            free(tmp);

            // Encrypt message
            y = strlen(buffer);
            xor_encrypt(key, buffer, y);

            if (sslon)
                n = SSL_write(ssl, buffer, y);
            else
                n = send(sockfd, buffer, y, 0);

        } else
            __fpurge(stdin);

        bzero(buffer, BUFFER_MAX);

        if (clear)
            addYou();
    }
    return 0;
}

void *interrupt_Handler() {
    printf("\r%sType '??quit' to exit.\n%s", YELLOW, RESET_COLOR);
    return 0;
}
void *zzz() {
    printf("\r%sType '??quit' to exit.\n%s", YELLOW, RESET_COLOR);
    return 0;
}

void printDebug(char *string) {
    if (debugsOn)
        printf("%s", string);
}

void printLog(char *string) {
    if (logsOn)
        writeLog(fp_l, string);
}

void clearLogs() {
    system("rm -f shh_log*");
}

void initLog(char logname[]) {
    int result;
    char log_name_default[1024] = "/";
    char log_name_new[1024] = "/";

    strncat(log_name_default, logname, strlen(logname));
    strncat(log_name_new, logname, strlen(logname));
    strncat(log_name_new, "_", 1);

    if (createPaths(log_name_default, log_name_new) == 0)
        exitLogError();

    result = rename(log_name_default, log_name_new);

    // Open log file
    fp_l = fopen(log_name_default, "a");

    if (fp_l == NULL && result != 0)
        exitLogError();
}

int createPaths(char log_name_default[], char log_name_new[]) {
    int time_t = (int)time(NULL);
    char time_str[32];
    char cwd[1024];
    char tmp_cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return 0;

    strncpy(tmp_cwd, cwd, strlen(cwd));
    sprintf(time_str, "%d", time_t);
    strcat(log_name_new, time_str);
    strcat(cwd, log_name_new);
    strncpy(log_name_new, cwd, strlen(cwd));
    strncpy(cwd, tmp_cwd, strlen(cwd));
    strcat(cwd, log_name_default);
    strncpy(log_name_default, cwd, strlen(cwd));

    return 1;
}

void exitLogError() {
    printf("%sLog file error, shutting down.\n%s", RED, RESET_COLOR);
    exit(0);
}

void writeLog(FILE *fp, char *str) {
    fprintf(fp, "%s", str);
}

void addYou() {
    addedYou = true;
    printf("You: ");
}

void outputHelp() {
    printf("\n??logon - Start logging chat session to local file.\n??logoff - Stop logging chat session to local file.\n??logclear - Delete the current logfile.\n??who - Display which users are connected.\n??list - Show all available user accounts.\n??quit - Shutdown this client.\n");
}

// Encryption routine
void *xor_encrypt(char *key, char *string, int n) {
    int i;
    int keyLength = strlen(key);

    for (i = 0;i < n;i++) {
        string[i] = string[i]^key[i%keyLength];
    }
    return 0;
}

