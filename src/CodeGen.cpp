#include "CodeGen.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
constexpr std::size_t kElementBytes = sizeof(int);

bool hasName(const ASTNode* node) {
    return node != nullptr && !node->value.empty();
}
}

CodeGen::CodeGen(std::unique_ptr<ASTNode> ast, int size)
    : CodeGen(std::move(ast), size, {}) {}

CodeGen::CodeGen(std::unique_ptr<ASTNode> ast, int size,
                 const std::unordered_map<std::string, int>& constants)
    : root(std::move(ast)), matrix_size(size) {
    if (matrix_size < 0) {
        throw std::invalid_argument("matrix size cannot be negative");
    }
    // Literal dimensions do not require a global #define.  Defer validation
    // until declarations and operations are collected so inputs such as
    // int A[2][2] remain valid while dimensionless programs fail clearly.
    for (const auto& entry : constants) {
        if (entry.second > 0) {
            definitions.emplace(entry.first, static_cast<std::size_t>(entry.second));
        }
    }
}

std::vector<std::string> CodeGen::generatePIM_ISA() {
    if (!root) {
        throw std::invalid_argument("cannot generate ISA from a null AST");
    }

    // Collect declarations and infer roles before allocation. This ensures a
    // rectangular output receives the correct number of bytes even when its
    // first mention is in an operation node.
    identifyMatrices();
    operation_count = 0;

    std::vector<std::string> isa;
    isa.emplace_back("# MEMORY CONFIGURATION");
    isa.emplace_back("ALLOCATE 0x0000 0xFFFF");
    isa.emplace_back("");

    generateMacOperation(isa);
    generateMatrixMultiplyOperation(isa);

    isa.emplace_back("# MATRIX ALLOCATIONS");
    for (const auto& entry : matrix_shapes) {
        const std::string address = allocateMatrix(entry.first);
        std::ostringstream line;
        line << "# Matrix " << entry.first << " allocated at " << address
             << " (" << entry.second.rows << "x" << entry.second.cols << ")";
        isa.push_back(line.str());
    }
    isa.emplace_back("");

    isa.emplace_back("# MATRIX OPERATIONS");
    for (const auto& child : root->children) {
        if (child->type == FUNCTION_NODE) {
            processFunctionNode(child.get(), isa);
        } else {
            processNode(child.get(), isa);
        }
    }

    isa.emplace_back("");
    isa.emplace_back("# MEMORY RELEASE");
    // allocation_order is deterministic and is the same order used for
    // bounds/accounting during allocation.
    for (auto it = allocation_order.rbegin(); it != allocation_order.rend(); ++it) {
        const auto shapeIt = matrix_shapes.find(*it);
        const auto addrIt = matrix_map.find(*it);
        if (shapeIt == matrix_shapes.end() || addrIt == matrix_map.end()) {
            continue;
        }
        const std::size_t bytes = shapeIt->second.rows * shapeIt->second.cols * kElementBytes;
        isa.emplace_back("FREE " + addrIt->second + " " + std::to_string(bytes));
    }

    isa.emplace_back("END");
    if (operation_count == 0) {
        throw std::runtime_error("no supported matrix operations found");
    }
    return isa;
}

void CodeGen::generateMacOperation(std::vector<std::string>& isa) {
    isa.emplace_back("# Define the MAC (Multiply-Accumulate) operation for dot product");
    isa.emplace_back("PROG r0, mac_operation");
    isa.emplace_back("EXE MUL r1, ah, bh");
    isa.emplace_back("EXE MUL r2, al, bl");
    isa.emplace_back("EXE MUL r3, ah, bl");
    isa.emplace_back("EXE MUL r4, al, bh");
    isa.emplace_back("EXE ADD r5, r3, r4");
    isa.emplace_back("EXE ADD r6, r1, r2");
    isa.emplace_back("EXE ADD r0, r5, r6");
    isa.emplace_back("END mac_operation");
    isa.emplace_back("");
}

void CodeGen::generateMatrixMultiplyOperation(std::vector<std::string>& isa) {
    isa.emplace_back("# Define matrix multiplication operation");
    isa.emplace_back("PROG r2, matrix_multiply");
    isa.emplace_back("EXE ZERO acc");
    isa.emplace_back("EXE READ r1, X_addr[i][k]");
    isa.emplace_back("EXE READ r2, Y_addr[k][j]");
    isa.emplace_back("EXE MUL r3, r1, r2");
    isa.emplace_back("EXE ADD acc, acc, r3");
    isa.emplace_back("EXE WRITE Z_addr[i][j], acc");
    isa.emplace_back("END matrix_multiply");
    isa.emplace_back("");
}

void CodeGen::identifyMatrices() {
    matrix_shapes.clear();
    explicit_shapes.clear();
    matrix_map.clear();
    allocation_order.clear();
    current_address = 0x1000;

    for (const auto& child : root->children) {
        collectMatrices(child.get());
    }
    for (const auto& child : root->children) {
        inferOperationShapes(child.get());
    }
}

