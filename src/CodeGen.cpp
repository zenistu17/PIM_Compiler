#include "CodeGen.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <iomanip>  // For std::setw, std::setfill

using namespace std;

CodeGen::CodeGen(unique_ptr<ASTNode> ast, int size)
    : root(std::move(ast)), matrix_size(size) {}

vector<string> CodeGen::generatePIM_ISA() {
    vector<string> isa;
    cout << "[CodeGen] Starting ISA generation for matrix size " << matrix_size << endl;
    
    // Memory configuration
    isa.push_back("# MEMORY CONFIGURATION");
    isa.push_back("ALLOCATE 0x0000 0xFFFF");
    isa.push_back("");
    
    // Define MAC operation for programming cores
    generateMacOperation(isa);
    
    // Define matrix multiplication operation
    generateMatrixMultiplyOperation(isa);
    
    // First pass: identify all matrices that need allocation
    identifyMatrices();
    
    // Second pass: allocate all matrices
    isa.push_back("# MATRIX ALLOCATIONS");
    for (const auto& name : matrices_to_allocate) {
        string addr = allocateMatrix(name);
        isa.push_back("# Matrix " + name + " allocated at " + addr);
    }
    isa.push_back("");
    
    for (const auto& [name, addr] : matrix_map) {
        cout << "[CodeGen] Matrix " << name << " mapped to " << addr << endl;
    }
    
    // Third pass: process functions containing matrix operations
    isa.push_back("# MATRIX OPERATIONS");
    for (const auto& node : root->children) {
        if (node->type == FUNCTION_NODE) {
            processFunctionNode(node.get(), isa);
        }
    }
    
    // Memory cleanup
    isa.push_back("");
    isa.push_back("# MEMORY RELEASE");
    for (const auto& [name, addr] : matrix_map) {
        size_t size = matrix_size * matrix_size * sizeof(int);
        isa.push_back("FREE " + addr + " " + to_string(size));
    }
    
    // End program
    isa.push_back("END");
    
    cout << "[CodeGen] Generated " << isa.size() << " instructions" << endl;
    return isa;
}

void CodeGen::generateMacOperation(std::vector<std::string>& isa) {
    isa.push_back("# Define the MAC (Multiply-Accumulate) operation for dot product");
    isa.push_back("# First program the MAC function into the pPIM core");
    
    // Program the MAC operation into a core (register r0)
    isa.push_back("PROG r0, mac_operation");
    isa.push_back("# MAC operation microcode");
    isa.push_back("EXE MUL r1, ah, bh  # Multiply high bits");
    isa.push_back("EXE MUL r2, al, bl  # Multiply low bits");
    isa.push_back("EXE MUL r3, ah, bl  # Multiply high with low");
    isa.push_back("EXE MUL r4, al, bh  # Multiply low with high");
    isa.push_back("EXE ADD r5, r3, r4  # Combine cross products");
    isa.push_back("EXE ADD r6, r1, r2  # Combine direct products");
    isa.push_back("EXE ADD r0, r5, r6  # Final result");
    isa.push_back("END mac_operation");
    isa.push_back("");
}

void CodeGen::generateMatrixMultiplyOperation(std::vector<std::string>& isa) {
    isa.push_back("# Define a matrix multiplication operation");
    isa.push_back("# Program the matrix multiplication function into the pPIM core");
    isa.push_back("PROG r2, matrix_multiply");
    
    // Define basic operations needed for matrix multiplication
    isa.push_back("# Matrix multiplication microcode");
    isa.push_back("EXE ADD r0, r1, r2  # Addition operation: r0 = r1 + r2");
    isa.push_back("EXE MUL r0, r1, r2  # Multiplication operation: r0 = r1 * r2");
    isa.push_back("EXE ZERO r0         # Zero register: r0 = 0");
    
    // Matrix multiplication implementation for NxN matrices
    isa.push_back("# Matrix multiplication implementation for " + to_string(matrix_size) + "x" + to_string(matrix_size) + " matrices");
    
    // We'll define a generic matrix multiplication pattern
    // The actual matrix indices will be resolved during execution
    isa.push_back("# For each element of the result matrix");
    isa.push_back("# Z[i][j] = sum(X[i][k] * Y[k][j]) for all k");
    
    // Implement the core multiplication loop structure
    isa.push_back("EXE ZERO acc                # Initialize accumulator to 0");
    isa.push_back("EXE READ r1, X_addr[i][k]   # Load X[i][k]");
    isa.push_back("EXE READ r2, Y_addr[k][j]   # Load Y[k][j]");
    isa.push_back("EXE MUL r3, r1, r2          # r3 = X[i][k] * Y[k][j]");
    isa.push_back("EXE ADD acc, acc, r3        # acc += r3");
    isa.push_back("EXE WRITE Z_addr[i][j], acc # Store result to Z[i][j]");
    
    isa.push_back("END matrix_multiply");
    isa.push_back("");
}

