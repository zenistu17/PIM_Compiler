#include "Parser.h"

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <unordered_set>

namespace {
bool isOpenBrace(const Token& token) {
    return token.type == SYMBOL && token.value == "{";
}

bool isCloseBrace(const Token& token) {
    return token.type == SYMBOL && token.value == "}";
}

bool isBoundary(const Token& token) {
    return token.type == SYMBOL &&
           (token.value == ";" || token.value == "{" || token.value == "}");
}
} // namespace

Parser::Parser(const std::vector<Token>& input) : tokens(input) {}

const Token& Parser::current() const {
    if (index >= tokens.size()) {
        throw std::runtime_error("Unexpected end of input");
    }
    return tokens[index];
}

void Parser::advance() {
    if (index < tokens.size()) {
        ++index;
    }
}

bool Parser::match(TokenType type) const {
    return index < tokens.size() && tokens[index].type == type;
}

bool Parser::check(TokenType type) const {
    return match(type);
}

bool Parser::isSymbol(const char* value) const {
    return index < tokens.size() && tokens[index].type == SYMBOL &&
           tokens[index].value == value;
}

bool Parser::isMatrixTypeToken(const Token& token) {
    return token.type == MATRIX_TYPE ||
           (token.type == IDENTIFIER &&
            (token.value == "int" || token.value == "float" ||
             token.value == "double"));
}

bool Parser::isIdentifierToken(const Token& token) {
    return token.type == IDENTIFIER || token.type == MATRIX_DECL;
}

bool Parser::isFunctionStart(size_t at) const {
    if (at + 2 >= tokens.size()) {
        return false;
    }
    const Token& returnType = tokens[at];
    const Token& name = tokens[at + 1];
    const Token& open = tokens[at + 2];
    // A function call such as multiply(A, B, C) has the same local token
    // shape as "Type multiply(...)" if every identifier is accepted as a
    // return type. Restrict this recognition to C/C++ type words understood by
    // this frontend; arbitrary function names remain supported.
    const bool validReturn = isMatrixTypeToken(returnType) ||
                             (returnType.type == IDENTIFIER &&
                              (returnType.value == "void" ||
                               returnType.value == "bool" ||
                               returnType.value == "char" ||
                               returnType.value == "short" ||
                               returnType.value == "long" ||
                               returnType.value == "auto" ||
                               returnType.value == "size_t"));
    return validReturn && isIdentifierToken(name) &&
           open.type == SYMBOL && open.value == "(";
}

std::unique_ptr<ASTNode> Parser::makeMatrixNode(
    const std::string& name, int line,
    const std::vector<std::string>& dimensions) const {
    auto node = std::make_unique<ASTNode>();
    node->type = MATRIX_DECL_NODE;
    node->value = name;
    node->line = line;
    if (!dimensions.empty()) {
        node->dimensions = dimensions;
    } else {
        const auto it = matrix_dimensions.find(name);
        if (it != matrix_dimensions.end()) {
            node->dimensions = it->second;
        }
    }
    return node;
}

std::vector<std::string> Parser::extractDimensions(
    const std::vector<Token>& body, size_t at) {
    std::vector<std::string> dimensions;
    size_t pos = at + 1;
    while (pos < body.size() && body[pos].type == SYMBOL && body[pos].value == "[") {
        ++pos;
        std::string dimension;
        int nested = 1;
        while (pos < body.size() && nested > 0) {
            if (body[pos].type == SYMBOL && body[pos].value == "[") {
                ++nested;
            } else if (body[pos].type == SYMBOL && body[pos].value == "]") {
                --nested;
                if (nested == 0) {
                    break;
                }
            }
            if (nested > 0) {
                if (!dimension.empty()) {
                    dimension += " ";
                }
                dimension += body[pos].value;
            }
            ++pos;
        }
        dimensions.push_back(dimension);
        if (pos < body.size() && body[pos].type == SYMBOL && body[pos].value == "]") {
            ++pos;
        }
    }
    return dimensions;
}

bool Parser::isIndexedMatrix(const std::vector<Token>& body, size_t at) {
    return at < body.size() && isIdentifierToken(body[at]) && at + 1 < body.size() &&
           body[at + 1].type == SYMBOL && body[at + 1].value == "[";
}

