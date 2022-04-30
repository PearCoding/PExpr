#include <cmath>
#include <iostream>

#include "PExpr.h"

using namespace PExpr;

using ValueBlock = std::variant<bool, Integer, Number, Vec2, Vec3, Vec4, std::string>;

static std::unordered_map<std::string_view, Number> Constants = {
    { "Pi", 3.14159265358979323846 }
};

inline Number convNumber(const ValueBlock& A, ElementaryType type)
{
    return type == ElementaryType::Integer ? Number(std::get<Integer>(A)) : std::get<Number>(A);
}

template <typename Func>
inline ValueBlock unaryCwise(const ValueBlock& A, ElementaryType fromType, Func func)
{
    switch (fromType) {
    // Not defined for below
    default:
        return Number(0);
    case ElementaryType::Integer:
        return Integer(func(std::get<Integer>(A)));
    case ElementaryType::Number:
        return Number(func(std::get<Number>(A)));
    case ElementaryType::Vec2:
        return Vec2{ func(std::get<Vec2>(A)[0]),
                     func(std::get<Vec2>(A)[1]) };
    case ElementaryType::Vec3:
        return Vec3{ func(std::get<Vec3>(A)[0]),
                     func(std::get<Vec3>(A)[1]),
                     func(std::get<Vec3>(A)[2]) };
    case ElementaryType::Vec4:
        return Vec4{ func(std::get<Vec4>(A)[0]),
                     func(std::get<Vec4>(A)[1]),
                     func(std::get<Vec4>(A)[2]),
                     func(std::get<Vec4>(A)[3]) };
    }
}

template <typename Func>
inline ValueBlock binaryCwise(const ValueBlock& A, ElementaryType AType, const ValueBlock& B, ElementaryType BType, Func func)
{
    auto convANumber = [&]() {
        return convNumber(A, AType);
    };
    auto convBNumber = [&]() {
        return convNumber(B, BType);
    };

    switch (AType) {
    // Not defined for below
    default:
        return Number(0);
    case ElementaryType::Integer:
        return func(convANumber(), convBNumber());
    case ElementaryType::Number:
        return func(convANumber(), convBNumber());
    case ElementaryType::Vec2:
        return Vec2{ func(std::get<Vec2>(A)[0], std::get<Vec2>(B)[0]),
                     func(std::get<Vec2>(A)[1], std::get<Vec2>(B)[1]) };
    case ElementaryType::Vec3:
        return Vec3{ func(std::get<Vec3>(A)[0], std::get<Vec3>(B)[0]),
                     func(std::get<Vec3>(A)[1], std::get<Vec3>(B)[1]),
                     func(std::get<Vec3>(A)[2], std::get<Vec3>(B)[2]) };
    case ElementaryType::Vec4:
        return Vec4{ func(std::get<Vec4>(A)[0], std::get<Vec4>(B)[0]),
                     func(std::get<Vec4>(A)[1], std::get<Vec4>(B)[1]),
                     func(std::get<Vec4>(A)[2], std::get<Vec4>(B)[2]),
                     func(std::get<Vec4>(A)[3], std::get<Vec4>(B)[3]) };
    }
}

inline ValueBlock scale(const ValueBlock& A, ElementaryType AType, Number scale)
{
    switch (AType) {
    // Not defined for below
    default:
        return Number(0);
    case ElementaryType::Vec2:
        return Vec2{ std::get<Vec2>(A)[0] * scale,
                     std::get<Vec2>(A)[1] * scale };
    case ElementaryType::Vec3:
        return Vec3{ std::get<Vec3>(A)[0] * scale,
                     std::get<Vec3>(A)[1] * scale,
                     std::get<Vec3>(A)[2] * scale };
    case ElementaryType::Vec4:
        return Vec4{ std::get<Vec4>(A)[0] * scale,
                     std::get<Vec4>(A)[1] * scale,
                     std::get<Vec4>(A)[2] * scale,
                     std::get<Vec4>(A)[3] * scale };
    }
}

