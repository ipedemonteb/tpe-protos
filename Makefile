SRC_DIRS := src/server src/server/states src/utils

SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))

OBJS := $(SRCS:.c=.o)

TARGET := server

CC := gcc

# CFLAGS := -Wall -Wextra -g

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
