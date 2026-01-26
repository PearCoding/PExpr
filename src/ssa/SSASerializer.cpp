#include "SSASerializer.h"
#include "Enums.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <sstream>
#include <string_view>

namespace PExpr::ssa {

// Helper functions from SSAStructs.cpp
static inline std::string_view toInstructionString(UnaryOperation op)
{
    switch (op) {
    case UnaryOperation::Pos:
        return "pos";
    case UnaryOperation::Neg:
        return "neg";
    case UnaryOperation::Not:
        return "not";
    default:
        PEXPR_ASSERT(false, "Invalid unary operation enum");
        return "";
    }
}

static inline std::string_view toInstructionString(BinaryOperation op)
{
    switch (op) {
    case BinaryOperation::Add:
        return "add";
    case BinaryOperation::Sub:
        return "sub";
    case BinaryOperation::Mul:
        return "mul";
    case BinaryOperation::Div:
        return "div";
    case BinaryOperation::Pow:
        return "pow";
    case BinaryOperation::Mod:
        return "mod";
    case BinaryOperation::And:
        return "and";
    case BinaryOperation::Or:
        return "or";
    case BinaryOperation::Less:
        return "ls";
    case BinaryOperation::Greater:
        return "gt";
    case BinaryOperation::LessEqual:
        return "le";
    case BinaryOperation::GreaterEqual:
        return "ge";
    case BinaryOperation::Equal:
        return "eq";
    case BinaryOperation::NotEqual:
        return "neq";
    default:
        PEXPR_ASSERT(false, "Invalid binary operation enum");
        return "";
    }
}

// Helper function to write value to stream
void SSASerializer::write(std::ostream& os, const Type& type, const ValueVariant& value, bool withTypeSuffix)
{
    if (const auto* b = std::get_if<bool>(&value)) {
        PEXPR_ASSERT(type.kind() == TypeKind::Boolean, "Expected variant to have a bool value");
        os << (*b ? "true" : "false");
    } else if (const auto* i = std::get_if<Integer>(&value)) {
        PEXPR_ASSERT(type.kind() == TypeKind::Integer, "Expected variant to have a Integer value");
        os << *i;
    } else if (const auto* n = std::get_if<Number>(&value)) {
        PEXPR_ASSERT(type.kind() == TypeKind::Number, "Expected variant to have a Number value");
        os << *n;
    } else if (const auto* s = std::get_if<std::string>(&value)) {
        PEXPR_ASSERT(type.kind() == TypeKind::String, "Expected variant to have a string value");
        os << "\"" << *s << "\"";
    } else if (const auto* tp = std::get_if<Tuple>(&value)) {
        PEXPR_ASSERT(type.kind() == TypeKind::Tuple, "Expected variant to have a Tuple value");
        const auto& t = *tp;

        os << "[";
        for (size_t i = 0; i < t->elements.size(); ++i) {
            if (i)
                os << ", ";
            write(os, type.components().at(i), t->elements[i], withTypeSuffix);
        }
        os << "]";
    } else {
        PEXPR_ASSERT(false, "Non exhaustive SSASerializer vor ValueVariant");
    }

    if (withTypeSuffix)
        os << ":" << type.toString();
}

// Helper function to write value to stream
void SSASerializer::write(std::ostream& os, const SSAValue& value)
{
    if (value.isConstant())
        write(os, value.type(), value.rawValue(), true);
    else
        os << value.name() << ":" << value.type().toString();
}

void SSASerializer::writeAssign(std::ostream& os, const SSAInstrAssign& instr)
{
    write(os, instr.Target);
    os << " = ";

    switch (instr.Operator) {
    case SSAInstrAssign::OpKind::Assign:
        os << "assign";
        break;
    case SSAInstrAssign::OpKind::Unary:
        os << toInstructionString(instr.UnaryOp);
        break;
    case SSAInstrAssign::OpKind::Binary:
        os << toInstructionString(instr.BinaryOp);
        break;
    case SSAInstrAssign::OpKind::Swizzle:
        os << "swizzle[" << instr.Swizzle << "]";
        break;
    case SSAInstrAssign::OpKind::Access:
        os << "access";
        break;
    case SSAInstrAssign::OpKind::Vector:
        os << "vec[" << instr.Operands.size() << "]";
        break;
    case SSAInstrAssign::OpKind::Cast:
        os << "cast";
        break;
    default:
        PEXPR_ASSERT(false, "Unknown operator");
        os << "unknown";
        break;
    }

    os << "(";
    for (size_t i = 0; i < instr.Operands.size(); ++i) {
        if (i)
            os << ", ";
        write(os, instr.Operands[i]);
    }
    os << ")";
}

void SSASerializer::writeCall(std::ostream& os, const SSAInstrCall& instr)
{
    write(os, instr.Target);
    os << " = call[" << instr.FunctionName << "](";
    for (size_t i = 0; i < instr.Arguments.size(); ++i) {
        if (i)
            os << ", ";
        write(os, instr.Arguments[i]);
    }
    os << ")";
}

void SSASerializer::writeReturn(std::ostream& os, const SSAInstrReturn& instr)
{
    os << "return ";
    write(os, instr.Value);
}

void SSASerializer::writeLabel(std::ostream& os, const SSAInstrLabel& instr)
{
    os << instr.Name << ":";
}

void SSASerializer::writeBranch(std::ostream& os, const SSAInstrBranch& instr)
{
    os << "br ";
    write(os, instr.Condition);
    os << " -> " << instr.TargetLabel;
}

void SSASerializer::writeGoto(std::ostream& os, const SSAInstrGoto& instr)
{
    os << "goto " << instr.TargetLabel;
}

void SSASerializer::writePhi(std::ostream& os, const SSAInstrPhi& instr)
{
    write(os, instr.Target);
    os << " = phi[";
    for (size_t i = 0; i < instr.Conditions.size(); ++i) {
        if (i)
            os << ", ";
        write(os, instr.Conditions[i]);
    }
    os << "](";
    for (size_t i = 0; i < instr.Branches.size(); ++i) {
        if (i)
            os << ", ";
        write(os, instr.Branches[i]);
    }
    os << ")";
}

void SSASerializer::write(std::ostream& os, const SSAInstr& instr)
{
    if (const auto* assign = dynamic_cast<const SSAInstrAssign*>(&instr)) {
        writeAssign(os, *assign);
    } else if (const auto* call = dynamic_cast<const SSAInstrCall*>(&instr)) {
        writeCall(os, *call);
    } else if (const auto* ret = dynamic_cast<const SSAInstrReturn*>(&instr)) {
        writeReturn(os, *ret);
    } else if (const auto* label = dynamic_cast<const SSAInstrLabel*>(&instr)) {
        writeLabel(os, *label);
    } else if (const auto* branch = dynamic_cast<const SSAInstrBranch*>(&instr)) {
        writeBranch(os, *branch);
    } else if (const auto* gotoInstr = dynamic_cast<const SSAInstrGoto*>(&instr)) {
        writeGoto(os, *gotoInstr);
    } else if (const auto* phi = dynamic_cast<const SSAInstrPhi*>(&instr)) {
        writePhi(os, *phi);
    } else {
        PEXPR_ASSERT(false, "Unknown instruction type");
    }
}

void SSASerializer::write(std::ostream& os, const SSAFunction& func)
{
    if (func.External) {
        os << "[[extern";
        if (!func.HasSideEffect)
            os << ", pure";
        os << "]] ";
    }

    os << "fn " << func.Name << "(";
    for (size_t i = 0; i < func.Parameters.size(); ++i) {
        if (i)
            os << ", ";
        os << func.Parameters[i];
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

void SSASerializer::write(std::ostream& os, const SSAProgram& program)
{
    for (const auto& f : program.Functions) {
        write(os, f);
        os << std::endl;
    }

    for (const auto& instr : program.Body) {
        write(os, *instr);
        os << std::endl;
    }
}

std::string SSASerializer::serialize(const SSAProgram& program)
{
    std::ostringstream oss;
    write(oss, program);
    return oss.str();
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

// Helper function to strip comments from a line
static std::string stripComments(const std::string& line, bool& inBlockComment)
{
    std::string result;
    result.reserve(line.size());

    for (size_t i = 0; i < line.size(); ++i) {
        if (inBlockComment) {
            // Check for end of block comment
            if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '/') {
                inBlockComment = false;
                ++i; // Skip the '/'
            }
            continue;
        }

        // Check for start of block comment
        if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '*') {
            inBlockComment = true;
            ++i; // Skip the '*'
            continue;
        }

        // Check for line comment
        if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') {
            // Line comment, ignore the rest of the line
            break;
        }

        result += line[i];
    }

    return result;
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

Type SSASerializer::parseType(const std::string& typeStr)
{
    if (typeStr == "bool") {
        return Type(TypeKind::Boolean);
    } else if (typeStr == "int") {
        return Type(TypeKind::Integer);
    } else if (typeStr == "num") {
        return Type(TypeKind::Number);
    } else if (typeStr == "str") {
        return Type(TypeKind::String);
    } else if (typeStr.rfind("vec", 0) == 0) {
        // Vector type like vec1, vec2, vec3, vec4, ...
        try {
            size_t num = std::stoul(typeStr.substr(3));
            if (num >= 1)
                return Type::AsVector(num);
        } catch (...) {
            // fall through
        }
        return Type(TypeKind::Unspecified);
    } else {
        // TODO Tuple!
        return Type(TypeKind::Unspecified);
    }
}

bool SSASerializer::parseValue(const std::string& str, SSAValue& outValue)
{
    // Format: name:type or constant:type
    size_t colon = str.find(':'); // Last of?
    if (colon == std::string::npos)
        return false;

    std::string name    = str.substr(0, colon);
    std::string typeStr = str.substr(colon + 1);

    const auto type = SSASerializer::parseType(typeStr);
    if (type.kind() == TypeKind::Unspecified)
        return false;

    // Check if it's a constant
    if (name == "true" || name == "false") {
        bool val = (name == "true");
        outValue = SSAValue::Constant(val);
    } else if (name.find('.') != std::string::npos && std::ranges::all_of(name, [](char c) {
                   return isdigit(c) || c == '.' || c == '-'; // TODO: e notation support
               })) {
        // Number constant
        try {
            Number val = std::stod(name);
            outValue   = SSAValue::Constant(val);
        } catch (...) {
            return false;
        }
    } else if (name.find_first_not_of("0123456789") == std::string::npos) {
        // Integer constant
        try {
            Integer val = std::stoll(name);
            if (type.kind() == TypeKind::Number)
                outValue = SSAValue::Constant(static_cast<Number>(val));
            else
                outValue = SSAValue::Constant(val);
        } catch (...) {
            return false;
        }
    } else if (name.front() == '[' && name.back() == ']') {
        // TODO: Other stuff
        // Vector constant: [1.0,2.0,3.0]
        if (!type.isVector())
            return false;

        std::string inner              = name.substr(1, name.size() - 2);
        std::vector<std::string> parts = split(inner, ',');
        std::vector<Number> vec;
        try {
            for (const auto& part : parts)
                vec.push_back(std::stod(trim(part)));
        } catch (...) {
            return false;
        }

        // Verify vector size matches type
        if (vec.size() != type.size())
            return false;

        outValue = SSAValue::Constant(vec);
    } else {
        // Named value
        outValue = SSAValue::Named(name, type);
    }

    return true;
}

std::vector<SSAValue> SSASerializer::parseValueList(const std::string& str)
{
    std::vector<SSAValue> values;
    std::vector<std::string> tokens = split(str, ',');
    for (const auto& token : tokens) {
        if (SSAValue val; parseValue(token, val))
            values.push_back(val);
    }
    return values;
}

// Helper to parse function attributes like [[extern]], [[extern, pure]], [[pure, extern]], etc.
static void parseFunctionAttributes(const std::string& line, bool& isExternal, bool& hasSideEffect)
{
    isExternal    = false;
    hasSideEffect = true; // Default to having side effects

    // Find attribute section [[...]]
    size_t attrStart = line.find("[[");
    if (attrStart == std::string::npos)
        return;

    size_t attrEnd = line.find("]]", attrStart);
    if (attrEnd == std::string::npos)
        return;

    std::string attrContent        = line.substr(attrStart + 2, attrEnd - attrStart - 2);
    std::vector<std::string> attrs = split(attrContent, ',');

    for (const auto& attr : attrs) {
        std::string trimmed = trim(attr);
        if (trimmed == "extern")
            isExternal = true;
        else if (trimmed == "pure")
            hasSideEffect = false;
    }
}

// Parse a single instruction line
std::shared_ptr<SSAInstr> SSASerializer::readInstruction(const std::string& line)
{
    if (line.empty())
        return nullptr;

    // Check for label
    if (line.back() == ':') {
        auto label  = std::make_shared<SSAInstrLabel>();
        label->Name = line.substr(0, line.size() - 1);
        return label;
    }

    // Check for branch instruction
    if (line.find("br ") == 0) {
        // Format: br cond:bool -> lbl
        size_t arrow = line.find("->");
        if (arrow == std::string::npos)
            return nullptr;

        std::string condStr  = trim(line.substr(3, arrow - 3));
        std::string labelStr = trim(line.substr(arrow + 2));

        SSAValue cond;
        if (!parseValue(condStr, cond))
            return nullptr;

        auto branch         = std::make_shared<SSAInstrBranch>();
        branch->Condition   = cond;
        branch->TargetLabel = labelStr;
        return branch;
    }

    // Check for goto instruction
    if (line.find("goto ") == 0) {
        auto gotoInstr         = std::make_shared<SSAInstrGoto>();
        gotoInstr->TargetLabel = trim(line.substr(5));
        return gotoInstr;
    }

    // Check for return instruction
    if (line.find("return ") == 0) {
        std::string valStr = trim(line.substr(7));
        SSAValue val;
        if (!parseValue(valStr, val))
            return nullptr;

        auto ret   = std::make_shared<SSAInstrReturn>();
        ret->Value = val;
        return ret;
    }

    // Check for phi instruction
    if (line.find("phi") != std::string::npos && line.find('=') != std::string::npos) {
        // Format: target:type = phi[cond1:type, cond2:type](val1:type, val2:type)
        size_t eq             = line.find('=');
        std::string targetStr = trim(line.substr(0, eq));

        size_t bracketStart = line.find('[');
        size_t bracketEnd   = line.find(']');
        if (bracketStart == std::string::npos || bracketEnd == std::string::npos)
            return nullptr;

        size_t parenStart = line.find('(', bracketEnd);
        size_t parenEnd   = line.find(')', bracketEnd);
        if (parenStart == std::string::npos || parenEnd == std::string::npos)
            return nullptr;

        std::string condsStr    = line.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
        std::string branchesStr = line.substr(parenStart + 1, parenEnd - parenStart - 1);

        SSAValue target;
        if (!parseValue(targetStr, target))
            return nullptr;

        auto phi        = std::make_shared<SSAInstrPhi>();
        phi->Target     = target;
        phi->Conditions = parseValueList(condsStr);
        phi->Branches   = parseValueList(branchesStr);
        return phi;
    }

    // Check for call instruction
    if (line.find("call[") != std::string::npos && line.find('=') != std::string::npos) {
        // Format: target:type = call[funcName](arg1:type, arg2:type)
        size_t eq             = line.find('=');
        std::string targetStr = trim(line.substr(0, eq));

        size_t bracketStart = line.find('[');
        size_t bracketEnd   = line.find(']');
        if (bracketStart == std::string::npos || bracketEnd == std::string::npos)
            return nullptr;

        size_t parenStart = line.find('(', bracketEnd);
        size_t parenEnd   = line.find(')', bracketEnd);
        if (parenStart == std::string::npos || parenEnd == std::string::npos)
            return nullptr;

        std::string funcName = line.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
        std::string argsStr  = line.substr(parenStart + 1, parenEnd - parenStart - 1);

        SSAValue target;
        if (!parseValue(targetStr, target))
            return nullptr;

        auto call                = std::make_shared<SSAInstrCall>();
        call->Target             = target;
        call->FunctionName       = funcName;
        call->PublicFunctionName = funcName; // Assume same for now
        call->Arguments          = parseValueList(argsStr);
        return call;
    }

    // Check for assignment instruction (assign, unary, binary, etc.)
    if (line.find('=') != std::string::npos) {
        // Format: target:type = op(args...)
        size_t eq             = line.find('=');
        std::string targetStr = trim(line.substr(0, eq));
        std::string rest      = trim(line.substr(eq + 1));

        size_t parenStart = rest.find('(');
        if (parenStart == std::string::npos)
            return nullptr;

        std::string op  = trim(rest.substr(0, parenStart));
        size_t parenEnd = rest.find(')', parenStart);
        if (parenEnd == std::string::npos)
            return nullptr;

        std::string argsStr = rest.substr(parenStart + 1, parenEnd - parenStart - 1);

        SSAValue target;
        if (!parseValue(targetStr, target))
            return nullptr;

        auto assign      = std::make_shared<SSAInstrAssign>();
        assign->Target   = target;
        assign->Operands = parseValueList(argsStr);

        // Determine operator kind
        if (op == "assign") {
            assign->Operator = SSAInstrAssign::OpKind::Assign;
        } else if (op == "add") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Add;
        } else if (op == "sub") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Sub;
        } else if (op == "mul") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Mul;
        } else if (op == "div") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Div;
        } else if (op == "pow") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Pow;
        } else if (op == "mod") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Mod;
        } else if (op == "and") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::And;
        } else if (op == "or") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Or;
        } else if (op == "ls") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Less;
        } else if (op == "gt") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Greater;
        } else if (op == "le") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::LessEqual;
        } else if (op == "ge") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::GreaterEqual;
        } else if (op == "eq") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::Equal;
        } else if (op == "neq") {
            assign->Operator = SSAInstrAssign::OpKind::Binary;
            assign->BinaryOp = BinaryOperation::NotEqual;
        } else if (op == "pos") {
            assign->Operator = SSAInstrAssign::OpKind::Unary;
            assign->UnaryOp  = UnaryOperation::Pos;
        } else if (op == "neg") {
            assign->Operator = SSAInstrAssign::OpKind::Unary;
            assign->UnaryOp  = UnaryOperation::Neg;
        } else if (op == "not") {
            assign->Operator = SSAInstrAssign::OpKind::Unary;
            assign->UnaryOp  = UnaryOperation::Not;
        } else if (op == "cast") {
            assign->Operator = SSAInstrAssign::OpKind::Cast;
        } else if (op.find("swizzle[") == 0) {
            assign->Operator = SSAInstrAssign::OpKind::Swizzle;
            size_t start     = op.find('[') + 1;
            size_t end       = op.find(']');
            if (end != std::string::npos)
                assign->Swizzle = op.substr(start, end - start);
        } else if (op.find("vec[") == 0) {
            assign->Operator = SSAInstrAssign::OpKind::Vector;
        } else if (op == "access") {
            assign->Operator = SSAInstrAssign::OpKind::Access;
        } else {
            // TODO: Error
            assign->Operator = SSAInstrAssign::OpKind::Assign; // default
        }

        return assign;
    }

    return nullptr;
}

