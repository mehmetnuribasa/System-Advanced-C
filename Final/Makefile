CC = gcc
CFLAGS = -Wall -pthread

SERVER_SRC = server/server.c
CLIENT_SRC = client/client.c

SERVER_BIN = chatserver
CLIENT_BIN = chatclient

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
