CC      := gcc
CFLAGS  := -Wall -Wextra -pthread
TARGET  := satellite_station
SRC     := satellite_station.c

.PHONY: all clean

all: clean run

compile: $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: compile
	./$(TARGET)

clean:
	$(RM) $(TARGET)
