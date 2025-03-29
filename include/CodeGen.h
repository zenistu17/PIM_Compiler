#ifndef CODEGEN_H
#define CODEGEN_H

#include "Parser.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <memory>

class CodeGen {
public:
    CodeGen(std::unique_ptr<ASTNode> ast, int size);
    std::vector<std::string> generatePIM_ISA();
    
private:
    void identifyMatrices();
    void processFunctionNode(const ASTNode* funcNode, std::vector<std::string>& isa);
    void generateMatrixMultiplyExecution(const std::string& matA, const std::string& matB,
                                        const std::string& matC, std::vector<std::string>& isa);
    std::string allocateMatrix(const std::string& name);
    void validateMatrix(const std::string& name);
    
    // Changed function names to better reflect their purpose
    void generateMacOperation(std::vector<std::string>& isa);
    void generateMatrixMultiplyOperation(std::vector<std::string>& isa);
    
    std::unique_ptr<ASTNode> root;
    int matrix_size;
    std::unordered_map<std::string, std::string> matrix_map;
    std::set<std::string> matrices_to_allocate;
    int current_address = 0x1000; // Start allocating at 0x1000
};

#endif // CODEGEN_H
