# semantic-rs
Naive implementation of type checking for the rust language. Builds a syntax tree and symbol table for analysis.

## Compilation
1. `$ mkdir bin build`
2. `$ make`

The lexer and parser will be built at the `bin` directory.

### lexer
The input is read from `stdin`. Outputs the tokens recognized by the lexer.

### parser
-  `$./parser  < ../inp1.txt`  
Prints only the semantic errors
-  `$./parser -v <../inp1.txt`  
Verbose switch prints the parse tree also
