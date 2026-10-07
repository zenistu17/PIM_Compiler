#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include "Lexer.h"
#include "Parser.h"
#include "CodeGen.h"
#include "TargetBackend.h"

using namespace std;

namespace {
void printUsage(const char* executable, ostream& stream) {
    stream << "Usage: " << executable << " <input.cpp> -o <output.isa>\n"
           << "       " << executable << " --help\n";
}

bool parseArguments(int argc, char* argv[], string& inputPath, string& outputPath) {
    if (argc == 2 && (string(argv[1]) == "--help" || string(argv[1]) == "-h")) {
        printUsage(argv[0], cout);
        return false;
    }
    if (argc != 4) {
        printUsage(argv[0], cerr);
        return false;
    }

    // Keep -o explicit so a source path beginning with '-' cannot be treated
    // as an option accidentally.  Accepting the output option in either
    // position makes the command pleasant to use while retaining a strict,
    // unambiguous interface.
    if (string(argv[2]) == "-o") {
        inputPath = argv[1];
        outputPath = argv[3];
    } else if (string(argv[1]) == "-o") {
        inputPath = argv[3];
        outputPath = argv[2];
    } else {
        cerr << "Error: expected '-o <output.isa>'.\n";
        printUsage(argv[0], cerr);
        return false;
    }

    if (inputPath.empty() || outputPath.empty()) {
        cerr << "Error: input and output paths must not be empty.\n";
        return false;
    }
    return true;
}

bool hasMatrixOperation(const ASTNode* node) {
    if (node == nullptr) {
        return false;
    }
    if (node->type == MATRIX_OP_NODE && node->value == "*") {
        return true;
    }
    for (const auto& child : node->children) {
        if (hasMatrixOperation(child.get())) {
            return true;
        }
    }
    return false;
}
} // namespace

int main(int argc, char* argv[]) {
    string inputPath;
    string outputPath;
    if (!parseArguments(argc, argv, inputPath, outputPath)) {
        // --help is successful; malformed invocations are errors.  This
        // distinction is useful to shell scripts and package managers.
        return argc == 2 && (string(argv[1]) == "--help" || string(argv[1]) == "-h") ? 0 : 1;
    }

    try {
        ifstream input(inputPath);
        if (!input.is_open()) {
            throw runtime_error("could not open input file: " + inputPath);
        }

        string source((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
        if (source.empty()) {
            throw runtime_error("input file is empty: " + inputPath);
        }

        Lexer lexer(source);
        const auto tokens = lexer.tokenize();

        Parser parser(tokens);
        auto ast = parser.parse();
        if (!ast) {
            throw runtime_error("parser returned no AST");
        }
        if (!hasMatrixOperation(ast.get())) {
            throw runtime_error("no matrix multiplication expression found in input");
        }

        CodeGen codegen(std::move(ast), lexer.getMatrixSize(), lexer.getDefines());
        const auto isa = codegen.generatePIM_ISA();
        if (isa.empty()) {
            throw runtime_error("code generator produced no ISA instructions");
        }

        if (!TargetBackend::validateISA(isa)) {
            throw runtime_error("generated ISA failed validation");
        }

        TargetBackend::emitISA(isa, outputPath);
        return 0;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
