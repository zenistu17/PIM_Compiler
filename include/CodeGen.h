#ifndef CODEGEN_H
#define CODEGEN_H

#include "Parser.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Dimensions are kept separate from the ISA address. The original backend
// treated every matrix as matrix_size x matrix_size, which silently allocated
// the wrong amount of memory for rectangular products.
struct MatrixShape {
    std::size_t rows = 0;
    std::size_t cols = 0;
};

class CodeGen {
public:
    CodeGen(std::unique_ptr<ASTNode> ast, int size);
    CodeGen(std::unique_ptr<ASTNode> ast, int size,
            const std::unordered_map<std::string, int>& definitions);

    std::vector<std::string> generatePIM_ISA();

private:
    void identifyMatrices();
    void collectMatrices(const ASTNode* node);
    void inferOperationShapes(const ASTNode* node);
    void processFunctionNode(const ASTNode* funcNode, std::vector<std::string>& isa);
    void processNode(const ASTNode* node, std::vector<std::string>& isa);
    void generateMatrixMultiplyExecution(const std::string& matA, const std::string& matB,
                                         const std::string& matC, std::vector<std::string>& isa);

    std::string allocateMatrix(const std::string& name);
    void validateMatrix(const std::string& name) const;
    void registerMatrix(const std::string& name, const MatrixShape& shape, bool explicitShape);
    MatrixShape shapeFromNode(const ASTNode* node) const;
    MatrixShape defaultShape() const;
    std::size_t resolveDimension(const std::string& expression) const;

    void generateMacOperation(std::vector<std::string>& isa);
    void generateMatrixMultiplyOperation(std::vector<std::string>& isa);

    std::unique_ptr<ASTNode> root;
    int matrix_size;
    std::unordered_map<std::string, std::size_t> definitions;

    // std::map gives stable allocation and release order in generated ISA.
    std::map<std::string, std::string> matrix_map;
    std::map<std::string, MatrixShape> matrix_shapes;
    std::unordered_set<std::string> explicit_shapes;
    std::vector<std::string> allocation_order;
    std::size_t current_address = 0x1000;
    std::size_t operation_count = 0;
    static constexpr std::size_t kMemoryEnd = 0x10000;
};

#endif // CODEGEN_H
