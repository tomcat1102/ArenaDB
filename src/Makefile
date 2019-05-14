
#MAKEFLAGS += --silent

PREFIX = $(shell pwd)

BIN_DIR = $(PREFIX)/../bin
SRC_DIR = $(PREFIX)
INC_DIR = $(PREFIX)/../include

CFLAGS = -Wall -g -I$(INC_DIR)

SRC = $(shell find *.c)
OBJECTS = $(patsubst %.c, %.o, $(SRC))

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
