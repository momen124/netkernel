CC = gcc
CFLAGS = -Wall -g -Iinclude
LDLIBS = -lpthread

SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

all: $(BIN_DIR)/http_server

$(BIN_DIR)/http_server: $(OBJ_DIR)/app/http_server.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/app/http_server.o: $(SRC_DIR)/app/httpServer.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

.PHONY: all clean