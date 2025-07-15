include ./Makefile.inc

SERVER_SOURCES=$(wildcard src/server/*.c) $(wildcard src/server/states/*.c) $(wildcard src/server/utils/*.c)
MONITOR_SOURCES=src/client/monitor_client.c
SHARED_SOURCES=$(wildcard src/shared/*.c)

OUTPUT_FOLDER=./bin
OBJECTS_FOLDER=./obj

SERVER_OBJECTS=$(SERVER_SOURCES:src/%.c=obj/%.o)
MONITOR_OBJECTS=$(MONITOR_SOURCES:src/%.c=obj/%.o)
SHARED_OBJECTS=$(SHARED_SOURCES:src/%.c=obj/%.o)

SERVER_OUTPUT_FILE=$(OUTPUT_FOLDER)/server
MONITOR_OUTPUT_FILE=$(OUTPUT_FOLDER)/monitor_client

all: server monitor_client

server: $(SERVER_OUTPUT_FILE)
monitor_client: $(MONITOR_OUTPUT_FILE)

$(SERVER_OUTPUT_FILE): $(SERVER_OBJECTS) $(SHARED_OBJECTS)
	mkdir -p $(OUTPUT_FOLDER)
	$(COMPILER) $(LDFLAGS) $(SERVER_OBJECTS) $(SHARED_OBJECTS) -o $(SERVER_OUTPUT_FILE)

$(MONITOR_OUTPUT_FILE): $(MONITOR_OBJECTS) $(SHARED_OBJECTS)
	mkdir -p $(OUTPUT_FOLDER)
	$(COMPILER) $(LDFLAGS) $(MONITOR_OBJECTS) $(SHARED_OBJECTS) -o $(MONITOR_OUTPUT_FILE)

clean:
	rm -rf $(OUTPUT_FOLDER)
	rm -rf $(OBJECTS_FOLDER)

obj/%.o: src/%.c
	mkdir -p $(OBJECTS_FOLDER)/server
	mkdir -p $(OBJECTS_FOLDER)/server/states
	mkdir -p $(OBJECTS_FOLDER)/server/utils
	mkdir -p $(OBJECTS_FOLDER)/client
	mkdir -p $(OBJECTS_FOLDER)/shared
	$(COMPILER) $(COMPILERFLAGS) -c $< -o $@

.PHONY: all server monitor_client clean
