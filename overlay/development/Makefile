CC = gcc
CFLAGS = -Iincludes -pthread
LDFLAGS = -lcurl -ljson-c -pthread
SRC = src/main.c src/environment.c src/check_task.c
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