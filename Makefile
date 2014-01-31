CC=gcc
CFLAGS=-lpthread --std=gnu99
SRC=src
BIN=bin

# Check for Linux environment
# ENV:=$(shell uname -s | cut -d _ -f1)

all:
	$(CC) -o $(BIN)/server $(SRC)/server.c $(CFLAGS)
	$(CC) -o $(BIN)/client $(SRC)/client.c $(CFLAGS)
	@echo Finished shhchat build

clean:
	rm -f $(BIN)/server $(BIN)/client
	@echo Finished clean
