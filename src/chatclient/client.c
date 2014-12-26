/*
shhchat client
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
#define default_port 9956 
#define BUFFER_MAX 1024

#ifndef VERSION
#define VERSION "_beta"
#endif

#define RED "\e[1;31m"
#define GREEN "\e[1;32m"
#define YELLOW "\e[1;33m"
#define BLUE "\e[1;34m"
#define RESET_COLOR "\e[0m"

int sockfd, n, x, y, count;
struct sockaddr_in serv_addr;
char buffer[BUFFER_MAX];
char buf[10];
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
void *xor_encrypt(char *key, char *string, int n);
void initLog(char logname[]);
int createPaths(char log_name_default[], char log_name_new[]);
void exitLogError();
bool debugsOn = false;
bool logsOn = false;
bool addedYou = false;
FILE *fp_l;

int main(int argc, char *argv[]) {
    pthread_t thr1, thr2;
    unsigned short port = default_port;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    bool haveKey = false;
    int buf_size;

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
    fp = fopen(".sshkey", "r");

    if (fp == NULL) {
        printf("%sKey file not found in current dir - Searching in standard build filepath...\n%s", GREEN, RESET_COLOR);
        fp = fopen("cfg/key", "r");

        if (fp == NULL) {
            printf("%sKey file not found in current standard build dir - Searching in /etc/shhchat...\n%s", RED, RESET_COLOR);
            fp = fopen("/etc/shhchat/key", "r");

            if (fp == NULL) {
                printf("%sKey file not found...Exiting\n%s", RED, RESET_COLOR);
                exit(EXIT_FAILURE);
            }
            else {
                printf("%sKey found in /etc/shhchat dir.\n%s", GREEN, RESET_COLOR);
            }
        }
        else {
            printf("%sKey found in cfg dir.\n%s", GREEN, RESET_COLOR);
        }
    }

    // Read contents of key file
    while ((read = getline(&line, &len, fp)) != -1) {
        buf_size = sizeof(line);
        strncpy(key, line, buf_size);
        // Don't print key out!
        // printf("%s", key);
        haveKey = true;
        break;
    }

    fclose(fp);
    free(line);

    if (!haveKey) {
        printDebug("Failed to read key file\n");
        exit(EXIT_FAILURE);
     }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
        printf ("%sFailed to open socket - Is the port already in use?\n%s", RED, RESET_COLOR);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    bzero(buf, 10);
    printf("Choose username for this chat session: ");
    fgets(buf, 10, stdin);
    __fpurge(stdin);
    buf[strlen(buf)-1] = ':';

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        printf("%sFailed to connect - Is the server running?\n%s", RED, RESET_COLOR);
        exit(0);
    } else
        printf("%s%s connected.\n%s", GREEN, buf, RESET_COLOR);

	addYou();

    y = strlen(buf);
    xor_encrypt(key, buf, y);
    send(sockfd, buf, y, 0);
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
                        printf("%s\nServer has shutdown.\n%s", GREEN, RESET_COLOR);
                        exit(0);
                    }

                    // Special server commands are preceded by '!!'
                    if (strncmp(buffer, "!!key", 5) == 0) {
                        sprintf(serverKey, "%s", buffer + 5);
                        continue;
                    }

                    if (addedYou) {
                        printLog(buffer);
                        printf("\r%s%s%s", BLUE, buffer, RESET_COLOR);
                    } else {
                        printf("\n%s%s%s", BLUE, buffer, RESET_COLOR);
                        addedYou = false;
                    }

                    fflush(stdout);
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
        fflush(stdout);
        __fpurge(stdin);
	    fgets(buffer, sizeof(buffer), stdin);

        if (strlen(buffer)-1 > sizeof(buffer)) {
            printf("%sBuffer full, reduce size of message.%s\n", GREEN, RESET_COLOR);
            bzero(buffer, BUFFER_MAX);
            __fpurge(stdin);
        }

        if (strlen(buffer) > 1) {

            // ========================
            // Server Side Requests

            if (strncmp(buffer, "??who", 5) == 0) {
                printf("%sUser's currently connected:%s\n", GREEN, RESET_COLOR);
                clear = false;
            }

            if (strncmp(buffer, "??list", 6) == 0) {
                printf("%sUser's in db:%s\n", GREEN, RESET_COLOR);
                clear = false;
            }

            if (strncmp(buffer, "??quit", 6) == 0) {
                exit(0);
            }

            // End Server Side Requests
            // ========================


            // ==================
            // Local Requests

            if (strncmp(buffer, "??logon", 7) == 0) {
                logsOn = true;
                initLog ("shh_log");
                continue;
            }

            if (strncmp(buffer, "??logoff", 8) == 0) {
                logsOn = false;
                continue;
            }

            // End Local Requests
            // ==================

            printLog(buffer);

            char *tmp = strdup(buffer);
            strncpy(buffer, serverKey, sizeof(serverKey));
            strcat(buffer, tmp);
            free(tmp);

            // Encrypt message
            y = strlen(buffer);
            xor_encrypt(key, buffer, y);
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

void initLog(char logname[]) {
    int result;
    char log_name_default[1024] = "/";
    char log_name_new[1024] = "/";

    // TODO #34 - Check if there are over 5 log files and if so remove the oldest before adding a new one

    strncat(log_name_default, logname, strlen(logname));
    strncat(log_name_new, logname, strlen(logname));
    strncat(log_name_new, "_", 1);

    if (createPaths(log_name_default, log_name_new) == 0)
        exitLogError();

    // TODO #35 - Handle error while copying
    result = rename(log_name_default, log_name_new);

    // Open log file
    fp_l = fopen(log_name_default, "a");

    if (fp_l == NULL)
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

// Encryption routine
void *xor_encrypt(char *key, char *string, int n) {
    int i;
    int keyLength = strlen(key);

    for (i = 0;i < n;i++) {
        string[i] = string[i]^key[i%keyLength];
    }
    return 0;
}

