#include <iostream>
#include <fstream>
#include <chrono>
#include "Lexer.h"
#include "Parser.h"
#include "CodeGen.h"
#include "TargetBackend.h"

using namespace std;

void printAST(const ASTNode* node, int depth = 0) {
    string indent(depth * 2, ' ');
    cout << indent << "Type: " << node->type << ", Value: " << node->value;
    if (node->line > 0) cout << " (Line: " << node->line << ")";
    cout << endl;
    
    for (const auto& child : node->children) {
        printAST(child.get(), depth + 1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4 || string(argv[2]) != "-o") {
        cerr << "Usage: " << argv[0] << " <input.cpp> -o <output.isa>\n";
        return 1;
    }

    try {
        auto start_time = chrono::high_resolution_clock::now();
        
        cout << "=== PIM Compiler ===\n";
        cout << "Reading input file: " << argv[1] << endl;
        ifstream input(argv[1]);
        if (!input.is_open()) {
            cerr << "Error: Could not open input file\n";
            return 1;
        }

        string source((istreambuf_iterator<char>(input)),
                     istreambuf_iterator<char>());
        cout << "File read successfully (" << source.size() << " bytes)\n";

        cout << "\n=== Lexer ===\n";
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        cout << "Generated " << tokens.size() << " tokens\n";
        cout << "Detected matrix size: " << lexer.getMatrixSize() << "x" << lexer.getMatrixSize() << endl;

        cout << "\n=== Parser ===\n";
        Parser parser(tokens);
        auto ast = parser.parse();
        cout << "AST built with " << ast->children.size() << " top-level nodes\n";
        
        // Print detailed AST info for debugging
        cout << "\nAST Structure:\n";
        printAST(ast.get());
        
        cout << "\n=== Code Generation ===\n";
        CodeGen codegen(std::move(ast), lexer.getMatrixSize());
        auto isa = codegen.generatePIM_ISA();
        cout << "Generated " << isa.size() << " ISA instructions\n";

        // Validate the generated ISA
        bool valid = TargetBackend::validateISA(isa);
        if (!valid) {
            cerr << "Warning: Generated ISA might have structural issues\n";
        }

        cout << "\n=== Output ===\n";
        cout << "Writing output to " << argv[3] << endl;
        TargetBackend::emitISA(isa, argv[3]);
        
        auto end_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
        cout << "Compilation completed in " << duration.count() << "ms\n";
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
