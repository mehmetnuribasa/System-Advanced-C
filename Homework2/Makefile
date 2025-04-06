CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = parent
SRC = parent_child.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o