std::unique_ptr<ASTNode> Parser::parse() {
    auto program = std::make_unique<ASTNode>();
    program->type = PROGRAM_NODE;
    program->value = "Program";

    while (index < tokens.size() && !check(END)) {
        if (isFunctionStart(index)) {
            auto function = parseFunction();
            if (function) {
                program->children.push_back(std::move(function));
            }
            continue;
        }

        // A preprocessor token represents a directive already consumed by the
        // lexer. It carries no executable AST node, but consume it to avoid
        // repeatedly trying to parse the same token.
        if (check(PREPROCESSOR)) {
            advance();
            continue;
        }

        // Preserve global matrix declarations for clients that inspect the AST.
        // Code generation generally uses function parameters, but globals are
        // still useful for diagnostics and future frontends.
        if (isMatrixTypeToken(current()) && index + 1 < tokens.size() &&
            isIdentifierToken(tokens[index + 1]) && index + 2 < tokens.size() &&
            tokens[index + 2].type == SYMBOL && tokens[index + 2].value == "[") {
            if (auto statement = parseMatrixDeclaration()) {
                program->children.push_back(std::move(statement));
            }
            continue;
        }

        // Keep support for a standalone matrix operation in a small DSL input.
        if (match(OPERATOR) && current().value == "*") {
            if (auto operation = parseMatrixOperation()) {
                program->children.push_back(std::move(operation));
            }
            continue;
        }

        advance();
    }
    return program;
}

std::unique_ptr<ASTNode> Parser::parseFunction() {
    if (!isFunctionStart(index)) {
        return nullptr;
    }

    const int line = tokens[index].line;
    advance(); // return type
    const std::string name = current().value;
    advance(); // function name

    auto function = std::make_unique<ASTNode>();
    function->type = FUNCTION_NODE;
    function->value = name;
    function->line = line;

    // Parse parameters. A parameter is a matrix when its identifier is
    // followed by one or more bracketed dimensions, irrespective of whether
    // the identifier is upper-case or lower-case.
    if (isSymbol("(")) {
        advance();
    }
    while (index < tokens.size() && !isSymbol(")") && !check(END)) {
        if (isIdentifierToken(tokens[index]) && index + 1 < tokens.size() &&
            tokens[index + 1].type == SYMBOL && tokens[index + 1].value == "[") {
            const std::string matrixName = tokens[index].value;
            const int matrixLine = tokens[index].line;
            std::vector<std::string> dimensions;
            size_t pos = index + 1;
            while (pos < tokens.size() && tokens[pos].type == SYMBOL && tokens[pos].value == "[") {
                ++pos;
                std::string dimension;
                int nested = 1;
                while (pos < tokens.size() && nested > 0) {
                    if (tokens[pos].type == SYMBOL && tokens[pos].value == "[") {
                        ++nested;
                    } else if (tokens[pos].type == SYMBOL && tokens[pos].value == "]") {
                        --nested;
                        if (nested == 0) {
                            break;
                        }
                    }
                    if (nested > 0) {
                        if (!dimension.empty()) {
                            dimension += " ";
                        }
                        dimension += tokens[pos].value;
                    }
                    ++pos;
                }
                dimensions.push_back(dimension);
                if (pos < tokens.size() && tokens[pos].type == SYMBOL && tokens[pos].value == "]") {
                    ++pos;
                }
            }
            declared_matrices.insert(matrixName);
            matrix_dimensions[matrixName] = dimensions;
            function->children.push_back(makeMatrixNode(matrixName, matrixLine, dimensions));
            index = pos;
            continue;
        }
        advance();
    }
    if (isSymbol(")")) {
        advance();
    }

    // Find and consume the function body. Braces are balanced here, so a
    // nested loop or conditional cannot accidentally terminate the function.
    while (index < tokens.size() && !isOpenBrace(tokens[index]) &&
           !(tokens[index].type == SYMBOL && tokens[index].value == ";") &&
           !check(END)) {
        advance();
    }
    if (index < tokens.size() && tokens[index].type == SYMBOL && tokens[index].value == ";") {
        // A declaration/prototype has no body and must not consume the next
        // function while looking for an opening brace.
        advance();
        return function;
    }
    if (index >= tokens.size() || !isOpenBrace(tokens[index])) {
        return function;
    }

    advance();
    const size_t bodyStart = index;
    int depth = 1;
    while (index < tokens.size() && depth > 0) {
        if (isOpenBrace(tokens[index])) {
            ++depth;
        } else if (isCloseBrace(tokens[index])) {
            --depth;
        }
        if (depth > 0) {
            ++index;
        }
    }
    const size_t bodyEnd = index;
    if (index < tokens.size() && isCloseBrace(tokens[index])) {
        ++index;
    }

    std::vector<Token> body(tokens.begin() + static_cast<std::ptrdiff_t>(bodyStart),
                            tokens.begin() + static_cast<std::ptrdiff_t>(bodyEnd));
    parseStatementsInBody(body, function.get());
    return function;
}