template <typename Func>
inline bool comp(const ValueBlock& A, ElementaryType AType, const ValueBlock& B, ElementaryType BType, bool allowSpecial, Func func)
{
    if (AType == ElementaryType::Integer && BType == ElementaryType::Integer)
        return func(std::get<Integer>(A), std::get<Integer>(B));
    else if (isConvertible(AType, BType) || isConvertible(BType, AType))
        return func(convNumber(A, AType), convNumber(B, BType));
    else if (AType != BType)
        return false;

    if (!allowSpecial)
        return false;

    switch (AType) {
    // Not defined for below
    default:
        return false;
    case ElementaryType::Boolean:
        return func(std::get<bool>(A), std::get<bool>(B));
    case ElementaryType::String:
        return func(std::get<std::string>(A), std::get<std::string>(B));
    case ElementaryType::Vec2:
        return func(std::get<Vec2>(A)[0], std::get<Vec2>(B)[0]) && func(std::get<Vec2>(A)[1], std::get<Vec2>(B)[1]);
    case ElementaryType::Vec3:
        return func(std::get<Vec3>(A)[0], std::get<Vec3>(B)[0]) && func(std::get<Vec3>(A)[1], std::get<Vec3>(B)[1]) && func(std::get<Vec3>(A)[2], std::get<Vec3>(B)[2]);
    case ElementaryType::Vec4:
        return func(std::get<Vec4>(A)[0], std::get<Vec4>(B)[0]) && func(std::get<Vec4>(A)[1], std::get<Vec4>(B)[1]) && func(std::get<Vec4>(A)[2], std::get<Vec4>(B)[2]) && func(std::get<Vec4>(A)[3], std::get<Vec4>(B)[3]);
    }
}

template <typename Func>
inline ValueBlock func1Cwise(const ValueBlock& A, ElementaryType fromType, Func func)
{
    switch (fromType) {
    // Not defined for below
    default:
        return Number(0);
    case ElementaryType::Integer:
    case ElementaryType::Number:
        return Number(func(convNumber(A, fromType)));
    case ElementaryType::Vec2:
        return Vec2{ func(std::get<Vec2>(A)[0]),
                     func(std::get<Vec2>(A)[1]) };
    case ElementaryType::Vec3:
        return Vec3{ func(std::get<Vec3>(A)[0]),
                     func(std::get<Vec3>(A)[1]),
                     func(std::get<Vec3>(A)[2]) };
    case ElementaryType::Vec4:
        return Vec4{ func(std::get<Vec4>(A)[0]),
                     func(std::get<Vec4>(A)[1]),
                     func(std::get<Vec4>(A)[2]),
                     func(std::get<Vec4>(A)[3]) };
    }
}

class CalcVisitor {
public:
    static ValueBlock calc(const Ptr<Expression>& expr)
    {
        switch (expr->type()) {
        case ExpressionType::Variable:
            return calcNode(std::reinterpret_pointer_cast<VariableExpression>(expr));
        case ExpressionType::Const:
            return calcNode(std::reinterpret_pointer_cast<ConstExpression>(expr));
        case ExpressionType::Unary:
            return calcNode(std::reinterpret_pointer_cast<UnaryExpression>(expr));
        case ExpressionType::Binary:
            return calcNode(std::reinterpret_pointer_cast<BinaryExpression>(expr));
        case ExpressionType::Call:
            return calcNode(std::reinterpret_pointer_cast<CallExpression>(expr));
        case ExpressionType::Access:
            return calcNode(std::reinterpret_pointer_cast<AccessExpression>(expr));
        default:
            return Number(0);
        }
    };

private:
    static ValueBlock calcNode(const Ptr<VariableExpression>& expr)
    {
        if (Constants.count(expr->name()))
            return Constants.at(expr->name());

        PEXPR_ASSERT(false, "Should not reach this point");
        return Number(0); // Error should be caught in type checking
    }

    static ValueBlock calcNode(const Ptr<ConstExpression>& expr)
    {
        if (expr->returnType() == ElementaryType::Boolean)
            return expr->getBool();
        if (expr->returnType() == ElementaryType::Integer)
            return expr->getInteger();
        if (expr->returnType() == ElementaryType::Number)
            return expr->getNumber();
        if (expr->returnType() == ElementaryType::String)
            return expr->getString();

        PEXPR_ASSERT(false, "Should not reach this point");
        return Number(0); // Error should be caught in type checking
    }

    static ValueBlock calcNode(const Ptr<UnaryExpression>& expr)
    {
        auto A = calc(expr->inner());
        switch (expr->op()) {
        case UnaryOperation::Pos:
            return A;
        case UnaryOperation::Neg:
            return unaryCwise(A, expr->inner()->returnType(), [](Number i) { return -i; });
        case UnaryOperation::Not:
            return !std::get<bool>(A);
        }

        PEXPR_ASSERT(false, "Should not reach this point");
        return Number(0); // Error should be caught in type checking
    }

