
#MAKEFLAGS += --silent

PREFIX = $(shell pwd)

BIN_DIR = $(PREFIX)/../bin
SRC_DIR = $(PREFIX)
INC_DIR = $(PREFIX)/../include

CFLAGS = -Wall -g -I$(INC_DIR)

OBJECTS = benchmark.o client.o command.o config.o db.o debug.o dict.o \
			event.o experiment.o log.o net.o obj.o sds.o server.o \
			siphash.o temp.o test.o util.o
			
SRC = $(shell find *.c)

# Generate dependency file for source and header files each time we remake
.depend: $(SRC)
	rm -f ./.depend
	$(CC) $(CFLAGS) -MM $^ > ./.depend

include .depend

# Added target for code::blocks to remake
Debug: all

all:  $(BIN_DIR)/ArenaDB

$(BIN_DIR)/ArenaDB: $(OBJECTS)
	$(CC) -o $(BIN_DIR)/ArenaDB $(OBJECTS)

clean:
	rm -fr $(BIN_DIR)/*
	rm -f $(SRC_DIR)/*.o
	rm -f $(SRC_DIR)/.depend
	
.PHONY: all clean 
