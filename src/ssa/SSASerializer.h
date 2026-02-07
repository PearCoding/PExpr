#pragma once

#include "SSAStructs.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <span>
#include <vector>

namespace PExpr::ssa {

/// Serialization class for SSA IR that supports reading from and writing to streams.
/// Provides both human-readable text serialization and deserialization capabilities.
class SSASerializer {
public:
    /// Write an SSAProgram to a stream in human-readable format
    static void write(std::ostream& os, const SSAProgram& program);

    /// Write an SSAFunction to a stream in human-readable format
    static void write(std::ostream& os, const SSAFunction& func);

    /// Write an SSAInstr to a stream in human-readable format
    static void write(std::ostream& os, const SSAInstr& instr);

    /// Write an SSAValue to a stream in human-readable format
    static void write(std::ostream& os, const SSAValue& value);

    /// Write an SSAValue to a stream in human-readable format
    static void write(std::ostream& os, const type::Type& type, const ValueVariant& value, bool withTypeSuffix);

    /// Read an SSAProgram from a stream (deserialize)
    static SSAProgram read(std::istream& is);

    /// Helper: Serialize an list of instructions to a string (convenience wrapper)
    [[nodiscard]] static std::string serialize(std::span<const std::shared_ptr<SSAInstr>> instructions);

    /// Helper: Serialize an SSAProgram to a string (convenience wrapper)
    [[nodiscard]] static std::string serialize(const SSAProgram& program);

    /// Helper: Deserialize an SSAProgram from a string (convenience wrapper)
    [[nodiscard]] static SSAProgram deserialize(const std::string& str);

    /// Escape special characters in a string for serialization
    static std::string escapeString(const std::string& str);

    /// Unescape special characters in a string after deserialization
    static std::string unescapeString(const std::string& str);

    /// Parse a type string to Type (useful for testing)
    static type::Type parseType(const std::string& typeStr);

private:
    // Internal helper functions for writing specific instruction types
    static void writeAssign(std::ostream& os, const SSAInstrAssign& instr);
    static void writeCall(std::ostream& os, const SSAInstrCall& instr);
    static void writeReturn(std::ostream& os, const SSAInstrReturn& instr);
    static void writeLabel(std::ostream& os, const SSAInstrLabel& instr);
    static void writeBranch(std::ostream& os, const SSAInstrBranch& instr);
    static void writeGoto(std::ostream& os, const SSAInstrGoto& instr);
    static void writePhi(std::ostream& os, const SSAInstrPhi& instr);

    // Internal helper functions for reading/parsing
    static std::shared_ptr<SSAInstr> readInstruction(const std::string& line);
    static bool parseValue(const std::string& str, SSAValue& outValue);
    static std::vector<SSAValue> parseValueList(const std::string& str);
};

} // namespace PExpr::ssa