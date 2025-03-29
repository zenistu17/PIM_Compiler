#include "Parser.h"
#include <iostream>
#include <stdexcept>
#include <stack>

using namespace std;

Parser::Parser(const vector<Token>& tokens) : tokens(tokens) {}

const Token& Parser::current() const {
    if (index >= tokens.size()) {
        throw runtime_error("Unexpected end of input");
    }
    return tokens[index];
}

void Parser::advance() {
    if (index < tokens.size()) index++;
}

bool Parser::match(TokenType type) const {
    return index < tokens.size() && current().type == type;
}

bool Parser::check(TokenType type) const {
    return index < tokens.size() && tokens[index].type == type;
}

unique_ptr<ASTNode> Parser::parse() {
    auto program = make_unique<ASTNode>();
    program->type = PROGRAM_NODE;
    program->value = "Program";
    
    // First pass: identify function declarations and matrix declarations
    while (index < tokens.size() && !check(END)) {
        try {
            // Look for function definitions like "void multiply(...)"
            if (match(IDENTIFIER) && current().value == "void") {
                advance();
                if (match(IDENTIFIER)) {
                    string funcName = current().value;
                    advance();
                    
                    // Check if this is a matrix multiplication function
                    if (funcName == "multiply" || funcName == "matmul" || funcName == "matrix_multiply") {
                        auto funcNode = parseFunction();
                        if (funcNode) {
                            funcNode->value = funcName;
                            program->children.push_back(std::move(funcNode));
                        }
                    } else {
                        // Skip other functions
                        skipToNextFunction();
                    }
                }
            } else if (auto stmt = parseStatement()) {
                program->children.push_back(std::move(stmt));
            } else {
                advance(); // Skip unrecognized tokens
            }
        } catch (const runtime_error& e) {
            cerr << "Parse error at token " << index << " ('"
                 << current().value << "'): " << e.what() << endl;
            advance();
        }
    }
    
    return program;
}

void Parser::skipToNextFunction() {
    // Skip until we find an opening brace
    while (index < tokens.size() && !(match(SYMBOL) && current().value == "{")) {
        advance();
    }
    
    // Skip the function body (including nested braces)
    if (index < tokens.size()) {
        advance(); // Skip the opening brace
        int braceCount = 1;
        
        while (index < tokens.size() && braceCount > 0) {
            if (match(SYMBOL)) {
                if (current().value == "{") braceCount++;
                else if (current().value == "}") braceCount--;
            }
            advance();
        }
    }
}

unique_ptr<ASTNode> Parser::parseFunction() {
    auto funcNode = make_unique<ASTNode>();
    funcNode->type = FUNCTION_NODE;
    funcNode->line = current().line;
    
    // Skip to opening parenthesis
    while (index < tokens.size() && !(match(SYMBOL) && current().value == "(")) {
        advance();
    }
    
    if (match(SYMBOL) && current().value == "(") {
        advance();
        
        // Parse function parameters (matrix arguments)
        while (index < tokens.size() && !(match(SYMBOL) && current().value == ")")) {
            // Look for matrix declarations in parameters
            if (match(MATRIX_TYPE) ||
                (match(IDENTIFIER) && (current().value == "int" ||
                                     current().value == "float" ||
                                     current().value == "double"))) {
                advance();
                
                if (match(IDENTIFIER) || match(MATRIX_DECL)) {
                    string matrixName = current().value;
                    
                    // Add matrix declaration to the function node
                    auto matrixNode = make_unique<ASTNode>();
                    matrixNode->type = MATRIX_DECL_NODE;
                    matrixNode->value = matrixName;
                    matrixNode->line = current().line;
                    
                    funcNode->children.push_back(std::move(matrixNode));
                    
                    // Skip array dimensions
                    while (index < tokens.size() &&
                           !(match(SYMBOL) && (current().value == "," || current().value == ")"))) {
                        advance();
                    }
                }
            } else {
                advance();
            }
        }
        
        // Skip to function body
        while (index < tokens.size() && !(match(SYMBOL) && current().value == "{")) {
            advance();
        }
        
        if (match(SYMBOL) && current().value == "{") {
            advance();
            
            // Now parse the function body for matrix operations
            parseFunctionBody(funcNode.get());
        }
    }
    
    return funcNode;
}

