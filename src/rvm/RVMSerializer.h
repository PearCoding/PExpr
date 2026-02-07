#pragma once

#include "RVMProgram.h"
#include "RVMInstruction.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

namespace PExpr::rvm {

/// Serialization class for RVM IR that supports reading from and writing to streams.
/// Provides both human-readable text serialization and deserialization capabilities.
class RVMSerializer {
public:
    /// Write an RVMProgram to a stream in human-readable format
    static void write(std::ostream& os, const RVMProgram& program);

    /// Write an RVMInstr to a stream in human-readable format
    static void write(std::ostream& os, const RVMInstr& instr);

    /// Write an RVMValue to a stream in human-readable format
    static void write(std::ostream& os, const RVMValue& value);

    /// Write a ValueVariant to a stream in human-readable format
    static void write(std::ostream& os, const ValueVariant& value);

    /// Read an RVMProgram from a stream (deserialize)
    static RVMProgram read(std::istream& is);

    /// Helper: Serialize an RVMProgram to a string (convenience wrapper)
    [[nodiscard]] static std::string serialize(const RVMProgram& program);

    /// Helper: Deserialize an RVMProgram from a string (convenience wrapper)
    [[nodiscard]] static RVMProgram deserialize(const std::string& str);

    /// Escape special characters in a string for serialization
    static std::string escapeString(const std::string& str);

    /// Unescape special characters in a string after deserialization
    static std::string unescapeString(const std::string& str);

    /// Parse a type string to Type
    static type::Type parseType(const std::string& typeStr);

    /// Convert opcode to string representation
    static std::string opcodeToString(Opcode op);

    /// Convert string to opcode
    static Opcode stringToOpcode(const std::string& str);

    /// Parse an instruction from a line of text (used for testing)
    static std::shared_ptr<RVMInstr> readInstruction(const std::string& line);

    /// Parse a value from string representation
    static bool parseValue(const std::string& str, RVMValue& outValue);

private:
    // Internal helper functions for writing specific instruction types
    static void write2Op(std::ostream& os, const RVMInstr2Op& instr);
    static void write3Op(std::ostream& os, const RVMInstr3Op& instr);
    static void writeBranch(std::ostream& os, const RVMInstrBranch& instr);
    static void writeJump(std::ostream& os, const RVMInstrJump& instr);
    static void writeComment(std::ostream& os, const RVMInstrComment& instr);
    static void writeLabel(std::ostream& os, const RVMInstrLabel& instr);
    static void writeCall(std::ostream& os, const RVMInstrExternalCall& instr);
    static void writeCall(std::ostream& os, const RVMInstrInternalCall& instr);
    static void writeReturn(std::ostream& os, const RVMInstrReturn& instr);
    static void writePushFrame(std::ostream& os, const RVMInstrPushFrame& instr);
    static void writePopFrame(std::ostream& os, const RVMInstrPopFrame& instr);

    // Internal helper functions for reading/parsing
    static std::vector<RVMValue> parseValueList(const std::string& str);
};

} // namespace PExpr::rvm