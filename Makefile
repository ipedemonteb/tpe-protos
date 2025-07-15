SRC_DIRS := src/server src/server/states src/utils src/monitor

# Servidores principales (SOCKS5 y servidor monitoreo)
SERVER_SRCS := $(wildcard src/server/*.c) $(wildcard src/server/states/*.c) $(wildcard src/utils/*.c) $(filter-out src/monitor/monitor_client.c, $(wildcard src/monitor/*.c))
SERVER_OBJS := $(SERVER_SRCS:.c=.o)

# Monitoreo (cliente)
CLIENT_SRCS := src/monitor/monitor_client.c $(wildcard src/utils/*.c) src/server/buffer.c src/server/args.c
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

TARGET := server
CLIENT_TARGET := monitor_client

CC := gcc
CFLAGS := -Wall -Wextra -g -fsanitize=address
LDFLAGS := -fsanitize=address

all: $(TARGET) $(CLIENT_TARGET)

$(TARGET): $(SERVER_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) 

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) $(TARGET) $(CLIENT_TARGET)

.PHONY: all clean
