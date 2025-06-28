SRC_DIR := src/server

SRCS := $(wildcard $(SRC_DIR)/*.c)

OBJS := $(SRCS:.c=.o)

TARGET := server

#CFLAGS := -Wall -Wextra -g

CC := gcc

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^
	#$(CC) $(CFLAGS) -o $@ $^

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)
