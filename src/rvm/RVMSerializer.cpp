#include "RVMSerializer.h"
#include <algorithm>
#include <cctype>
#include <ranges>
#include <sstream>
#include <string_view>

namespace PExpr::rvm {
using namespace type;

std::string RVMSerializer::opcodeToString(Opcode op)
{
    switch (op) {
    // Arithmetic operations
    case Opcode::ADD:
        return "add";
    case Opcode::SUB:
        return "sub";
    case Opcode::MUL:
        return "mul";
    case Opcode::DIV:
        return "div";
    case Opcode::MOD:
        return "mod";
    case Opcode::POW:
        return "pow";

    // Bit operations
    case Opcode::AND:
        return "and";
    case Opcode::OR:
        return "or";
    case Opcode::XOR:
        return "xor";
    case Opcode::SHL:
        return "shl";
    case Opcode::SHR:
        return "shr";

    // Comparisons
    case Opcode::CMP_EQ:
        return "cmp_eq";
    case Opcode::CMP_NE:
        return "cmp_ne";
    case Opcode::CMP_LT:
        return "cmp_lt";
    case Opcode::CMP_LE:
        return "cmp_le";
    case Opcode::CMP_GT:
        return "cmp_gt";
    case Opcode::CMP_GE:
        return "cmp_ge";

    // Conversions
    case Opcode::I2F:
        return "i2f";
    case Opcode::F2I:
        return "f2i";
    case Opcode::B2I:
        return "b2i";
    case Opcode::I2B:
        return "i2b";
    case Opcode::F2B:
        return "f2b";
    case Opcode::B2F:
        return "b2f";

    // Memory/Data operations
    case Opcode::MOV:
        return "mov";

    // Control flow
    case Opcode::BR:
        return "br";
    case Opcode::BRZ:
        return "brz";
    case Opcode::BRNZ:
        return "brnz";
    case Opcode::JMP:
        return "jmp";
    case Opcode::CALL_EXTERNAL:
        return "call_external";
    case Opcode::CALL_INTERNAL:
        return "call_internal";
    case Opcode::RET:
        return "ret";

    // Register frame operations
    case Opcode::PUSH_FRAME:
        return "push_frame";
    case Opcode::POP_FRAME:
        return "pop_frame";

    default:
        return "unknown";
    }
}

Opcode RVMSerializer::stringToOpcode(const std::string& str)
{
    // Map string back to opcode
    if (str == "add")
        return Opcode::ADD;
    if (str == "sub")
        return Opcode::SUB;
    if (str == "mul")
        return Opcode::MUL;
    if (str == "div")
        return Opcode::DIV;
    if (str == "mod")
        return Opcode::MOD;
    if (str == "pow")
        return Opcode::POW;

    if (str == "and")
        return Opcode::AND;
    if (str == "or")
        return Opcode::OR;
    if (str == "xor")
        return Opcode::XOR;
    if (str == "shl")
        return Opcode::SHL;
    if (str == "shr")
        return Opcode::SHR;

    if (str == "cmp_eq")
        return Opcode::CMP_EQ;
    if (str == "cmp_ne")
        return Opcode::CMP_NE;
    if (str == "cmp_lt")
        return Opcode::CMP_LT;
    if (str == "cmp_le")
        return Opcode::CMP_LE;
    if (str == "cmp_gt")
        return Opcode::CMP_GT;
    if (str == "cmp_ge")
        return Opcode::CMP_GE;

    if (str == "i2f")
        return Opcode::I2F;
    if (str == "f2i")
        return Opcode::F2I;
    if (str == "b2i")
        return Opcode::B2I;
    if (str == "i2b")
        return Opcode::I2B;
    if (str == "f2b")
        return Opcode::F2B;
    if (str == "b2f")
        return Opcode::B2F;

    if (str == "mov")
        return Opcode::MOV;

    if (str == "br")
        return Opcode::BR;
    if (str == "brz")
        return Opcode::BRZ;
    if (str == "brnz")
        return Opcode::BRNZ;
    if (str == "jmp")
        return Opcode::JMP;
    if (str == "call_external")
        return Opcode::CALL_EXTERNAL;
    if (str == "call_internal")
        return Opcode::CALL_INTERNAL;
    if (str == "ret")
        return Opcode::RET;

    if (str == "push_frame")
        return Opcode::PUSH_FRAME;
    if (str == "pop_frame")
        return Opcode::POP_FRAME;

    return Opcode::ADD; // Default
}

void RVMSerializer::write(std::ostream& os, const ValueVariant& value)
{
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            os << (arg ? "true" : "false");
        } else if constexpr (std::is_same_v<T, Integer>) {
            os << arg;
        } else if constexpr (std::is_same_v<T, Number>) {
            os << arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            os << "\"" << escapeString(arg) << "\"";
        } else {
            // Note: Tuples should be dissolved before reaching RVM IR
            // so we don't handle Tuple type here
            PEXPR_ASSERT(false, "Invalid value variant given for RVM IR");
        }
    },
               value);
}

