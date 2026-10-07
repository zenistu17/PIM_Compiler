#include "TargetBackend.h"

#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace TargetBackend {
namespace {

std::string trim(const std::string& input) {
    std::size_t first = 0;
    while (first < input.size() && std::isspace(static_cast<unsigned char>(input[first]))) {
        ++first;
    }
    std::size_t last = input.size();
    while (last > first && std::isspace(static_cast<unsigned char>(input[last - 1]))) {
        --last;
    }
    return input.substr(first, last - first);
}

bool parseInteger(const std::string& text, std::size_t& value) {
    try {
        std::size_t consumed = 0;
        const bool hexadecimal = text.size() > 2 && text[0] == '0' &&
                                 (text[1] == 'x' || text[1] == 'X');
        value = std::stoull(text, &consumed, hexadecimal ? 16 : 10);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

bool parseRange(const std::string& line, std::size_t& start, std::size_t& end) {
    static const std::regex pattern(
        R"(^ALLOCATE\s+(0[xX][0-9a-fA-F]+|[0-9]+)\s+(0[xX][0-9a-fA-F]+|[0-9]+)$)");
    std::smatch match;
    if (!std::regex_match(line, match, pattern)) {
        return false;
    }
    return parseInteger(match[1].str(), start) && parseInteger(match[2].str(), end) && start < end;
}

bool isCommentOrBlank(const std::string& line) {
    return line.empty() || line.front() == '#';
}
} // namespace

void emitISA(const std::vector<std::string>& instructions, const std::string& filename) {
    std::ofstream output(filename);
    if (!output.is_open()) {
        throw std::runtime_error("Could not open output file: " + filename);
    }

    for (const auto& instruction : instructions) {
        output << instruction << '\n';
    }
    if (!output.good()) {
        throw std::runtime_error("Could not write ISA output: " + filename);
    }
}

bool validateISA(const std::vector<std::string>& instructions) {
    // A program consists of zero or more named PROG/END blocks followed by a
    // single bare END. EXE instructions are legal both inside a block and at
    // runtime, so counting EXE and END (the old implementation) is invalid.
    static const std::regex progPattern(
        R"(^PROG\s+r[0-9]+\s*,\s*([A-Za-z_][A-Za-z0-9_]*)$)");
    static const std::regex namedEndPattern(
        R"(^END\s+([A-Za-z_][A-Za-z0-9_]*)$)");
    static const std::regex exePattern(R"(^EXE\s+.+$)");
    static const std::regex freePattern(
        R"(^FREE\s+(0[xX][0-9a-fA-F]+|[0-9]+)\s+([0-9]+)$)");

    bool inProgram = false;
    bool finalEnd = false;
    std::string programName;
    std::size_t allocationStart = 0;
    std::size_t allocationEnd = 0;
    bool sawAllocation = false;

    for (const std::string& raw : instructions) {
        const std::string line = trim(raw);
        if (isCommentOrBlank(line)) {
            continue;
        }
        if (finalEnd) {
            // Emitting anything after the program terminator is almost always
            // a truncated/concatenated output file.
            return false;
        }

        if (line.rfind("ALLOCATE ", 0) == 0) {
            std::size_t start = 0;
            std::size_t end = 0;
            if (!parseRange(line, start, end)) {
                return false;
            }
            if (sawAllocation && (start != allocationStart || end != allocationEnd)) {
                return false;
            }
            sawAllocation = true;
            allocationStart = start;
            allocationEnd = end;
            continue;
        }

        std::smatch match;
        if (std::regex_match(line, match, progPattern)) {
            if (inProgram) {
                return false;
            }
            inProgram = true;
            programName = match[1].str();
            continue;
        }

        if (std::regex_match(line, match, namedEndPattern)) {
            if (!inProgram || match[1].str() != programName) {
                return false;
            }
            inProgram = false;
            programName.clear();
            continue;
        }

        if (line == "END") {
            if (inProgram) {
                return false;
            }
            finalEnd = true;
            continue;
        }

        if (std::regex_match(line, exePattern)) {
            continue;
        }

        if (std::regex_match(line, match, freePattern)) {
            std::size_t address = 0;
            std::size_t bytes = 0;
            if (!parseInteger(match[1].str(), address) || !parseInteger(match[2].str(), bytes) ||
                bytes == 0) {
                return false;
            }
            if (sawAllocation &&
                (address < allocationStart || address > allocationEnd ||
                 bytes - 1 > allocationEnd - address)) {
                return false;
            }
            continue;
        }

        // Unknown opcodes and malformed lines should not be silently accepted.
        return false;
    }

    return finalEnd && !inProgram;
}

} // namespace TargetBackend