void Parser::parseFunctionBody(ASTNode* funcNode) {
    // Kept as a compatibility entry point for callers of the original parser.
    // parseFunction() now captures a balanced body and delegates to the pure
    // token-range implementation below.
    if (funcNode == nullptr || index >= tokens.size()) {
        return;
    }
    const size_t start = index;
    int depth = 0;
    while (index < tokens.size()) {
        if (isOpenBrace(tokens[index])) {
            ++depth;
        } else if (isCloseBrace(tokens[index])) {
            if (depth == 0) {
                break;
            }
            --depth;
        }
        ++index;
        if (depth == 0) {
            break;
        }
    }
    const size_t end = index;
    std::vector<Token> body(tokens.begin() + static_cast<std::ptrdiff_t>(start),
                            tokens.begin() + static_cast<std::ptrdiff_t>(end));
    parseStatementsInBody(body, funcNode);
}

void Parser::parseStatementsInBody(const std::vector<Token>& body, ASTNode* funcNode) {
    if (funcNode == nullptr) {
        return;
    }

    // Matrix expressions are recognized from indexed operands, rather than
    // from identifier capitalization. This handles A/B/C and a/b/c equally,
    // and avoids confusing loop variables (i, j, k) with matrices.
    auto collectMatrices = [&](size_t begin, size_t end) {
        std::vector<size_t> locations;
        std::unordered_set<std::string> seen;
        for (size_t p = begin; p < end && p < body.size(); ++p) {
            if (!isIndexedMatrix(body, p)) {
                continue;
            }
            if (seen.insert(body[p].value).second) {
                locations.push_back(p);
            }
        }
        return locations;
    };

    for (size_t star = 0; star < body.size(); ++star) {
        if (body[star].type != OPERATOR || body[star].value != "*") {
            continue;
        }

        size_t statementBegin = star;
        while (statementBegin > 0 && !isBoundary(body[statementBegin - 1])) {
            --statementBegin;
        }
        const size_t statementEnd = [&]() {
            size_t p = star + 1;
            while (p < body.size() && !(body[p].type == SYMBOL && body[p].value == ";")) {
                ++p;
            }
            return p;
        }();

        // The lowering currently supports a single binary product per
        // assignment.  Reject chained products instead of accidentally
        // emitting one operation for each '*' with shifted operands.
        size_t productCount = 0;
        for (size_t p = statementBegin; p < statementEnd; ++p) {
            if (body[p].type == OPERATOR && body[p].value == "*") {
                ++productCount;
            }
        }
        if (productCount != 1) {
            continue;
        }

        size_t assignment = statementBegin;
        bool foundAssignment = false;
        for (size_t p = statementBegin; p < star; ++p) {
            if (body[p].type == OPERATOR &&
                (body[p].value == "=" || body[p].value == "+=")) {
                assignment = p;
                foundAssignment = true;
            }
        }
        if (!foundAssignment) {
            continue;
        }

        const auto left = collectMatrices(statementBegin, assignment);
        const auto right = collectMatrices(assignment + 1, statementEnd);
        if (left.empty() || right.size() < 2) {
            continue;
        }

        // Ignore operations that involve a scalar multiplication with only one
        // matrix operand. The two right-hand indexed matrices are the A and B
        // inputs; the indexed left-hand matrix is C.
        const std::string resultName = body[left.front()].value;
        const std::string aName = body[right[0]].value;
        const std::string bName = body[right[1]].value;
        if (aName == bName || resultName == aName || resultName == bName) {
            // In-place products can legitimately use C as an input, so only
            // reject duplicate right operands here; C == A/B is allowed.
            if (aName == bName) {
                continue;
            }
        }

        auto operation = std::make_unique<ASTNode>();
        operation->type = MATRIX_OP_NODE;
        operation->value = "*";
        operation->line = body[star].line;
        // Prefer declaration dimensions (e.g. ROWS/COLS). If a matrix was
        // not declared as a parameter, retain the dimensions observed at its
        // indexed use as useful fallback metadata.
        const auto dimensionsFor = [&](const std::string& name, size_t location) {
            return matrix_dimensions.find(name) == matrix_dimensions.end()
                       ? extractDimensions(body, location)
                       : std::vector<std::string>{};
        };
        operation->children.push_back(makeMatrixNode(aName, body[right[0]].line,
                                                     dimensionsFor(aName, right[0])));
        operation->children.push_back(makeMatrixNode(bName, body[right[1]].line,
                                                     dimensionsFor(bName, right[1])));
        operation->children.push_back(makeMatrixNode(resultName, body[left.front()].line,
                                                     dimensionsFor(resultName, left.front())));
        funcNode->children.push_back(std::move(operation));
    }
}

