CC = gcc
CFLAGS = -Wall -g -pthread

SERVER_DIR = server
CLIENT_DIR = client

SERVER_SRC = $(SERVER_DIR)/chatserver.c
CLIENT_SRC = $(CLIENT_DIR)/chatclient.c

SERVER_BIN = chatserver
CLIENT_BIN = chatclient

TARGETS = $(SERVER_BIN) $(CLIENT_BIN)

all: $(TARGETS)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) $< -o $@

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)