void CodeGen::identifyMatrices() {
    // Process all function nodes to find matrix declarations
    for (const auto& node : root->children) {
        if (node->type == FUNCTION_NODE) {
            // Function parameters (matrices)
            for (const auto& child : node->children) {
                if (child->type == MATRIX_DECL_NODE) {
                    matrices_to_allocate.insert(child->value);
                }
                
                // Also check for matrix operations within the function
                if (child->type == MATRIX_OP_NODE) {
                    for (const auto& opChild : child->children) {
                        if (opChild->type == MATRIX_DECL_NODE) {
                            matrices_to_allocate.insert(opChild->value);
                        }
                    }
                }
            }
        }
    }
    
    // Debug output
    cout << "[CodeGen] Identified matrices to allocate: ";
    for (const auto& matrix : matrices_to_allocate) {
        cout << matrix << " ";
    }
    cout << endl;
}

void CodeGen::processFunctionNode(const ASTNode* funcNode, vector<string>& isa) {
    cout << "[CodeGen] Processing function: " << funcNode->value << endl;
    
    // First pass: collect all matrices
    for (const auto& node : funcNode->children) {
        if (node->type == MATRIX_DECL_NODE) {
            cout << "[CodeGen] Found matrix: " << node->value << endl;
            allocateMatrix(node->value);
        }
    }
    
    // Second pass: process operations
    for (const auto& node : funcNode->children) {
        if (node->type == MATRIX_OP_NODE && node->value == "*") {
            if (node->children.size() >= 3) {
                const string& A = node->children[0]->value;
                const string& B = node->children[1]->value;
                const string& C = node->children[2]->value;
                
                cout << "[CodeGen] Generating multiplication: "
                     << A << " * " << B << " -> " << C << endl;
                
                // Validate addresses
                if (matrix_map.find(A) == matrix_map.end() ||
                    matrix_map.find(B) == matrix_map.end() ||
                    matrix_map.find(C) == matrix_map.end()) {
                    cerr << "Error: Missing matrix address\n";
                    continue;
                }
                
                generateMatrixMultiplyExecution(A, B, C, isa);
            }
        }
    }
}

void CodeGen::generateMatrixMultiplyExecution(const string& matA, const string& matB,
                                             const string& matC, vector<string>& isa) {
    isa.push_back("# MATRIX MULTIPLICATION " + matA + " * " + matB + " -> " + matC);
    
    // Use the pre-programmed matrix multiplication operation from register r2
    isa.push_back("EXE r2, " + matrix_map[matA] + ", " + matrix_map[matB] + ", " +
                 matrix_map[matC] + ", " + to_string(matrix_size));
}

string CodeGen::allocateMatrix(const string& name) {
    if (matrix_map.find(name) != matrix_map.end()) {
        return matrix_map[name];
    }
    
    size_t size_needed = matrix_size * matrix_size * sizeof(int);
    if (current_address + size_needed > 0xFFFF) {
        throw runtime_error("PIM memory overflow");
    }
    
    // Format address with proper hex
    stringstream ss;
    ss << "0x" << hex << setw(4) << setfill('0') << current_address;
    matrix_map[name] = ss.str();
    current_address += size_needed;
    
    return matrix_map[name];
}

void CodeGen::validateMatrix(const string& name) {
    if (matrix_map.find(name) == matrix_map.end()) {
        // If the matrix doesn't exist yet, allocate it
        allocateMatrix(name);
    }
}