void Parser::skipToNextFunction() {
    while (index < tokens.size() && !isOpenBrace(tokens[index]) && !check(END)) {
        advance();
    }
    if (index >= tokens.size() || check(END)) {
        return;
    }
    int depth = 0;
    do {
        if (isOpenBrace(tokens[index])) {
            ++depth;
        } else if (isCloseBrace(tokens[index])) {
            --depth;
        }
        advance();
    } while (index < tokens.size() && depth > 0);
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (check(PREPROCESSOR)) {
        const int line = current().line;
        advance();
        auto node = std::make_unique<ASTNode>();
        node->type = MEMORY_OP_NODE;
        node->value = "#define";
        node->line = line;
        return node;
    }
    if (isMatrixTypeToken(current())) {
        return parseMatrixDeclaration();
    }
    if (match(OPERATOR) && current().value == "*") {
        return parseMatrixOperation();
    }
    advance();
    return nullptr;
}

std::unique_ptr<ASTNode> Parser::parseMatrixDeclaration() {
    if (index >= tokens.size() || !isMatrixTypeToken(tokens[index])) {
        return nullptr;
    }
    const int line = tokens[index].line;
    ++index;
    if (index >= tokens.size() || !isIdentifierToken(tokens[index])) {
        return nullptr;
    }

    const std::string name = tokens[index].value;
    const int nameLine = tokens[index].line;
    ++index;
    std::vector<std::string> dimensions;
    while (index < tokens.size() && tokens[index].type == SYMBOL && tokens[index].value == "[") {
        ++index;
        std::string dimension;
        while (index < tokens.size() &&
               !(tokens[index].type == SYMBOL && tokens[index].value == "]")) {
            if (!dimension.empty()) {
                dimension += " ";
            }
            dimension += tokens[index].value;
            ++index;
        }
        dimensions.push_back(dimension);
        if (index < tokens.size()) {
            ++index;
        }
    }

    declared_matrices.insert(name);
    matrix_dimensions[name] = dimensions;
    auto node = std::make_unique<ASTNode>();
    node->type = MATRIX_DECL_NODE;
    node->value = name;
    node->line = line;
    node->children.push_back(makeMatrixNode(name, nameLine, dimensions));

    // Consume the remainder of an initializer/declaration safely.
    int braces = 0;
    while (index < tokens.size()) {
        if (tokens[index].type == SYMBOL && tokens[index].value == "{") {
            ++braces;
        } else if (tokens[index].type == SYMBOL && tokens[index].value == "}") {
            if (braces == 0) {
                break;
            }
            --braces;
        } else if (tokens[index].type == SYMBOL && tokens[index].value == ";" && braces == 0) {
            ++index;
            break;
        }
        ++index;
    }
    return node;
}

std::unique_ptr<ASTNode> Parser::parseMatrixOperation() {
    if (!(match(OPERATOR) && current().value == "*")) {
        return nullptr;
    }
    auto operation = std::make_unique<ASTNode>();
    operation->type = MATRIX_OP_NODE;
    operation->value = "*";
    operation->line = current().line;
    advance();

    while (index < tokens.size() && !isSymbol(";")) {
        if (isIdentifierToken(tokens[index])) {
            operation->children.push_back(makeMatrixNode(tokens[index].value, tokens[index].line));
            if (operation->children.size() == 3) {
                break;
            }
        }
        advance();
    }
    return operation;
}