void RVMSerializer::write(std::ostream& os, const RVMValue& value)
{
    if (value.isConstant()) {
        write(os, value.constantValue());
        os << ":" << value.type().toString();
    } else if (value.isRegister()) {
        os << "%r" << value.regId() << ":" << value.type().toString();
    } else if (value.isStringRef()) {
        os << "#str" << value.stringId() << ":" << value.type().toString();
    }
}

void RVMSerializer::write2Op(std::ostream& os, const RVMInstr2Op& instr)
{
    write(os, instr.dst().value());
    os << " = " << opcodeToString(instr.opcode()) << " ";

    auto srcs = instr.srcs();
    if (!srcs.empty())
        write(os, srcs[0]);
}

void RVMSerializer::write3Op(std::ostream& os, const RVMInstr3Op& instr)
{
    write(os, instr.dst().value());
    os << " = " << opcodeToString(instr.opcode()) << " ";

    auto srcs = instr.srcs();
    if (srcs.size() >= 1) {
        write(os, srcs[0]);
    }
    if (srcs.size() >= 2) {
        os << ", ";
        write(os, srcs[1]);
    }
}

void RVMSerializer::writeBranch(std::ostream& os, const RVMInstrBranch& instr)
{
    os << opcodeToString(instr.opcode()) << " ";
    auto srcs = instr.srcs();
    if (!srcs.empty())
        write(os, srcs[0]);
    os << " -> " << instr.targetLabel();
}

void RVMSerializer::writeJump(std::ostream& os, const RVMInstrJump& instr)
{
    os << "jmp -> " << instr.targetLabel();
}

void RVMSerializer::writeLabel(std::ostream& os, const RVMInstrLabel& instr)
{
    os << instr.labelName() << ":";
}

void RVMSerializer::writeCall(std::ostream& os, const RVMInstrExternalCall& instr)
{
    if (instr.dst().has_value()) {
        write(os, instr.dst().value());
        os << " = ";
    }
    os << "call_external " << instr.functionName() << "(";

    auto srcs = instr.srcs();
    for (size_t i = 0; i < srcs.size(); ++i) {
        if (i > 0)
            os << ", ";
        write(os, srcs[i]);
    }
    os << ")";
}

void RVMSerializer::writeCall(std::ostream& os, const RVMInstrInternalCall& instr)
{
    os << "call_internal " << instr.functionName();
}

void RVMSerializer::writeReturn(std::ostream& os, const RVMInstrReturn& instr)
{
    os << "ret";
    if (instr.returnValue().has_value()) {
        os << " ";
        write(os, instr.returnValue().value());
    }
}

void RVMSerializer::writePushFrame(std::ostream& os, const RVMInstrPushFrame& instr)
{
    os << "push_frame " << instr.registerCount();
}

void RVMSerializer::writePopFrame(std::ostream& os, const RVMInstrPopFrame& instr)
{
    os << "pop_frame " << instr.registerCount();
}

void RVMSerializer::write(std::ostream& os, const RVMInstr& instr)
{
    if (const auto* label = dynamic_cast<const RVMInstrLabel*>(&instr))
        writeLabel(os, *label);
    else if (const auto* op2 = dynamic_cast<const RVMInstr2Op*>(&instr))
        write2Op(os, *op2);
    else if (const auto* op3 = dynamic_cast<const RVMInstr3Op*>(&instr))
        write3Op(os, *op3);
    else if (const auto* branch = dynamic_cast<const RVMInstrBranch*>(&instr))
        writeBranch(os, *branch);
    else if (const auto* jump = dynamic_cast<const RVMInstrJump*>(&instr))
        writeJump(os, *jump);
    else if (const auto* callE = dynamic_cast<const RVMInstrExternalCall*>(&instr))
        writeCall(os, *callE);
    else if (const auto* callI = dynamic_cast<const RVMInstrInternalCall*>(&instr))
        writeCall(os, *callI);
    else if (const auto* ret = dynamic_cast<const RVMInstrReturn*>(&instr))
        writeReturn(os, *ret);
    else if (const auto* pushFrame = dynamic_cast<const RVMInstrPushFrame*>(&instr))
        writePushFrame(os, *pushFrame);
    else if (const auto* popFrame = dynamic_cast<const RVMInstrPopFrame*>(&instr))
        writePopFrame(os, *popFrame);
    else {
        os << "unknown_instr";
    }
}

void RVMSerializer::write(std::ostream& os, const RVMFunction& func)
{
    if (func.External) {
        os << "[[extern";
        if (!func.HasSideEffect)
            os << ", pure";
        os << "]] ";
    }

    os << "fn " << func.Name << "(";
    for (size_t i = 0; i < func.Parameters.size(); ++i) {
        if (i > 0)
            os << ", ";
        os << "%p" << i << ":" << func.Parameters[i].toString();
    }
    os << ") : " << func.ReturnType.toString() << std::endl;

    if (!func.Body.empty()) {
        for (const auto& instr : func.Body) {
            os << "  ";
            write(os, *instr);
            os << std::endl;
        }
    }
    os << "endfn" << std::endl;
}

void RVMSerializer::write(std::ostream& os, const RVMProgram& program)
{
    // Write string table if present
    if (program.StringTable && program.StringTable->size() > 0) {
        os << "[[strings]]" << std::endl;
        for (uint32_t i = 0; i < program.StringTable->size(); ++i)
            os << "  #str" << i << " = \"" << escapeString(program.StringTable->getString(i)) << "\"" << std::endl;
        os << std::endl;
    }

    // Write functions
    for (const auto& f : program.Functions) {
        write(os, f);
        os << std::endl;
    }

    // Write main body
    for (const auto& instr : program.Body) {
        write(os, *instr);
        os << std::endl;
    }
}

std::string RVMSerializer::serialize(const RVMProgram& program)
{
    std::ostringstream oss;
    write(oss, program);
    return oss.str();
}

// Note: Deserialization implementation would be more complex
// and require parsing logic similar to SSASerializer

std::string RVMSerializer::escapeString(const std::string& str)
{
    std::string result;
    result.reserve(str.size());

    for (char c : str) {
        switch (c) {
        case '\"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
            break;
        }
    }

    return result;
}

std::string RVMSerializer::unescapeString(const std::string& str)
{
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
            case '\"':
                result += '\"';
                ++i;
                break;
            case '\\':
                result += '\\';
                ++i;
                break;
            case 'n':
                result += '\n';
                ++i;
                break;
            case 'r':
                result += '\r';
                ++i;
                break;
            case 't':
                result += '\t';
                ++i;
                break;
            default:
                result += str[i];
                break;
            }
        } else {
            result += str[i];
        }
    }

    return result;
}

Type RVMSerializer::parseType(const std::string& typeStr)
{
    // Basic type parsing (same as SSASerializer)
    if (typeStr == "bool")
        return Type(TypeKind::Boolean);
    if (typeStr == "int")
        return Type(TypeKind::Integer);
    if (typeStr == "num")
        return Type(TypeKind::Number);
    if (typeStr == "str")
        return Type(TypeKind::String);
    // No support for tuples/vectors in RVM IR
    return Type(TypeKind::Unspecified);
}

// Helper functions for parsing
static std::string trim(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos)
        return std::string();
    size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

static std::vector<std::string> split(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        token = trim(token);
        if (!token.empty())
            tokens.push_back(token);
    }
    return tokens;
}

bool RVMSerializer::parseValue(const std::string& str, RVMValue& outValue)
{
    // Format: value:type or %rN:type or #strN:type
    size_t colon = str.find(':');
    if (colon == std::string::npos)
        return false;

    std::string valueStr = str.substr(0, colon);
    std::string typeStr  = trim(str.substr(colon + 1));

    Type type = parseType(typeStr);
    if (type.kind() == TypeKind::Unspecified)
        return false;

    // Check for register: %rN
    if (valueStr.rfind("%r", 0) == 0) {
        try {
            uint32_t regId = std::stoul(valueStr.substr(2));
            outValue       = RVMValue::Register(regId, type);
            return true;
        } catch (...) {
            return false;
        }
    }

    // Check for string reference: #strN
    if (valueStr.rfind("#str", 0) == 0) {
        try {
            uint32_t strId = std::stoul(valueStr.substr(4));
            outValue       = RVMValue::StringRef(strId, type);
            return true;
        } catch (...) {
            return false;
        }
    }

    // Check for constants
    if (valueStr == "true" || valueStr == "false") {
        bool val = (valueStr == "true");
        outValue = RVMValue::Constant(val);
        return true;
    }

    // Integer constant
    if (valueStr.find_first_not_of("0123456789-") == std::string::npos) {
        try {
            Integer val = std::stoll(valueStr);
            if (type.kind() == TypeKind::Number)
                outValue = RVMValue::Constant(static_cast<Number>(val));
            else
                outValue = RVMValue::Constant(val);
            return true;
        } catch (...) {
            return false;
        }
    }

    // Number constant (contains dot or scientific notation)
    if (valueStr.find('.') != std::string::npos || valueStr.find('e') != std::string::npos || valueStr.find('E') != std::string::npos) {
        bool allDigitsOrDotOrSign = std::ranges::all_of(valueStr, [](char c) {
            return isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E';
        });
        if (allDigitsOrDotOrSign) {
            try {
                Number val = std::stod(valueStr);
                outValue   = RVMValue::Constant(val);
                return true;
            } catch (...) {
                return false;
            }
        }
    }

    // String constant (quoted) - treat as string reference
    // Note: In RVM, strings are stored in string table
    // This would need to be added to string table during deserialization
    if (valueStr.front() == '"' && valueStr.back() == '"') {
        // For now, we can't create string constants directly
        // This needs to be handled at a higher level during deserialization
        return false;
    }

    return false;
}

std::vector<RVMValue> RVMSerializer::parseValueList(const std::string& str)
{
    std::vector<RVMValue> values;
    std::vector<std::string> tokens = split(str, ',');
    for (const auto& token : tokens) {
        RVMValue val;
        if (parseValue(token, val))
            values.push_back(val);
    }
    return values;
}

std::shared_ptr<RVMInstr> RVMSerializer::readInstruction(const std::string& line)
{
    std::string trimmed = trim(line);
    if (trimmed.empty())
        return nullptr;

    // Check for push/pop frame
    if (trimmed.rfind("push_frame ", 0) == 0 || trimmed == "push_frame") {
        uint32_t count = 0;
        if (trimmed.size() > 11) {
            try {
                count = std::stoul(trimmed.substr(11));
            } catch (...) {
                count = 0;
            }
        }
        return std::make_shared<RVMInstrPushFrame>(count);
    }
    if (trimmed.rfind("pop_frame ", 0) == 0 || trimmed == "pop_frame") {
        uint32_t count = 0;
        if (trimmed.size() > 10) {
            try {
                count = std::stoul(trimmed.substr(10));
            } catch (...) {
                count = 0;
            }
        }
        return std::make_shared<RVMInstrPopFrame>(count);
    }

    // Check for label (format: labelname:)
    if (trimmed.back() == ':' && trimmed.find('=') == std::string::npos) {
        std::string labelName = trimmed.substr(0, trimmed.size() - 1);
        return std::make_shared<RVMInstrLabel>(labelName);
    }

    // Check for jump (format: jmp -> labelname)
    if (trimmed.rfind("jmp -> ", 0) == 0) {
        std::string labelName = trimmed.substr(7);
        return std::make_shared<RVMInstrJump>(labelName);
    }

    // Check for branch (format: opcode value -> labelname)
    if (trimmed.find(" -> ") != std::string::npos) {
        size_t arrow       = trimmed.find(" -> ");
        std::string before = trim(trimmed.substr(0, arrow));
        std::string after  = trim(trimmed.substr(arrow + 4));

        // Parse opcode and value
        size_t space = before.find(' ');
        if (space == std::string::npos)
            return nullptr;

        std::string opcodeStr = trim(before.substr(0, space));
        std::string valueStr  = trim(before.substr(space + 1));

        RVMValue value;
        if (!parseValue(valueStr, value))
            return nullptr;

        Opcode op = stringToOpcode(opcodeStr);
        return std::make_shared<RVMInstrBranch>(op, value, after);
    }

    // Check for return
    if (trimmed.rfind("ret", 0) == 0) {
        if (trimmed.size() == 3) {
            return std::make_shared<RVMInstrReturn>(std::nullopt);
        } else {
            std::string valueStr = trim(trimmed.substr(4));
            RVMValue value;
            if (!parseValue(valueStr, value))
                return nullptr;
            return std::make_shared<RVMInstrReturn>(value);
        }
    }

    // Check for assignment instructions (dst = op ...)
    size_t eqPos = trimmed.find('=');
    if (eqPos != std::string::npos) {
        std::string dstStr = trim(trimmed.substr(0, eqPos));
        std::string rest   = trim(trimmed.substr(eqPos + 1));

        RVMValue dst;
        if (!parseValue(dstStr, dst))
            return nullptr;

        // Check for mov
        if (rest.rfind("mov ", 0) == 0) {
            std::string srcStr = trim(rest.substr(4));
            RVMValue src;
            if (!parseValue(srcStr, src))
                return nullptr;
            return std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src);
        }

        // Check for 2-operand or 3-operand instruction
        size_t space = rest.find(' ');
        if (space != std::string::npos) {
            std::string opcodeStr = trim(rest.substr(0, space));
            std::string argsStr   = trim(rest.substr(space + 1));

            Opcode op                          = stringToOpcode(opcodeStr);
            std::vector<std::string> argTokens = split(argsStr, ',');

            if (argTokens.size() == 1) {
                // 2-operand instruction: dst = op src
                RVMValue src;
                if (parseValue(argTokens[0], src))
                    return std::make_shared<RVMInstr2Op>(op, dst, src);
            } else if (argTokens.size() == 2) {
                // 3-operand instruction: dst = op src1, src2
                RVMValue src1, src2;
                if (parseValue(argTokens[0], src1) && parseValue(argTokens[1], src2))
                    return std::make_shared<RVMInstr3Op>(op, dst, src1, src2);
            }
        }

        // Check for call_external
        if (rest.rfind("call_external ", 0) == 0) {
            size_t parenStart = rest.find('(');
            size_t parenEnd   = rest.find(')', parenStart);
            if (parenStart == std::string::npos || parenEnd == std::string::npos)
                return nullptr;

            std::string funcName = trim(rest.substr(15, parenStart - 5));
            std::string argsStr  = rest.substr(parenStart + 1, parenEnd - parenStart - 1);

            std::vector<RVMValue> args = parseValueList(argsStr);
            return std::make_shared<RVMInstrExternalCall>(dst, funcName, args);
        }

        // Check for call_internal
        if (rest.rfind("call_internal ", 0) == 0) {
            std::string funcName = trim(rest.substr(15));
            return std::make_shared<RVMInstrInternalCall>(funcName);
        }
    }

    return nullptr;
}

