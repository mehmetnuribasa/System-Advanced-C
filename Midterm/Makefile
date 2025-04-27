CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -lrt

all: bank_server bank_client

bank_server: bank_server.c
	$(CC) $(CFLAGS) -o bank_server bank_server.c $(LDFLAGS)

bank_client: bank_client.c
	$(CC) $(CFLAGS) -o bank_client bank_client.c $(LDFLAGS)

clean:
	rm -f bank_server bank_client *.o

run: all
	@chmod +x test_multiple_clients.sh
	@./test_multiple_clients.sh

.PHONY: clean all run 