
CC = gcc
CFLAGS = -Iincludes -pthread
LDFLAGS = -lcurl -ljson-c -pthread
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = environment

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean