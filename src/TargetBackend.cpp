#include "TargetBackend.h"
#include <fstream>
#include <iostream>
#include <regex>

namespace TargetBackend {

void emitISA(const std::vector<std::string>& instructions, const std::string& filename) {
    std::ofstream output(filename);
    if (!output.is_open()) {
        throw std::runtime_error("Could not open output file: " + filename);
    }
    
    for (const auto& instr : instructions) {
        output << instr << std::endl;
    }
    
    output.close();
    std::cout << "ISA instructions successfully written to " << filename << std::endl;
}

bool validateISA(const std::vector<std::string>& instructions) {
    int exeCount = 0;
    int endCount = 0;
    
    for (const auto& instr : instructions) {
        // Skip comments and empty lines
        if (instr.empty() || instr[0] == '#') {
            continue;
        }
        
        // Track EXE/END pairs
        if (instr.substr(0, 3) == "EXE") {
            exeCount++;
        } else if (instr.substr(0, 3) == "END") {
            endCount++;
        }
        
        // Additional checks for instruction syntax could be implemented here
    }
    
    // Basic check: ensure each EXE has a corresponding END
    return exeCount == endCount;
}

}
