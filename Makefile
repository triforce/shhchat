CC=gcc
CFLAGS=-lpthread -lssl -lcrypto --std=gnu99 -I/usr/include/openssl/
SRC=src
BIN=build
CONF=conf
WS=-lwebsockets

# Check for Linux environment
ENV:=$(shell uname -s | cut -d _ -f1)

define os_check =
	$(if $(filter-out $(ENV), Linux), exit 1)
endef

all:
	$(os_check)
	@echo Starting shhchat build
	rm -f $(BIN)/shhchatd $(BIN)/shhclient
	$(CC) -o $(BIN)/shhchatd $(SRC)/shhchatd/server.c $(CFLAGS) -DVERSION='"_beta"'
	$(CC) -o $(BIN)/shhclient $(SRC)/chatclient/client.c $(CFLAGS) -DVERSION='"_beta"'
	rm -rf $(BIN)/$(CONF)
	mkdir $(BIN)/$(CONF)
	cp $(CONF)/* -t $(BIN)/$(CONF)
	@echo Finished shhchat build

clean:
	rm -f $(BIN)/shhchatd $(BIN)/shhclient
	@echo Finished clean

debug:
	$(os_check)
	@echo Starting shhchat build
	rm -f $(BIN)/shhchatd $(BIN)/shhclient
	$(CC) -o $(BIN)/shhchatd $(SRC)/shhchatd/server.c $(CFLAGS) -DDEBUG -g
	$(CC) -o $(BIN)/shhclient $(SRC)/chatclient/client.c $(CFLAGS) -DDEBUG
	rm -rf $(BIN)/$(CONF)
	mkdir $(BIN)/$(CONF)
	cp $(CONF)/* -t $(BIN)/$(CONF)
	@echo Finished shhchat debug build
