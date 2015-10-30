/*
=====================================
shhchat - simple encrypted linux chat
=====================================
websocket lib
=====================================
*/

#ifndef SHHCHAT_WS_H_INCLUDED
#define SHHCHAT_WS_H_INCLUDED

static int callback_http(
    struct libwebsocket_context * this,
    struct libwebsocket *wsi,
    enum libwebsocket_callback_reasons reason,
    void *user,
    void *in, size_t len) {
        return 0;
}

static int handle_ws_requests(
    struct libwebsocket_context * this,
    struct libwebsocket *wsi,
    enum libwebsocket_callback_reasons reason,
    void *user, void *in, size_t len) {
        switch (reason) {
            case LWS_CALLBACK_ESTABLISHED:
                printf("Connected\n");
                break;
            case LWS_CALLBACK_RECEIVE: {
                unsigned char *buf = (unsigned char*) malloc(LWS_SEND_BUFFER_PRE_PADDING + len +
                                                             LWS_SEND_BUFFER_POST_PADDING);

                int i;

                for (i=0;i < len;i++) {
                    buf[LWS_SEND_BUFFER_PRE_PADDING + (len - 1) - i ] = ((char *) in)[i];
                }

                printf("Received data: %s, replying: %.*s\n", (char *) in, (int) len,
                     buf + LWS_SEND_BUFFER_PRE_PADDING);

                libwebsocket_write(wsi, &buf[LWS_SEND_BUFFER_PRE_PADDING], len, LWS_WRITE_TEXT);

                free(buf);
                break;
            }
            default:
                break;
        }

    return 0;
}

static struct libwebsocket_protocols protocols[] = {
    {
        "http-only",
        callback_http,
        0
    },
    {
        "shhchat-protocol",
        handle_ws_requests,
        0

    },
    {
        NULL, NULL, 0
    }
};
#endif