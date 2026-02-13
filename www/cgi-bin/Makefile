# Makefile for main.cgi
CC?=gcc
CFLAGS?=-O2 -Wall -Wextra
LDFLAGS?=
LIBS=-ljson-c

ZIGBEE_DIR?=/development
ifeq ($(wildcard $(ZIGBEE_DIR)/src/zigbee_mq.c),)
ZIGBEE_DIR:=../../development
endif
CFLAGS+=-I./includes -I./src -I$(ZIGBEE_DIR)/includes

SRCS:=main.c $(wildcard src/*.c) $(ZIGBEE_DIR)/src/zigbee_mq.c
TARGET:=/www/cgi-bin/main.cgi

.PHONY:all clean test-local

all:$(TARGET)

$(TARGET):$(SRCS)
	@echo "Compiling main.cgi from $^"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	@chmod 755 $@
	@echo "Built:$@"

clean:
	rm -f $(TARGET)

test-local:TARGET:=./main.cgi
test-local:$(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $^ $(LIBS)
	./$(TARGET)