void Parser::parseFunctionBody(ASTNode* funcNode) {
    // Look for nested loops (typical matrix multiplication pattern)
    stack<int> loopNestingLevel;
    bool foundTripleLoop = false;
    int currentLine = -1;
    
    while (index < tokens.size()) {
        if (match(SYMBOL) && current().value == "}") {
            if (loopNestingLevel.empty()) {
                // End of function
                advance();
                break;
            } else {
                // End of a loop
                loopNestingLevel.pop();
                advance();
            }
        }
        else if (match(IDENTIFIER) && current().value == "for") {
            // Found a loop
            currentLine = current().line;
            loopNestingLevel.push(currentLine);
            advance();
            
            // If we've found three nested loops, this might be matrix multiplication
            if (loopNestingLevel.size() == 3) {
                foundTripleLoop = true;
                
                // Improved detection: Look ahead for matrix multiplication pattern
                int tempIndex = index;
                int maxLookAhead = 50; // Limit how far we look ahead
                
                while (maxLookAhead > 0 && tempIndex < tokens.size()) {
                    // Look for += or = operators inside the innermost loop
                    if ((tokens[tempIndex].type == OPERATOR &&
                         (tokens[tempIndex].value == "+=" || tokens[tempIndex].value == "=")) ||
                        (tokens[tempIndex].type == SYMBOL && tokens[tempIndex].value == ";")) {
                        
                        // This is a potential matrix multiplication statement
                        break;
                    }
                    tempIndex++;
                    maxLookAhead--;
                }
            }
        }
        else if (foundTripleLoop &&
                (match(OPERATOR) && (current().value == "+=" || current().value == "="))) {
            // Inside triply-nested loop with += or = operation, likely matrix multiply
            string operation = current().value;
            advance();
            
            // Extract the matrices involved
            string resultMatrix;
            string matrixA;
            string matrixB;
            
            // Find the result matrix (left side of += or =)
            int backup = 5; // Look back a few tokens
            for (int i = 1; i <= backup && index - i >= 0; i++) {
                Token prevToken = tokens[index - i];
                if ((prevToken.type == MATRIX_DECL || prevToken.type == IDENTIFIER) &&
                    prevToken.value.length() == 1 && isupper(prevToken.value[0])) {
                    resultMatrix = prevToken.value;
                    break;
                }
            }
            
            // Now process the right side for A*B pattern
            bool foundMultiply = false;
            while (index < tokens.size() && !(match(SYMBOL) && current().value == ";")) {
                if (match(MATRIX_DECL) || match(IDENTIFIER)) {
                    if (current().value.length() == 1 && isupper(current().value[0])) {
                        if (matrixA.empty()) {
                            matrixA = current().value;
                        } else {
                            matrixB = current().value;
                        }
                    }
                } else if (match(OPERATOR) && current().value == "*") {
                    foundMultiply = true;
                }
                advance();
            }
            
            // Create a matrix multiplication node with proper structure
            if (foundMultiply && !resultMatrix.empty() && !matrixA.empty() && !matrixB.empty()) {
                auto matMulNode = make_unique<ASTNode>();
                matMulNode->type = MATRIX_OP_NODE;
                matMulNode->value = "*";
                matMulNode->line = currentLine;
                
                // Add operand A
                auto opANode = make_unique<ASTNode>();
                opANode->type = MATRIX_DECL_NODE;
                opANode->value = matrixA;
                matMulNode->children.push_back(std::move(opANode));
                
                // Add operand B
                auto opBNode = make_unique<ASTNode>();
                opBNode->type = MATRIX_DECL_NODE;
                opBNode->value = matrixB;
                matMulNode->children.push_back(std::move(opBNode));
                
                // Add result matrix
                auto resultNode = make_unique<ASTNode>();
                resultNode->type = MATRIX_DECL_NODE;
                resultNode->value = resultMatrix;
                matMulNode->children.push_back(std::move(resultNode));
                
                funcNode->children.push_back(std::move(matMulNode));
                
                // Reset to prepare for next operation
                foundTripleLoop = false;
            }
        } else {
            advance();
        }
    }
}

unique_ptr<ASTNode> Parser::parseStatement() {
    if (check(PREPROCESSOR)) {
        advance();
        return make_unique<ASTNode>(ASTNode{MEMORY_OP_NODE, "#define", {}, current().line});
    }
    
    if (check(MATRIX_DECL)) {
        return parseMatrixDeclaration();
    }
    
    if (check(OPERATOR) && current().value == "*") {
        return parseMatrixOperation();
    }
    
    // Default case - skip unrecognized tokens
    advance();
    return nullptr;
}

unique_ptr<ASTNode> Parser::parseMatrixDeclaration() {
    auto node = make_unique<ASTNode>();
    node->type = MATRIX_DECL_NODE;
    node->line = current().line;
    
    // Skip type specifier
    while (index < tokens.size() && tokens[index].type != MATRIX_DECL) {
        advance();
    }
    
    // Get matrix name (skip duplicate checks)
    if (match(MATRIX_DECL)) {
        string name = current().value;
        declared_matrices.insert(name); // Always insert without warning
        node->children.push_back(make_unique<ASTNode>(ASTNode{
            MATRIX_DECL_NODE, name, {}, current().line
        }));
        advance();
    }
    
    // Skip array dimensions
    while (index < tokens.size() && !(match(SYMBOL) && current().value == ";")) {
        advance();
    }
    if (index < tokens.size()) advance(); // Skip semicolon
    
    return node;
}

unique_ptr<ASTNode> Parser::parseMatrixOperation() {
    auto node = make_unique<ASTNode>();
    node->type = MATRIX_OP_NODE;
    node->value = current().value;
    node->line = current().line;
    advance();
    
    // Get left operand
    if (match(MATRIX_DECL) || match(IDENTIFIER)) {
        node->children.push_back(make_unique<ASTNode>(ASTNode{
            MATRIX_DECL_NODE, current().value, {}, current().line
        }));
        advance();
    }
    
    // Get operator (already have it in node->value)
    
    // Get right operand
    if (match(MATRIX_DECL) || match(IDENTIFIER)) {
        node->children.push_back(make_unique<ASTNode>(ASTNode{
            MATRIX_DECL_NODE, current().value, {}, current().line
        }));
        advance();
    }
    
    // Get result operand if present (for assignments)
    if (match(SYMBOL) && current().value == "=") {
        advance();
        if (match(MATRIX_DECL) || match(IDENTIFIER)) {
            node->children.push_back(make_unique<ASTNode>(ASTNode{
                MATRIX_DECL_NODE, current().value, {}, current().line
            }));
            advance();
        }
    }
    
    return node;
}
