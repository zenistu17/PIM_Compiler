// Lexer.h
#ifndef LEXER_H
#define LEXER_H

#include <vector>
#include <string>
#include <unordered_set>

enum TokenType {
    KEYWORD, IDENTIFIER, OPERATOR, NUMBER,
    SYMBOL, END, PREPROCESSOR, TYPE_SPEC, MATRIX_TYPE,
    MATRIX_DECL, MEMORY_OP, SIZE_DEF
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

class Lexer {
public:
    Lexer(const std::string& src);
    std::vector<Token> tokenize();
    int getMatrixSize() const { return matrix_size; }
    
private:
    std::string source;
    size_t index = 0;
    int current_line = 1;
    int matrix_size = 0;
    
    void handlePreprocessor();
    void parseMatrixSize();
    char advance();
    void addToken(std::vector<Token>& tokens, TokenType type, std::string value);
};

#endif // LEXER_H
