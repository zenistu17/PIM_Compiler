#include "Lexer.h"
#include <cctype>
#include <algorithm>

Lexer::Lexer(const std::string& src) : source(src) {}

void Lexer::handlePreprocessor() {
    std::string directive;
    while (index < source.size() && std::isalpha(source[index])) {
        directive += source[index++];
    }
    
    if (directive == "define") {
        parseMatrixSize();
    }
}

void Lexer::parseMatrixSize() {
    while (index < source.size() && std::isspace(source[index])) index++;
    
    std::string ident;
    while (index < source.size() && (std::isalnum(source[index]) || source[index] == '_')) {
        ident += source[index++];
    }
    
    // Only track matrix size definitions
    if (ident == "N" || ident == "SIZE" || ident == "ROWS" || ident == "COLS" || ident == "INNER") {
        while (index < source.size() && !std::isdigit(source[index])) index++;
        
        std::string num;
        while (index < source.size() && std::isdigit(source[index])) {
            num += source[index++];
        }
        
        if (!num.empty()) {
            matrix_size = std::max(matrix_size, std::stoi(num));
        }
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (index < source.size()) {
        char c = source[index];
        
        if (c == '#') {
            addToken(tokens, PREPROCESSOR, "#");
            index++;
            handlePreprocessor();
            continue;
        }
        
        if (std::isspace(c)) {
            if (c == '\n') current_line++;
            index++;
            continue;
        }
        
        // In Lexer.cpp, modify the identifier handling section:
        if (std::isalpha(c) || c == '_') {
            std::string ident;
            while (index < source.size() && (std::isalnum(c) || c == '_')) {
                ident += c;
                c = source[++index];
            }
            // Mark as matrix type if it's a known matrix type
            if (ident == "int" || ident == "float" || ident == "double") {
                addToken(tokens, MATRIX_TYPE, ident);
            }
            // Add the new MATRIX type check here:
            else if (ident == "MATRIX") {
                addToken(tokens, MATRIX_TYPE, ident);
            }
            // Mark as matrix declaration if it's a single uppercase letter
            else if (ident.length() == 1 && isupper(ident[0])) {
                addToken(tokens, MATRIX_DECL, ident);
            } else {
                addToken(tokens, IDENTIFIER, ident);
            }
            continue;
        }
        
        if (std::isdigit(c)) {
            std::string num;
            while (index < source.size() && std::isdigit(c)) {
                num += c;
                c = source[++index];
            }
            addToken(tokens, NUMBER, num);
            continue;
        }
        
        else if (c == '[') {
            // Skip entire array declaration [N]
            while (index < source.size() && c != ']') {
                c = source[++index];
            }
            if (c == ']') index++; // Skip closing bracket
            continue;
        }
        
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=') {
            addToken(tokens, OPERATOR, std::string(1, c));
            index++;
            continue;
        }
        
        addToken(tokens, SYMBOL, std::string(1, c));
        index++;
    }
    
    addToken(tokens, END, "");
    return tokens;
}

void Lexer::addToken(std::vector<Token>& tokens, TokenType type, std::string value) {
    tokens.push_back({type, value, current_line});
}
