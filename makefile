
#MAKEFLAGS += --silent

#PREFIX = $(shell pwd)

BIN_DIR = bin
OBJ_DIR = obj
SRC_DIR = src
INC_DIR = include


			
CFLAGS = -Wall -g -I$(INC_DIR)

_OBJECTS = benchmark.o client.o command.o config.o db.o debug.o dict.o \
			event.o experiment.o log.o net.o obj.o sds.o server.o \
			siphash.o temp.o test.o util.o
OBJECTS = $(patsubst %, $(OBJ_DIR)/%, $(_OBJECTS))

all: ArenaDB

ArenaDB: $(_OBJECTS)
	$(CC) -o $(BIN_DIR)/ArenaDB $(OBJECTS)

# Implicit rules
%.c: 
	
%.h:
	 
%.o: %.c
	$(CC) -c $(CFLAGS) $(SRC_DIR)/$< -o $(OBJ_DIR)/$@

clean:
	@echo "clean"
	rm obj/*
	rm bin/*
	




.PHONY: all clean