// Helper function implementations for reading/parsing
SSAProgram SSASerializer::read(std::istream& is)
{
    SSAProgram program;
    std::string line;
    std::shared_ptr<SSAFunction> currentFunction = nullptr;
    bool inBlockComment                          = false;

    while (std::getline(is, line)) {
        // Strip comments from the line
        line = stripComments(line, inBlockComment);
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

        // Check for function declaration/definition
        size_t fnPos = line.find("fn ");
        if (fnPos != std::string::npos) {
            // Parse function header
            currentFunction = std::make_shared<SSAFunction>();

            // Parse attributes if present
            if (line.find("[[") != std::string::npos) {
                size_t attrStart = line.find("[["); // This is where attributes start
                size_t attrEnd   = line.find("]]");
                if (attrEnd != std::string::npos) {
                    // Extract the part from [[ to ]] including attributes
                    std::string attrContent = line.substr(attrStart, attrEnd - attrStart + 2);
                    parseFunctionAttributes(attrContent, currentFunction->External, currentFunction->HasSideEffect);
                }
            }

            // Find function name (skip over attributes if present)
            size_t nameStart  = fnPos + 3;
            size_t parenStart = line.find('(', nameStart);
            if (parenStart == std::string::npos)
                continue;

            currentFunction->Name = trim(line.substr(nameStart, parenStart - nameStart));

            // Parse parameters
            size_t parenEnd = line.find(')', parenStart);
            if (parenEnd == std::string::npos)
                continue;

            std::string paramsStr = line.substr(parenStart + 1, parenEnd - parenStart - 1);
            if (!paramsStr.empty()) {
                std::vector<std::string> params = split(paramsStr, ',');
                for (const auto& param : params) {
                    // Parameter format: name or name:type
                    if (size_t colon = param.find(':'); colon != std::string::npos)
                        currentFunction->Parameters.push_back(param.substr(0, colon));
                    else
                        currentFunction->Parameters.push_back(param);
                }
            }

            // Parse return type
            if (size_t colonPos = line.find(':', parenEnd); colonPos != std::string::npos) {
                std::string typeStr         = trim(line.substr(colonPos + 1));
                currentFunction->ReturnType = parseType(typeStr);
            }

            continue;
        }

        // Parse instruction
        std::string instrLine = trim(line);
        auto instr            = readInstruction(instrLine);
        if (instr) {
            if (currentFunction)
                currentFunction->Body.push_back(instr);
            else
                program.Body.push_back(instr);
        }
    }

    return program;
}

SSAProgram SSASerializer::deserialize(const std::string& str)
{
    std::istringstream iss(str);
    return read(iss);
}

std::string SSASerializer::escapeString(const std::string& str)
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

std::string SSASerializer::unescapeString(const std::string& str)
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

} // namespace PExpr::ssa