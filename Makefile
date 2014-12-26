CC=gcc
CFLAGS=-lpthread --std=gnu99
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
	rm -f $(BIN)/shhchatd $(BIN)/shhclient
	$(CC) -o $(BIN)/shhchatd $(SRC)/shhchatd/server.c $(CFLAGS) -DVERSION='"_beta"'
	$(CC) -o $(BIN)/shhclient $(SRC)/chatclient/client.c $(CFLAGS) -DVERSION='"_beta"'
	cp $(CONF)/* -t $(BIN)/cfg
	@echo Finished shhchat build

clean:
	rm -f $(BIN)/shhchatd $(BIN)/shhclient
	@echo Finished clean

debug:
	$(os_check)
	rm -f $(BIN)/shhchatd $(BIN)/shhclient
	$(CC) -o $(BIN)/shhchatd $(SRC)/shhchatd/server.c $(CFLAGS) -DDEBUG -g
	$(CC) -o $(BIN)/shhclient $(SRC)/chatclient/client.c $(CFLAGS) -DDEBUG
	cp $(CONF)/* -t $(BIN)/cfg
	@echo Finished shhchat debug build
