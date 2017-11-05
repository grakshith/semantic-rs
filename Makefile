CC=gcc
CXX=g++
CXXFLAGS= -Wno-write-strings -std=c++11 -g
# CXXFLAGS = 
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
	$(CXX) -o $(BIN_DIR)/$@ $^ $(CXXFLAGS) $(LDFLAGS)

$(BUILD_DIR)/parser.o: parser.tab.cc
	$(CXX) -c -o $@ $^ $(CXXFLAGS)

$(BUILD_DIR)/parser_main.o: parser_main.cc
	$(CXX) -c -o $@ $< $(CXXFLAGS)

$(BUILD_DIR)/lexer_p.o: lex.yy.c parser.tab.hh
	$(CXX) -include parser.tab.hh -c -o $@ $< $(CXXFLAGS)

parser.tab.cc parser.tab.hh: parser.y
	$(BISON) -o $@ $< -d -p rs -v --report=all --warnings=error=all

clean:
	rm -f $(BIN_DIR)/* $(BUILD_DIR)/* lex.yy.c parser.tab.cc parser.tab.hh parser.output