    static ValueBlock calcNode(const Ptr<BinaryExpression>& expr)
    {
        auto A     = calc(expr->left());
        auto AType = expr->left()->returnType();
        auto B     = calc(expr->right());
        auto BType = expr->right()->returnType();

        switch (expr->op()) {
        case BinaryOperation::Add:
            if (AType == ElementaryType::Integer && BType == ElementaryType::Integer)
                return std::get<Integer>(A) + std::get<Integer>(B);
            else
                return binaryCwise(A, AType, B, BType, [](Number a, Number b) { return a + b; });
        case BinaryOperation::Sub:
            if (AType == ElementaryType::Integer && BType == ElementaryType::Integer)
                return std::get<Integer>(A) - std::get<Integer>(B);
            else
                return binaryCwise(A, AType, B, BType, [](Number a, Number b) { return a - b; });
        case BinaryOperation::Mul:
            if (AType == ElementaryType::Integer && BType == ElementaryType::Integer)
                return std::get<Integer>(A) * std::get<Integer>(B);
            else if (isConvertible(AType, BType) || isConvertible(BType, AType))
                return binaryCwise(A, AType, B, BType, [](Number a, Number b) { return a * b; });
            else if (isConvertible(AType, ElementaryType::Number) && isArray(BType))
                return scale(B, BType, convNumber(A, AType));
            else if (isConvertible(BType, ElementaryType::Number) && isArray(AType))
                return scale(A, AType, convNumber(B, BType));
            break;
        case BinaryOperation::Div:
            if (AType == ElementaryType::Integer && BType == ElementaryType::Integer)
                return std::get<Integer>(A) / Number(std::get<Integer>(B));
            else if (isConvertible(AType, BType) || isConvertible(BType, AType))
                return binaryCwise(A, AType, B, BType, [](Number a, Number b) { return a / b; });
            else if (isConvertible(BType, ElementaryType::Number) && isArray(AType))
                return scale(A, AType, 1 / convNumber(B, BType));
            break;
        case BinaryOperation::Pow:
            if (AType == ElementaryType::Integer && BType == ElementaryType::Integer)
                return Integer(std::pow(std::get<Integer>(A), std::get<Integer>(B)));
            else
                return Integer(std::pow(convNumber(A, AType), convNumber(B, BType)));
        case BinaryOperation::Mod:
            return std::get<Integer>(A) % std::get<Integer>(B);
        case BinaryOperation::And:
            return std::get<bool>(A) && std::get<bool>(B);
        case BinaryOperation::Or:
            return std::get<bool>(A) || std::get<bool>(B);
        case BinaryOperation::Less:
            return comp(A, AType, B, BType, false, [](auto&& a, auto&& b) { return a < b; });
        case BinaryOperation::Greater:
            return comp(A, AType, B, BType, false, [](auto&& a, auto&& b) { return a > b; });
        case BinaryOperation::LessEqual:
            return comp(A, AType, B, BType, false, [](auto&& a, auto&& b) { return a <= b; });
        case BinaryOperation::GreaterEqual:
            return comp(A, AType, B, BType, false, [](auto&& a, auto&& b) { return a >= b; });
        case BinaryOperation::Equal:
            return comp(A, AType, B, BType, true, [](auto&& a, auto&& b) { return a == b; });
        case BinaryOperation::NotEqual:
            return !comp(A, AType, B, BType, true, [](auto&& a, auto&& b) { return a == b; });
        }

        PEXPR_ASSERT(false, "Should not reach this point");
        return Number(0); // Error should be caught in type checking
    }

    static ValueBlock calcNode(const Ptr<CallExpression>& expr)
    {
        using NumFunc = Number (*)(Number);

        const std::string& funcName = expr->name();

        std::vector<ElementaryType> types;
        std::vector<ValueBlock> args;
        types.reserve(expr->parameters().size());
        args.reserve(expr->parameters().size());

        for (const auto& e : expr->parameters()) {
            types.push_back(e->returnType());
            args.push_back(calc(e));
        }

        if (funcName == "vec2") {
            return Vec2{ convNumber(args[0], types[0]), convNumber(args[1], types[1]) };
        } else if (funcName == "vec3") {
            return Vec3{ convNumber(args[0], types[0]), convNumber(args[1], types[1]), convNumber(args[2], types[2]) };
        } else if (funcName == "vec4") {
            return Vec4{ convNumber(args[0], types[0]), convNumber(args[1], types[1]), convNumber(args[2], types[2]), convNumber(args[3], types[3]) };
        } else if (funcName == "sin") {
            return func1Cwise(args[0], types[0], (NumFunc)std::sin);
        } else if (funcName == "cos") {
            return func1Cwise(args[0], types[0], (NumFunc)std::cos);
        } else if (funcName == "tan") {
            return func1Cwise(args[0], types[0], (NumFunc)std::tan);
        } else if (funcName == "asin") {
            return func1Cwise(args[0], types[0], (NumFunc)std::asin);
        } else if (funcName == "acos") {
            return func1Cwise(args[0], types[0], (NumFunc)std::acos);
        } else if (funcName == "atan") {
            return func1Cwise(args[0], types[0], (NumFunc)std::atan);
        } else if (funcName == "exp") {
            return func1Cwise(args[0], types[0], (NumFunc)std::exp);
        } else if (funcName == "log") {
            return func1Cwise(args[0], types[0], (NumFunc)std::log);
        }

        PEXPR_ASSERT(false, "Should not reach this point");
        return Number(0); // Error should be caught in type checking
    }