void CodeGen::collectMatrices(const ASTNode* node) {
    if (!node) {
        return;
    }
    // Operands below a MATRIX_OP_NODE are access expressions (for example
    // A[i][k]), not declarations. Their dimensions may therefore contain loop
    // variables that cannot be resolved as compile-time constants. Actual
    // declarations are collected from function parameters/global statements;
    // unknown operands are inferred by inferOperationShapes instead.
    if (node->type == MATRIX_OP_NODE) {
        return;
    }
    if (node->type == MATRIX_DECL_NODE && hasName(node)) {
        registerMatrix(node->value, shapeFromNode(node), !node->dimensions.empty());
    }
    for (const auto& child : node->children) {
        collectMatrices(child.get());
    }
}

void CodeGen::inferOperationShapes(const ASTNode* node) {
    if (!node) {
        return;
    }
    if (node->type == MATRIX_OP_NODE && node->value == "*" && node->children.size() >= 3) {
        const ASTNode* a = node->children[0].get();
        const ASTNode* b = node->children[1].get();
        const ASTNode* c = node->children[2].get();
        const MatrixShape fallback = defaultShape();
        const std::size_t rowsFallback = definitions.count("ROWS")
                                              ? definitions.at("ROWS")
                                              : fallback.rows;
        const std::size_t colsFallback = definitions.count("COLS")
                                              ? definitions.at("COLS")
                                              : fallback.cols;
        const std::size_t innerFallback = definitions.count("INNER")
                                               ? definitions.at("INNER")
                                               : colsFallback;
        const auto shapeOr = [this](const ASTNode* operand, const MatrixShape& value) {
            if (!operand || operand->dimensions.empty()) {
                return value;
            }
            try {
                return shapeFromNode(operand);
            } catch (const std::runtime_error&) {
                // Indexed uses such as A[i][k] are not declarations. Keep the
                // role-based shape inferred from the other operands.
                return value;
            }
        };
        const MatrixShape aShape = matrix_shapes.count(a->value)
                                       ? matrix_shapes[a->value]
                                       : shapeOr(a, MatrixShape{rowsFallback, innerFallback});
        const MatrixShape bShape = matrix_shapes.count(b->value)
                                       ? matrix_shapes[b->value]
                                       : shapeOr(b, MatrixShape{innerFallback, colsFallback});
        const MatrixShape cShape = matrix_shapes.count(c->value)
                                       ? matrix_shapes[c->value]
                                       : shapeOr(c, MatrixShape{rowsFallback, colsFallback});
        const std::size_t rows = aShape.rows;
        const std::size_t inner = aShape.cols;
        const std::size_t cols = bShape.cols;

        if (hasName(a) && !matrix_shapes.count(a->value)) {
            registerMatrix(a->value, aShape.rows > 0 && aShape.cols > 0
                                           ? aShape
                                           : MatrixShape{rows, inner},
                           !a->dimensions.empty());
        }
        if (hasName(b) && !matrix_shapes.count(b->value)) {
            registerMatrix(b->value, bShape.rows > 0 && bShape.cols > 0
                                           ? bShape
                                           : MatrixShape{inner, cols},
                           !b->dimensions.empty());
        }
        if (hasName(c) && !matrix_shapes.count(c->value)) {
            registerMatrix(c->value, cShape.rows > 0 && cShape.cols > 0
                                           ? cShape
                                           : MatrixShape{rows, cols},
                           !c->dimensions.empty());
        }
    }
    for (const auto& child : node->children) {
        inferOperationShapes(child.get());
    }
}

void CodeGen::processFunctionNode(const ASTNode* funcNode, std::vector<std::string>& isa) {
    // Declarations are collected in the first pass. Recursing here allows the
    // parser to place operations below blocks in a future AST revision.
    for (const auto& child : funcNode->children) {
        processNode(child.get(), isa);
    }
}

void CodeGen::processNode(const ASTNode* node, std::vector<std::string>& isa) {
    if (!node) {
        return;
    }
    if (node->type == MATRIX_OP_NODE && node->value == "*") {
        if (node->children.size() < 3) {
            throw std::runtime_error("matrix multiplication requires A, B, and C operands at line " +
                                     std::to_string(node->line));
        }
        generateMatrixMultiplyExecution(node->children[0]->value,
                                        node->children[1]->value,
                                        node->children[2]->value, isa);
        return;
    }
    // A declaration's children are metadata, not executable operations.
    for (const auto& child : node->children) {
        processNode(child.get(), isa);
    }
}

