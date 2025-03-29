# PIM Compiler

A compiler that generates PIM (Processing-In-Memory) ISA code from matrix operation specifications.

## Features

- Converts matrix operations to PIM ISA instructions
- Supports matrix multiplication, addition, and other linear algebra operations
- Automatic memory allocation and management
- Generates optimized PIM instruction streams

## Project Structure

```
PIM_Compiler/
├── include/               # Header files
│   ├── Lexer.h
│   ├── Parser.h
│   ├── CodeGen.h
│   └── TargetBackend.h
├── src/                   # Source files
│   ├── Lexer.cpp
│   ├── Parser.cpp
│   ├── CodeGen.cpp
│   ├── TargetBackend.cpp
│   └── main.cpp
├── tests/                 # Test cases
│   ├── test1.cpp
│   ├── test2.cpp
│   └── ...
├── CMakeLists.txt         # Build configuration
└── README.md              # This file
```

## Prerequisites

- C++17 compatible compiler
- CMake (≥ 3.10)
- LLVM (optional, for advanced features)

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
./build/PIM_Compiler tests/test1.cpp -o output.isa
```

Example test.cpp:
```cpp
#include <iostream>
#define SIZE 4

void matmul(int X[SIZE][SIZE], int Y[SIZE][SIZE], int Z[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            Z[i][j] = 0;
            for (int k = 0; k < SIZE; k++) {
                Z[i][j] += X[i][k] * Y[k][j];
            }
        }
    }
}

int main() {
    int X[SIZE][SIZE] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    int Y[SIZE][SIZE] = {{2,2,2,2},{2,2,2,2},{2,2,2,2},{2,2,2,2}};
    int Z[SIZE][SIZE];
    
    matmul(X, Y, Z);
    return 0;
}

```

## Example Output

```isa
# MEMORY CONFIGURATION
ALLOCATE 0x0000 0xFFFF

# Define the MAC (Multiply-Accumulate) operation for dot product
# First program the MAC function into the pPIM core
PROG r0, mac_operation
# MAC operation microcode
EXE MUL r1, ah, bh  # Multiply high bits
EXE MUL r2, al, bl  # Multiply low bits
EXE MUL r3, ah, bl  # Multiply high with low
EXE MUL r4, al, bh  # Multiply low with high
EXE ADD r5, r3, r4  # Combine cross products
EXE ADD r6, r1, r2  # Combine direct products
EXE ADD r0, r5, r6  # Final result
END mac_operation

# Define a matrix multiplication operation
# Program the matrix multiplication function into the pPIM core
PROG r2, matrix_multiply
# Matrix multiplication microcode
EXE ADD r0, r1, r2  # Addition operation: r0 = r1 + r2
EXE MUL r0, r1, r2  # Multiplication operation: r0 = r1 * r2
EXE ZERO r0         # Zero register: r0 = 0
# Matrix multiplication implementation for 4x4 matrices
# For each element of the result matrix
# Z[i][j] = sum(X[i][k] * Y[k][j]) for all k
EXE ZERO acc                # Initialize accumulator to 0
EXE READ r1, X_addr[i][k]   # Load X[i][k]
EXE READ r2, Y_addr[k][j]   # Load Y[k][j]
EXE MUL r3, r1, r2          # r3 = X[i][k] * Y[k][j]
EXE ADD acc, acc, r3        # acc += r3
EXE WRITE Z_addr[i][j], acc # Store result to Z[i][j]
END matrix_multiply

# MATRIX ALLOCATIONS
# Matrix X allocated at 0x1000
# Matrix Y allocated at 0x1040
# Matrix Z allocated at 0x1080

# MATRIX OPERATIONS
# MATRIX MULTIPLICATION X * Y -> Z
EXE r2, 0x1000, 0x1040, 0x1080, 4

# MEMORY RELEASE
FREE 0x1080 64
FREE 0x1040 64
FREE 0x1000 64
END
```

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## Contact

Sujith S - [LinkedIn](https://www.linkedin.com/in/sujith-s-62aa7527a/) - sujithsures@gmail.com
Project Link: [https://github.com/zenistu17/PIM_Compiler](https://github.com/zenistu17/PIM_Compiler)
