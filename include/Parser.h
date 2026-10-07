#pragma once
#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "Lexer.h"

enum ASTNodeType {
    PROGRAM_NODE,
    FUNCTION_NODE,
    MEMORY_OP_NODE,
    MATRIX_DECL_NODE,
    MATRIX_OP_NODE
};

struct ASTNode {
    ASTNodeType type;
    std::string value;
    std::vector<std::unique_ptr<ASTNode>> children;
    int line = 0;
    // For matrix declarations, preserve the dimensions exactly as written
    // (e.g. {"ROWS", "COLS"}).  The backend may resolve symbolic dimensions
    // through preprocessor definitions; retaining them here avoids throwing
    // away rectangular shape information during lexing.
    std::vector<std::string> dimensions;
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ASTNode> parse();

private:
    std::vector<Token> tokens;
    size_t index = 0;
    std::unordered_set<std::string> declared_matrices;
    std::unordered_map<std::string, std::vector<std::string>> matrix_dimensions;

    const Token& current() const;
    void advance();
    bool match(TokenType type) const;
    bool check(TokenType type) const;

    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseMatrixDeclaration();
    std::unique_ptr<ASTNode> parseMatrixOperation();
    std::unique_ptr<ASTNode> parseFunction();
    void parseFunctionBody(ASTNode* funcNode);
    void skipToNextFunction();

    bool isSymbol(const char* value) const;
    bool isFunctionStart(size_t at) const;
    std::unique_ptr<ASTNode> makeMatrixNode(const std::string& name, int line,
                                            const std::vector<std::string>& dimensions = {}) const;
    static bool isMatrixTypeToken(const Token& token);
    static bool isIdentifierToken(const Token& token);
    static bool isIndexedMatrix(const std::vector<Token>& body, size_t at);
    static std::vector<std::string> extractDimensions(const std::vector<Token>& body,
                                                      size_t at);
    void parseStatementsInBody(const std::vector<Token>& body, ASTNode* funcNode);
};

#endif
