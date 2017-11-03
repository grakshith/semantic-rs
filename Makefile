CC=gcc
LDFLAGS=-lfl -lm
BIN_DIR=bin
BUILD_DIR=build

FLEX=flex

all: lexer

lexer: $(BUILD_DIR)/lexer_main.o $(BUILD_DIR)/lexer.o $(BUILD_DIR)/tokens.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

$(BUILD_DIR)/lexer_main.o: lexer_main.c
	$(CC) -c -o $@ $<

$(BUILD_DIR)/tokens.o: tokens.c
	$(CC) -std=c99 -c -o $@ $<

lex.yy.c: tokens.l
	$(FLEX) $<

$(BUILD_DIR)/lexer.o: lex.yy.c tokens.h
	$(CC) -c -o $@ $<

clean:
	rm -f $(BIN_DIR)/* $(BUILD_DIR)/* lex.yy.c
