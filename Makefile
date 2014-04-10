CC=gcc
CFLAGS=-lpthread --std=gnu99
SRC=src
BIN=build
CONF=conf

# Check for Linux environment
# ENV:=$(shell uname -s | cut -d _ -f1)

all:
	rm -f $(BIN)/shhchatd $(BIN)/shhclient
	$(CC) -o $(BIN)/shhchatd $(SRC)/shhchatd/server.c $(CFLAGS)
	$(CC) -o $(BIN)/shhclient $(SRC)/chatclient/client.c $(CFLAGS)
	cp $(CONF)/* -t $(BIN)/cfg
	@echo Finished shhchat build

clean:
	rm -f $(BIN)/shhchatd $(BIN)/shhclient
	@echo Finished clean
