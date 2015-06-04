/*
=====================================
shhchat - simple encrypted linux chat
=====================================
ssl lib
=====================================
*/

#ifndef SHHCHAT_SSL_H_INCLUDED
#define SHHCHAT_SSL_H_INCLUDED

#include <openssl/bio.h> 
#include <openssl/ssl.h> 
#include <openssl/err.h> 

void initSSL();
void destroySSL();
void closeSSL(SSL *ssl);

void initSSL() {
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
}

void destroySSL() {
    ERR_free_strings();
    EVP_cleanup();
}

void closeSSL(SSL *ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

#endif
