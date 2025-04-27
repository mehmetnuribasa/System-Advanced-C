CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -lrt

all: bank_server bank_client

bank_server: bank_server.c
	$(CC) $(CFLAGS) -o bank_server bank_server.c $(LDFLAGS)

bank_client: bank_client.c
	$(CC) $(CFLAGS) -o bank_client bank_client.c $(LDFLAGS)

clean:
	rm -f bank_server bank_client *.o AdaBank.bankLog

test: all
	@echo "Creating test client files..."
	@echo "N deposit 300" > Client01.file
	@echo "BankID_None withdraw 30" >> Client01.file
	@echo "N deposit 2000" >> Client01.file
	@echo "BankID_01 withdraw 300" > Client02.file
	@echo "N deposit 20" >> Client02.file
	@echo "BankID_02 withdraw 30" > Client03.file
	@echo "N deposit 2000" >> Client03.file
	@echo "BankID_02 deposit 200" >> Client03.file
	@echo "BankID_02 withdraw 300" >> Client03.file
	@echo "N withdraw 20" >> Client03.file
	@echo "Test client files created."

.PHONY: all clean test 