void CodeGen::generateMatrixMultiplyExecution(const std::string& matA,
                                               const std::string& matB,
                                               const std::string& matC,
                                               std::vector<std::string>& isa) {
    validateMatrix(matA);
    validateMatrix(matB);
    validateMatrix(matC);
    const MatrixShape& a = matrix_shapes.at(matA);
    const MatrixShape& b = matrix_shapes.at(matB);
    const MatrixShape& c = matrix_shapes.at(matC);
    if (a.cols != b.rows || c.rows != a.rows || c.cols != b.cols) {
        throw std::runtime_error("incompatible matrix dimensions for " + matA + " * " + matB +
                                 " -> " + matC);
    }

    isa.emplace_back("# MATRIX MULTIPLICATION " + matA + " * " + matB + " -> " + matC);
    std::string instruction = "EXE r2, " + matrix_map.at(matA) + ", " +
                              matrix_map.at(matB) + ", " + matrix_map.at(matC);
    // Preserve the compact legacy form for square products. Rectangular
    // products carry all three dimensions so a target can execute them
    // without guessing the inner loop bound.
    if (a.rows == a.cols && b.rows == b.cols && c.rows == c.cols &&
        a.rows == b.rows && b.rows == c.rows) {
        instruction += ", " + std::to_string(a.rows);
    } else {
        instruction += ", " + std::to_string(a.rows) + ", " + std::to_string(a.cols) +
                       ", " + std::to_string(b.cols);
    }
    isa.push_back(std::move(instruction));
    ++operation_count;
}

std::string CodeGen::allocateMatrix(const std::string& name) {
    const auto existing = matrix_map.find(name);
    if (existing != matrix_map.end()) {
        return existing->second;
    }
    const auto shapeIt = matrix_shapes.find(name);
    if (shapeIt == matrix_shapes.end() || shapeIt->second.rows == 0 || shapeIt->second.cols == 0) {
        throw std::runtime_error("matrix " + name + " has no valid dimensions");
    }
    if (shapeIt->second.rows > std::numeric_limits<std::size_t>::max() / shapeIt->second.cols ||
        shapeIt->second.rows * shapeIt->second.cols >
            std::numeric_limits<std::size_t>::max() / kElementBytes) {
        throw std::runtime_error("matrix " + name + " is too large");
    }
    const std::size_t bytes = shapeIt->second.rows * shapeIt->second.cols * kElementBytes;
    if (bytes > kMemoryEnd - current_address) {
        throw std::runtime_error("PIM memory overflow allocating matrix " + name);
    }

    std::ostringstream address;
    address << "0x" << std::hex << std::setw(4) << std::setfill('0') << current_address;
    matrix_map.emplace(name, address.str());
    allocation_order.push_back(name);
    current_address += bytes;
    return address.str();
}

void CodeGen::validateMatrix(const std::string& name) const {
    if (name.empty() || matrix_map.find(name) == matrix_map.end()) {
        throw std::runtime_error("unknown matrix " + name);
    }
}

void CodeGen::registerMatrix(const std::string& name, const MatrixShape& shape,
                             bool explicitShape) {
    if (name.empty()) {
        return;
    }
    const auto existing = matrix_shapes.find(name);
    if (existing == matrix_shapes.end()) {
        matrix_shapes.emplace(name, shape);
    } else if (explicitShape && explicit_shapes.count(name) &&
               (existing->second.rows != shape.rows || existing->second.cols != shape.cols)) {
        throw std::runtime_error("conflicting dimensions for matrix " + name);
    } else if (explicitShape && !explicit_shapes.count(name)) {
        existing->second = shape;
    }
    if (explicitShape) {
        explicit_shapes.insert(name);
    }
}

MatrixShape CodeGen::shapeFromNode(const ASTNode* node) const {
    if (!node || node->dimensions.empty()) {
        return defaultShape();
    }
    const std::size_t rows = resolveDimension(node->dimensions[0]);
    const std::size_t cols = node->dimensions.size() > 1
                                 ? resolveDimension(node->dimensions[1])
                                 : rows;
    if (rows == 0 || cols == 0) {
        throw std::runtime_error("invalid dimensions for matrix " + node->value);
    }
    return {rows, cols};
}

MatrixShape CodeGen::defaultShape() const {
    const auto rows = definitions.find("ROWS");
    const auto cols = definitions.find("COLS");
    // A square fallback is used for declarations without dimensions. For a
    // rectangular program the parser supplies dimensions on each parameter.
    if (rows != definitions.end() && cols != definitions.end()) {
        return {rows->second, cols->second};
    }
    return {static_cast<std::size_t>(matrix_size), static_cast<std::size_t>(matrix_size)};
}

std::size_t CodeGen::resolveDimension(const std::string& expression) const {
    try {
        std::size_t consumed = 0;
        const auto value = std::stoull(expression, &consumed);
        if (consumed == expression.size() && value > 0) {
            return value;
        }
    } catch (...) {
        // Resolve symbolic dimensions below.
    }
    const auto definition = definitions.find(expression);
    if (definition != definitions.end()) {
        return definition->second;
    }
    if (expression == "N" || expression == "SIZE") {
        return static_cast<std::size_t>(matrix_size);
    }
    throw std::runtime_error("unknown matrix dimension " + expression);
}
