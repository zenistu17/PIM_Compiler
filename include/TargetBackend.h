// TargetBackend.h
#ifndef TARGET_BACKEND_H
#define TARGET_BACKEND_H

#include <string>
#include <vector>

namespace TargetBackend {
    // Emit ISA instructions to a file
    void emitISA(const std::vector<std::string>& instructions, const std::string& filename);
    
    // Validate ISA instruction correctness
    bool validateISA(const std::vector<std::string>& instructions);
}

#endif