RVMProgram RVMSerializer::read(std::istream& is)
{
    RVMProgram program;
    auto stringTable    = std::make_shared<RVMStringTable>();
    program.StringTable = stringTable;

    std::string line;
    std::shared_ptr<RVMFunction> currentFunction = nullptr;

    while (std::getline(is, line)) {
        line = trim(line);
        if (line.empty())
            continue;

        // Check for end of function
        if (line == "endfn") {
            if (currentFunction) {
                program.Functions.push_back(std::move(*currentFunction));
                currentFunction.reset();
            }
            continue;
        }

        // Check for string table
        if (line == "[[strings]]") {
            // Read string table entries
            while (std::getline(is, line)) {
                line = trim(line);
                if (line.empty())
                    break;

                if (line.rfind("#str", 0) == 0 && line.find('=') != std::string::npos) {
                    size_t eqPos         = line.find('=');
                    std::string idStr    = trim(line.substr(0, eqPos));
                    std::string valueStr = trim(line.substr(eqPos + 1));

                    if (valueStr.front() == '"' && valueStr.back() == '"') {
                        std::string value = unescapeString(valueStr.substr(1, valueStr.size() - 2));
                        // Store in string table (id will be assigned in order)
                        stringTable->addString(value);
                    }
                }
            }
            continue;
        }

        // Check for function declaration
        if (line.rfind("fn ", 0) == 0) {
            currentFunction = std::make_shared<RVMFunction>();

            // Parse function header (simplified)
            // Format: fn name(params) : returnType
            size_t parenStart = line.find('(');
            size_t parenEnd   = line.find(')', parenStart);

            if (parenStart != std::string::npos && parenEnd != std::string::npos) {
                currentFunction->Name = trim(line.substr(3, parenStart - 3));

                // Parse parameters (simplified)
                std::string paramsStr = line.substr(parenStart + 1, parenEnd - parenStart - 1);
                if (!paramsStr.empty()) {
                    std::vector<std::string> paramTokens = split(paramsStr, ',');
                    for (const auto& param : paramTokens) {
                        size_t colon = param.find(':');
                        if (colon != std::string::npos) {
                            Type type = parseType(trim(param.substr(colon + 1)));
                            currentFunction->Parameters.push_back(type);
                        }
                    }
                }

                // Parse return type
                size_t colonPos = line.find(':', parenEnd);
                if (colonPos != std::string::npos) {
                    std::string returnTypeStr   = trim(line.substr(colonPos + 1));
                    currentFunction->ReturnType = parseType(returnTypeStr);
                }
            }
            continue;
        }

        // Parse instruction
        auto instr = readInstruction(line);
        if (instr) {
            if (currentFunction)
                currentFunction->Body.push_back(instr);
            else
                program.Body.push_back(instr);
        }
    }

    return program;
}

RVMProgram RVMSerializer::deserialize(const std::string& str)
{
    std::istringstream iss(str);
    return read(iss);
}

} // namespace PExpr::rvm