    static ValueBlock calcNode(const Ptr<AccessExpression>& expr)
    {
        ValueBlock A    = calc(expr->inner());
        const auto getC = [&](size_t i) {
            switch (expr->inner()->returnType()) {
            default:
                return Number(0);
            case ElementaryType::Vec2:
                return std::get<Vec2>(A)[i];
            case ElementaryType::Vec3:
                return std::get<Vec3>(A)[i];
            case ElementaryType::Vec4:
                return std::get<Vec4>(A)[i];
            }
        };
        const auto charC = [](char c) {
            if (c == 'x' || c == 'r')
                return 0;
            if (c == 'y' || c == 'g')
                return 1;
            if (c == 'z' || c == 'b')
                return 2;
            else
                return 3;
        };

        const std::string swizzle = expr->swizzle();

        if (swizzle.size() == 1) {
            return Number(getC(charC(swizzle[0])));
        } else if (swizzle.size() == 2) {
            return Vec2{ getC(charC(swizzle[0])),
                         getC(charC(swizzle[1])) };
        } else if (swizzle.size() == 3) {
            return Vec3{ getC(charC(swizzle[0])),
                         getC(charC(swizzle[1])),
                         getC(charC(swizzle[2])) };
        } else {
            return Vec4{ getC(charC(swizzle[0])),
                         getC(charC(swizzle[1])),
                         getC(charC(swizzle[2])),
                         getC(charC(swizzle[3])) };
        }
    }
};

int main(int argc, char** argv)
{
    std::string input;
    for (int i = 1; i < argc; ++i) {
        input += argv[i];
        input += " ";
    }

    std::stringstream stream(input);
    Environment env;
    for (auto c : Constants)
        env.registerDef(ConstantDef(std::string(c.first), ElementaryType::Number, c.second));

    env.registerDef(FunctionDef("vec2", ElementaryType::Vec2, { ElementaryType::Number, ElementaryType::Number }));
    env.registerDef(FunctionDef("vec3", ElementaryType::Vec3, { ElementaryType::Number, ElementaryType::Number, ElementaryType::Number }));
    env.registerDef(FunctionDef("vec4", ElementaryType::Vec4, { ElementaryType::Number, ElementaryType::Number, ElementaryType::Number, ElementaryType::Number }));

    auto addDynFunc1 = [&](const std::string& name) {
        env.registerDef(FunctionDef(name, ElementaryType::Number, { ElementaryType::Number }));
        env.registerDef(FunctionDef(name, ElementaryType::Vec2, { ElementaryType::Vec2 }));
        env.registerDef(FunctionDef(name, ElementaryType::Vec3, { ElementaryType::Vec3 }));
        env.registerDef(FunctionDef(name, ElementaryType::Vec4, { ElementaryType::Vec4 }));
    };

    addDynFunc1("sin");
    addDynFunc1("cos");
    addDynFunc1("tan");
    addDynFunc1("asin");
    addDynFunc1("acos");
    addDynFunc1("atan");
    addDynFunc1("exp");
    addDynFunc1("log");

    auto ast = env.parse(stream);

    if (ast == nullptr)
        return EXIT_FAILURE;

    auto ret = CalcVisitor::calc(ast);

    std::visit(
        [](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Integer>)
                std::cout << arg << std::endl;
            else if constexpr (std::is_same_v<T, Number>)
                std::cout << arg << std::endl;
            else if constexpr (std::is_same_v<T, bool>)
                std::cout << (arg ? "true" : "false") << std::endl;
            else if constexpr (std::is_same_v<T, Vec2>)
                std::cout << "[" << arg[0] << ", " << arg[1] << "]" << std::endl;
            else if constexpr (std::is_same_v<T, Vec3>)
                std::cout << "[" << arg[0] << ", " << arg[1] << ", " << arg[2] << "]" << std::endl;
            else if constexpr (std::is_same_v<T, Vec4>)
                std::cout << "[" << arg[0] << ", " << arg[1] << ", " << arg[2] << ", " << arg[3] << "]" << std::endl;
            else if constexpr (std::is_same_v<T, std::string>)
                std::cout << "'" << arg << "'" << std::endl;
        },
        ret);
    return EXIT_SUCCESS;
}