#pragma once
#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <memory>
#include <string>
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
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ASTNode> parse();

private:
    std::vector<Token> tokens;
    size_t index = 0;
    std::unordered_set<std::string> declared_matrices;

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
};

#endif
