CC=gcc
LDFLAGS=-lfl -lm
BIN_DIR=bin
BUILD_DIR=build

FLEX ?= flex
BISON ?= bison

all: lexer parser

lexer: $(BUILD_DIR)/lexer_main.o $(BUILD_DIR)/lexer.o $(BUILD_DIR)/tokens.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

$(BUILD_DIR)/lexer_main.o: lexer_main.c
	$(CC) -c -o $@ $<

$(BUILD_DIR)/tokens.o: tokens.c
	$(CC) -std=c99 -c -o $@ $<

lex.yy.c: tokens.l
	$(FLEX) $<

$(BUILD_DIR)/lexer.o: lex.yy.c tokens.h
	$(CC) -include tokens.h -c -o $@ $<

parser: $(BUILD_DIR)/parser.o $(BUILD_DIR)/parser_main.o $(BUILD_DIR)/lexer_p.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

$(BUILD_DIR)/parser.o: parser.tab.c
	$(CC) -c -o $@ $^

$(BUILD_DIR)/parser_main.o: parser_main.c
	$(CC) -std=c99 -c -o $@ $<

$(BUILD_DIR)/lexer_p.o: lex.yy.c parser.tab.h
	$(CC) -include parser.tab.h -c -o $@ $<

parser.tab.c: parser.y
	$(BISON) $< -d -p rs -v --report=all --warnings=error=all

clean:
	rm -f $(BIN_DIR)/* $(BUILD_DIR)/* lex.yy.c parser.tab.c parser.tab.h parser.output
