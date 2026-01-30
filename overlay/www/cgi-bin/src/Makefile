# Makefile for main.cgi
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra
LDFLAGS ?=
LIBS    = -ljson-c
ZIGBEE_DIR := /development
CFLAGS += -I$(ZIGBEE_DIR)/includes

LOCAL_SRCS   := $(wildcard *.c)
SRCS         := $(LOCAL_SRCS) $(ZIGBEE_DIR)/src/zigbee_mq.c
TARGET := /www/cgi-bin/main.cgi

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@echo "Compiling main.cgi from $^"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	@chmod 755 $@
	@echo "Built: $@"

clean:
	rm -f $(TARGET)

# 方便本地开发测试（非 chroot 环境）
test-local: TARGET := ./main.cgi
test-local: $(SRCS)
	gcc $(CFLAGS) -o $(TARGET) $^ $(LIBS)
	./$(TARGET)
