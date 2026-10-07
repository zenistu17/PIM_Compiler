#include "Lexer.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace {
// std::isalpha (and friends) require an unsigned-char argument. Using this
// helper also makes lexing arbitrary source bytes and EOF boundaries defined.
bool isAlpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool isSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}
} // namespace

Lexer::Lexer(const std::string& src) : source(src) {}

void Lexer::handlePreprocessor() {
    // '#' has already been consumed by tokenize(). Read the directive, then
    // parse numeric matrix-size definitions or discard directives such as
    // #include. Preprocessor text must not leak into the parser token stream.
    while (index < source.size() && isSpace(source[index]) && source[index] != '\n') {
        ++index;
    }

    std::string directive;
    while (index < source.size() && isAlpha(source[index])) {
        directive += source[index++];
    }

    if (directive == "define") {
        parseMatrixSize();
    } else {
        while (index < source.size() && source[index] != '\n') {
            ++index;
        }
    }
}

void Lexer::parseMatrixSize() {
    while (index < source.size() && isSpace(source[index]) && source[index] != '\n') {
        ++index;
    }

    std::string ident;
    while (index < source.size() && isIdentChar(source[index])) {
        ident += source[index++];
    }

    // Matrix dimensions in the input examples are conventionally named N,
    // SIZE, ROWS, COLS, or INNER. Restrict scanning to this physical line;
    // the old implementation could consume a later declaration looking for
    // digits when a #define had no value.
    const bool isDimension = ident == "N" || ident == "SIZE" ||
                             ident == "ROWS" || ident == "COLS" ||
                             ident == "INNER" || ident == "M" ||
                             ident == "K" || ident == "R" || ident == "C";
    while (index < source.size() && source[index] != '\n' && isSpace(source[index])) {
        ++index;
    }
    bool negative = false;
    if (index < source.size() && (source[index] == '+' || source[index] == '-')) {
        negative = source[index] == '-';
        ++index;
    }

    if (!negative && index < source.size() && isDigit(source[index])) {
        std::string number;
        while (index < source.size() && isDigit(source[index])) {
            number += source[index++];
        }
        try {
            const int value = std::stoi(number);
            defines[ident] = value;
            if (isDimension) {
                matrix_size = std::max(matrix_size, value);
            }
        } catch (...) {
            // An out-of-range define should not crash lexing. The parser and
            // code generator will report an unusable size later.
        }
    }

    // Ignore comments and trailing text on the #define line.
    while (index < source.size() && source[index] != '\n') {
        ++index;
    }
}

char Lexer::advance() {
    if (index >= source.size()) {
        return '\0';
    }
    return source[index++];
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    index = 0;
    current_line = 1;
    matrix_size = 0;
    defines.clear();

    while (index < source.size()) {
        const char c = source[index];

        if (c == '#') {
            addToken(tokens, PREPROCESSOR, "#");
            ++index;
            handlePreprocessor();
            continue;
        }

        if (isSpace(c)) {
            if (c == '\n') {
                ++current_line;
            }
            ++index;
            continue;
        }

        // String and character literals are irrelevant to matrix lowering,
        // but their contents may contain tokens such as "void f(" or "*".
        // Skip them as one lexical unit so they cannot be mistaken for a
        // function declaration or matrix operation.
        if (c == '"' || c == '\'') {
            const char quote = c;
            ++index;
            while (index < source.size()) {
                if (source[index] == '\\') {
                    index += (index + 1 < source.size()) ? 2 : 1;
                    continue;
                }
                if (source[index] == quote) {
                    ++index;
                    break;
                }
                if (source[index] == '\n') {
                    ++current_line;
                }
                ++index;
            }
            continue;
        }

        // Comments have no semantic meaning to the matrix frontend. Keeping
        // them out also prevents operators in comments looking like code.
        if (c == '/' && index + 1 < source.size() && source[index + 1] == '/') {
            index += 2;
            while (index < source.size() && source[index] != '\n') {
                ++index;
            }
            continue;
        }
        if (c == '/' && index + 1 < source.size() && source[index + 1] == '*') {
            index += 2;
            while (index < source.size()) {
                if (source[index] == '*' && index + 1 < source.size() && source[index + 1] == '/') {
                    index += 2;
                    break;
                }
                if (source[index] == '\n') {
                    ++current_line;
                }
                ++index;
            }
            continue;
        }

        if (isAlpha(c) || c == '_') {
            const int line = current_line;
            std::string ident;
            while (index < source.size() && isIdentChar(source[index])) {
                ident += source[index++];
            }

            if (ident == "int" || ident == "float" || ident == "double" ||
                ident == "MATRIX") {
                addToken(tokens, MATRIX_TYPE, ident);
            } else if (ident.size() == 1 &&
                       std::isupper(static_cast<unsigned char>(ident[0])) != 0) {
                // Keep compatibility with the original token contract for
                // conventional A/B/C matrix names. The parser also accepts
                // ordinary identifiers, so lower-case and multi-letter names
                // work as expected.
                addToken(tokens, MATRIX_DECL, ident);
            } else {
                // Keep control-flow and function words as IDENTIFIER. This is
                // part of the original token contract and supports arbitrary
                // function names without a fixed keyword table.
                addToken(tokens, IDENTIFIER, ident);
            }
            tokens.back().line = line;
            continue;
        }

        if (isDigit(c)) {
            const int line = current_line;
            std::string number;
            while (index < source.size() && isDigit(source[index])) {
                number += source[index++];
            }
            // Accept a simple decimal literal for initializers.
            if (index < source.size() && source[index] == '.' &&
                index + 1 < source.size() && isDigit(source[index + 1])) {
                number += source[index++];
                while (index < source.size() && isDigit(source[index])) {
                    number += source[index++];
                }
            }
            addToken(tokens, NUMBER, number);
            tokens.back().line = line;
            continue;
        }

        // Longest-match operators. In particular, += must remain one token;
        // the parser uses it to identify accumulation in a matmul loop.
        static const char* const twoCharacterOperators[] = {
            "+=", "-=", "*=", "/=", "==", "!=", "<=", ">=", "++", "--",
            "&&", "||"
        };
        bool matched = false;
        if (index + 1 < source.size()) {
            const std::string op{source[index], source[index + 1]};
            for (const char* candidate : twoCharacterOperators) {
                if (op == candidate) {
                    addToken(tokens, OPERATOR, op);
                    index += 2;
                    matched = true;
                    break;
                }
            }
        }
        if (matched) {
            continue;
        }

        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
            c == '%' || c == '!' || c == '<' || c == '>') {
            addToken(tokens, OPERATOR, std::string(1, c));
            ++index;
            continue;
        }

        // Punctuation, including '[' and ']', is retained. Array indices are
        // useful when distinguishing matrix operands from loop variables.
        addToken(tokens, SYMBOL, std::string(1, c));
        ++index;
    }

    addToken(tokens, END, "");
    return tokens;
}

void Lexer::addToken(std::vector<Token>& tokens, TokenType type, std::string value) {
    tokens.push_back({type, std::move(value), current_line});
}
