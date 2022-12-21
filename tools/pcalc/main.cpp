#include <cmath>
#include <iostream>

#include "PExpr.h"

using namespace PExpr;

using ValueBlock = std::variant<bool, Integer, Number, Vec2, Vec3, Vec4, std::string>;

static std::unordered_map<std::string_view, Number> Constants = {
    { "Pi", 3.14159265358979323846 }
};

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
inline ValueBlock binaryCwise(const ValueBlock& A, const ValueBlock& B, ElementaryType arithType, Func func)
{
    switch (arithType) {
    // Not defined for below
    default:
        return Number(0);
    case ElementaryType::Integer:
        return func(std::get<Integer>(A), std::get<Integer>(B));
    case ElementaryType::Number:
        return func(std::get<Number>(A), std::get<Number>(B));
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

template <typename Func>
inline bool comp(const ValueBlock& A, const ValueBlock& B, ElementaryType type, Func func)
{
    switch (type) {
    // Not defined for below
    default:
        return false;
    case ElementaryType::Boolean:
        return func(std::get<bool>(A), std::get<bool>(B));
    case ElementaryType::Integer:
        return func(std::get<Integer>(A), std::get<Integer>(B));
    case ElementaryType::Number:
        return func(std::get<Number>(A), std::get<Number>(B));
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
        return func(std::get<Integer>(A));
    case ElementaryType::Number:
        return func(std::get<Number>(A));
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
class CalcVisitor : public TranspileVisitor<ValueBlock> {
public:
    ValueBlock onVariable(const std::string& name, ElementaryType) override
    {
        return Constants.at(name);
    }

    ValueBlock onInteger(Integer v) override { return v; }
    ValueBlock onNumber(Number v) override { return v; }
    ValueBlock onBool(bool v) override { return v; }
    ValueBlock onString(const std::string& v) override { return v; }

    /// Implicit casts. Currently only int -> num
    ValueBlock onCast(const ValueBlock& v, ElementaryType, ElementaryType) override
    {
        return Number(std::get<Integer>(v));
    }

    /// +a, -a. Only called for arithmetic types
    ValueBlock onPosNeg(bool isNeg, ElementaryType arithType, const ValueBlock& v) override
    {
        if (isNeg)
            return unaryCwise(v, arithType, [](auto&& a) { return -a; });
        else
            return v;
    }

    // !a. Only called for bool
    ValueBlock onNot(const ValueBlock& v) override
    {
        return !std::get<bool>(v);
    }

    /// a+b, a-b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    ValueBlock onAddSub(bool isSub, ElementaryType arithType, const ValueBlock& a, const ValueBlock& b) override
    {
        if (isSub)
            return binaryCwise(a, b, arithType, [](auto&& a, auto&& b) { return a - b; });
        else
            return binaryCwise(a, b, arithType, [](auto&& a, auto&& b) { return a + b; });
    }

    /// a*b, a/b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    ValueBlock onMulDiv(bool isDiv, ElementaryType arithType, const ValueBlock& a, const ValueBlock& b) override
    {
        if (isDiv)
            return binaryCwise(a, b, arithType, [](auto&& a, auto&& b) { return a / b; });
        else
            return binaryCwise(a, b, arithType, [](auto&& a, auto&& b) { return a * b; });
    }

    /// a*f, f*a, a/f. A is an arithmetic type, f is 'num', except when a is 'int' then f is 'int' as well. Order of a*f or f*a does not matter
    ValueBlock onScale(bool isDiv, ElementaryType aType, const ValueBlock& a, const ValueBlock& f) override
    {
        if (isDiv) {
            return func1Cwise(a, aType, [&](auto&& v) { return v / std::get<Number>(f); });
        } else {
            if (aType == ElementaryType::Integer)
                return std::get<Integer>(a) * std::get<Integer>(f);
            else
                return func1Cwise(a, aType, [&](auto&& v) { return v * std::get<Number>(f); });
        }
    }

    /// a^f A is an arithmetic type, f is 'num', except when a is 'int' then f is 'int' as well. Vectorized types should apply component wise
    ValueBlock onPow(ElementaryType aType, const ValueBlock& a, const ValueBlock& f) override
    {
        if (aType == ElementaryType::Integer)
            return Integer(std::pow(std::get<Integer>(a), std::get<Integer>(f)));
        else
            return func1Cwise(a, aType, [&](auto&& v) { return Number(std::pow(v, std::get<Number>(f))); });
    }

    // a % b. Only called for int
    ValueBlock onMod(const ValueBlock& a, const ValueBlock& b) override
    {
        return std::get<Integer>(a) % std::get<Integer>(b);
    }

    // a&&b, a||b. Only called for bool types
    ValueBlock onAndOr(bool isOr, const ValueBlock& a, const ValueBlock& b) override
    {
        if (isOr)
            return std::get<bool>(a) || std::get<bool>(b);
        else
            return std::get<bool>(a) && std::get<bool>(b);
    }

    /// a < b... Boolean operation. a & b are of the same type. Only called for scalar arithmetic types (int, num)
    ValueBlock onRelOp(RelationalOp op, ElementaryType scalarArithType, const ValueBlock& a, const ValueBlock& b) override
    {
        switch (op) {
        case RelationalOp::Less:
            return comp(a, b, scalarArithType, [](auto&& x, auto&& y) { return x < y; });
        case RelationalOp::Greater:
            return comp(a, b, scalarArithType, [](auto&& x, auto&& y) { return x > y; });
        case RelationalOp::LessEqual:
            return comp(a, b, scalarArithType, [](auto&& x, auto&& y) { return x <= y; });
        case RelationalOp::GreaterEqual:
            return comp(a, b, scalarArithType, [](auto&& x, auto&& y) { return x >= y; });
        default:
            PEXPR_ASSERT(false, "Should not reach this point");
            return false;
        }
    }

    /// a==b, a!=b. For vectorized types it should check that all equal componont wise. The negation a!=b should behave as !(a==b)
    ValueBlock onEqual(bool isNeg, ElementaryType type, const ValueBlock& a, const ValueBlock& b) override
    {
        bool res = comp(a, b, type, [](auto&& x, auto&& y) { return x == y; });
        return isNeg ? !res : res;
    }

    /// name(...). Call to a function. Necessary casts are already handled.
    ValueBlock onFunctionCall(const std::string& name,
                              ElementaryType, const std::vector<ElementaryType>& argumentTypes,
                              const std::vector<ValueBlock>& argumentPayloads) override
    {
        using NumFunc = Number (*)(Number);
        if (name == "vec2") {
            return Vec2{ std::get<Number>(argumentPayloads[0]), std::get<Number>(argumentPayloads[1]) };
        } else if (name == "vec3") {
            return Vec3{ std::get<Number>(argumentPayloads[0]), std::get<Number>(argumentPayloads[1]), std::get<Number>(argumentPayloads[2]) };
        } else if (name == "vec4") {
            return Vec4{ std::get<Number>(argumentPayloads[0]), std::get<Number>(argumentPayloads[1]), std::get<Number>(argumentPayloads[2]), std::get<Number>(argumentPayloads[3]) };
        } else if (name == "sin") {
            return func1Cwise(argumentPayloads[0], argumentTypes[0], (NumFunc)std::sin);
        } else if (name == "cos") {
            return func1Cwise(argumentPayloads[0], argumentTypes[0], (NumFunc)std::cos);
        } else if (name == "tan") {
            return func1Cwise(argumentPayloads[0], argumentTypes[0], (NumFunc)std::tan);
        } else if (name == "asin") {
            return func1Cwise(argumentPayloads[0], argumentTypes[0], (NumFunc)std::asin);
        } else if (name == "acos") {
            return func1Cwise(argumentPayloads[0], argumentTypes[0], (NumFunc)std::acos);
        } else if (name == "atan") {
            return func1Cwise(argumentPayloads[0], argumentTypes[0], (NumFunc)std::atan);
        } else if (name == "exp") {
            return func1Cwise(argumentPayloads[0], argumentTypes[0], (NumFunc)std::exp);
        } else if (name == "log") {
            return func1Cwise(argumentPayloads[0], argumentTypes[0], (NumFunc)std::log);
        }

        PEXPR_ASSERT(false, "Should not reach this point");
        return Number(0); // Error should be caught in type checking
    }

    /// a.xyz Access operator for vector types
    ValueBlock onAccess(const ValueBlock& v, size_t inputSize, const std::vector<uint8>& outputPermutation) override
    {
        const auto getC = [&](size_t i) {
            switch (inputSize) {
            default:
                return Number(0);
            case 2:
                return std::get<Vec2>(v)[i];
            case 3:
                return std::get<Vec3>(v)[i];
            case 4:
                return std::get<Vec4>(v)[i];
            }
        };

        if (outputPermutation.size() == 1) {
            return Number(getC(outputPermutation[0]));
        } else if (outputPermutation.size() == 2) {
            return Vec2{ getC(outputPermutation[0]),
                         getC(outputPermutation[1]) };
        } else if (outputPermutation.size() == 3) {
            return Vec3{ getC(outputPermutation[0]),
                         getC(outputPermutation[1]),
                         getC(outputPermutation[2]) };
        } else {
            return Vec4{ getC(outputPermutation[0]),
                         getC(outputPermutation[1]),
                         getC(outputPermutation[2]),
                         getC(outputPermutation[3]) };
        }
    }
};

static std::optional<VariableDef> variableLookup(const VariableLookup& lkp)
{
    if (Constants.count(lkp.name()))
        return VariableDef(lkp.name(), ElementaryType::Number);
    return {};
}

static std::optional<FunctionDef> functionLookup(const FunctionLookup& lkp)
{
    if (lkp.name() == "vec2" && lkp.matchParameter({ ElementaryType::Number, ElementaryType::Number }))
        return FunctionDef("vec2", ElementaryType::Vec2, { ElementaryType::Number, ElementaryType::Number });
    if (lkp.name() == "vec3" && lkp.matchParameter({ ElementaryType::Number, ElementaryType::Number, ElementaryType::Number }))
        return FunctionDef("vec3", ElementaryType::Vec3, { ElementaryType::Number, ElementaryType::Number, ElementaryType::Number });
    if (lkp.name() == "vec4" && lkp.matchParameter({ ElementaryType::Number, ElementaryType::Number, ElementaryType::Number, ElementaryType::Number }))
        return FunctionDef("vec4", ElementaryType::Vec4, { ElementaryType::Number, ElementaryType::Number, ElementaryType::Number, ElementaryType::Number });

    if (lkp.parameters().size() == 1 && isArithmetic(lkp.parameters()[0])) {
        if (lkp.name() == "sin" || lkp.name() == "cos" || lkp.name() == "tan"
            || lkp.name() == "asin" || lkp.name() == "acos" || lkp.name() == "atan"
            || lkp.name() == "exp" || lkp.name() == "log") {
            ElementaryType type = lkp.parameters()[0] != ElementaryType::Integer ? lkp.parameters()[0] : ElementaryType::Number;
            return FunctionDef(lkp.name(), type, { type });
        }
    }

    return {};
}

int main(int argc, char** argv)
{
    std::string input;
    for (int i = 1; i < argc; ++i) {
        input += argv[i];
        input += " ";
    }

    Environment env;
    env.registerVariableLookupFunction(variableLookup);
    env.registerFunctionLookupFunction(functionLookup);

    auto ast = env.parse(input);

    if (ast == nullptr)
        return EXIT_FAILURE;

#if 0
    std::cout << StringVisitor::visit(ast) << std::endl;
#endif

    CalcVisitor visitor;
    auto ret = env.transpile(ast, &visitor);

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