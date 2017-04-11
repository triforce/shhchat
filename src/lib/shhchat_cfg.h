/*
=====================================
shhchat - simple encrypted linux chat
=====================================
config lib
=====================================
*/

#ifndef SHHCHAT_CFG_H_INCLUDED
#define SHHCHAT_CFG_H_INCLUDED

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAXLEN 80
#define CONFIG_FILE "/etc/shhchat/shhchat.conf"

struct parameters {
    char client_simple_key[MAXLEN];
    char client_ssl_key[MAXLEN];
    char client_ssl_cert[MAXLEN];
    char server_simple_key[MAXLEN];
    char server_ssl_key[MAXLEN];
    char server_ssl_cert[MAXLEN];
} parameters;

void init_parameters(struct parameters *params, char *home);
void parse_config(struct parameters *params);

void init_parameters(struct parameters *params, char *home) {
    char key[] = "/.key";
    char ssl_key[] = "/.shh_key";
    char ssl_cert[] = "/.shh_certificate";
    char tmp[50];
    memset(&tmp, 0, sizeof(tmp));

    if (home != NULL) {
        strncpy(tmp, home, strlen(home));
        strncat(tmp, key, sizeof(key));
        strncpy(params->client_simple_key, tmp, MAXLEN);
        memset(&tmp, 0, sizeof(tmp));

        strncpy(tmp, home, strlen(home));
        strncat(tmp, ssl_key, sizeof(ssl_key));
        strncpy(params->client_ssl_key, tmp, MAXLEN);
        memset(&tmp, 0, sizeof(tmp));

        strncpy(tmp, home, strlen(home));
        strncat(tmp, ssl_cert, sizeof(ssl_cert));
        strncpy(params->client_ssl_cert, tmp, MAXLEN);
        memset(&tmp, 0, sizeof(tmp));
    }
    
    strncpy(params->server_simple_key, "/etc/shhchat/key", MAXLEN);
    strncpy(params->server_ssl_key, "/etc/shhchat/shh_key.pem", MAXLEN);
    strncpy(params->server_ssl_cert, "/etc/shhchat/shh_certificate.pem", MAXLEN);
}

char *trim(char *s) {
    char *s1 = s, *s2 = &s[strlen (s) - 1];

    while ((isspace(*s2)) && (s2 >= s1))
        s2--;

    *(s2+1) = '\0';

    while ((isspace(*s1)) && (s1 < s2))
        s1++;

    strcpy(s, s1);
    return s;
}

void parse_config(struct parameters *params) {
    char *s, buff[256];
    FILE *fp = fopen (CONFIG_FILE, "r");

    // Config file location currently not configurable
    if (fp == NULL) {
        return;
    }

    while ((s = fgets(buff, sizeof buff, fp)) != NULL) {
        if (buff[0] == '\n' || buff[0] == '#')
            continue;

        char name[MAXLEN], value[MAXLEN];
        s = strtok(buff, "=");
    
        if (s == NULL)
            continue;
        else
            strncpy(name, s, MAXLEN);
      
        s = strtok (NULL, "=");
    
        if (s == NULL)
            continue;
        else
            strncpy(value, s, MAXLEN);
      
        trim(value);

        if (strcmp(name, "client_simple_key") == 0)
            strncpy(params->client_simple_key, value, MAXLEN);
        else if (strcmp(name, "client_ssl_key") == 0)
            strncpy(params->client_ssl_key, value, MAXLEN);
        else if (strcmp(name, "client_ssl_cert") == 0)
            strncpy(params->client_ssl_cert, value, MAXLEN);
        else if (strcmp(name, "server_simple_key") == 0)
            strncpy(params->server_simple_key, value, MAXLEN);
        else if (strcmp(name, "server_ssl_key") == 0)
            strncpy(params->server_ssl_key, value, MAXLEN);
        else if (strcmp(name, "server_ssl_cert") == 0)
            strncpy(params->server_ssl_cert, value, MAXLEN);
        else
            printf("Error: %s/%s: Invalid config detected - \n", name, value);
    }

    fclose(fp);
}

#endif
