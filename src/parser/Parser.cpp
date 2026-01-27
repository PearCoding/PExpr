#include "Parser.h"
#include "ast/Expression.h"
#include "ast/Pattern.h"
#include "ast/Statement.h"
#include "type/Mangler.h"
#include "type/Parameter.h"

namespace PExpr::parser {
using namespace ast;
using namespace type;

Parser::Parser(Lexer& lexer, utils::Reporter& reporter)
    : mLexer(lexer)
    , mReporter(reporter)
    , mCurrentToken()
    , mHasError(false)
{
}

Ptr<Closure> parse_translation_unit(Parser& parser, const SymbolTable* globals);
Ptr<Closure> Parser::parse(const SymbolTable* globals)
{
    mHasError = false;
    for (size_t i = 0; i < mCurrentToken.size(); ++i) {
        mCurrentToken[i] = mLexer.next();
        if (mCurrentToken[i].Type == TokenType::Error) {
            mHasError = true;
            return nullptr;
        }
    }

    return parse_translation_unit(*this, globals);
}

bool Parser::expect(TokenType type)
{
    bool same = cur().Type == type;
    if (!same) {
        if (cur().Type == TokenType::Eof)
            mReporter.errorf(cur().Location, "Expected '%s' but input terminated early", Token::toString(type).data());
        else
            mReporter.errorf(cur().Location, "Expected '%s' but got '%s'", Token::toString(type).data(), Token::toString(cur().Type).data());
        mHasError = true;
    }

    next();
    return same;
}

template <size_t N>
void Parser::error(const std::array<TokenType, N>& types)
{
    mHasError = true;

    std::string expectation;
    for (size_t i = 0; i < types.size(); ++i) {
        expectation += Token::toString(types[i]);
        if (i != types.size() - 1)
            expectation += ", ";
    }

    if (cur().Type == TokenType::Eof)
        mReporter.errorf(cur().Location, "Expected {%s} but input terminated early", expectation.c_str());
    else
        mReporter.errorf(cur().Location, "Expected {%s} but got '%s'", expectation.c_str(), Token::toString(cur().Type).data());
}

void Parser::eat(TokenType type)
{
    PEXPR_ASSERT(type == cur().Type, "eat() should only be called if current token is known");
    PEXPR_UNUSED(type);
    next();
}

bool Parser::accept(TokenType type)
{
    if (cur().Type == type) {
        eat(type);
        return true;
    }
    return false;
}

void Parser::next()
{
    for (size_t i = 1; i < mCurrentToken.size(); ++i)
        mCurrentToken[i - 1] = mCurrentToken[i];

    const auto nextToken                    = mLexer.next();
    mCurrentToken[mCurrentToken.size() - 1] = nextToken;

    if (nextToken.Type == TokenType::Error)
        mHasError = true;
}

// --------------------------------------- Grammar
class ParserGrammar {
public:
    inline explicit ParserGrammar(Parser& parser, const SymbolTable* globals)
        : P(parser)
        , mGlobals(globals)
    {
    }

    Parser& P;

    inline Ptr<Closure> parse()
    {
        auto closure = p_closure();
        if (!P.hasError() && P.cur().Type != TokenType::Eof) {
            P.signalError();
            P.mReporter.errorf(Location(0), "Parsing stopped before end of stream!");
        }

        return closure;
    }

private:
    // Attribute value variant
    struct Attribute {
        std::string name;
        std::variant<bool, Integer, Number, std::string> value;

        bool isBool() const { return std::holds_alternative<bool>(value); }
        bool isInt() const { return std::holds_alternative<Integer>(value); }
        bool isNum() const { return std::holds_alternative<Number>(value); }
        bool isString() const { return std::holds_alternative<std::string>(value); }

        bool getBool() const { return std::get<bool>(value); }
        Integer getInt() const { return std::get<Integer>(value); }
        Number getNum() const { return std::get<Number>(value); }
        const std::string& getString() const { return std::get<std::string>(value); }
    };

    using AttributeList = std::vector<Attribute>;

    Closure* mCurrentClosure = nullptr;
    const SymbolTable* mGlobals;

    inline Ptr<Closure> p_closure()
    {
        if (P.cur().Type == TokenType::Eof)
            return nullptr;

        Ptr<Closure> closure = std::make_shared<Closure>(P.cur().Location, mCurrentClosure);
        mCurrentClosure      = closure.get();
        if (mCurrentClosure->isTranslationUnit())
            mCurrentClosure->symbols().setParent(mGlobals); // Inject the global symbol table

        while (true) {
            // Check for attributes before statement
            AttributeList attrs;
            if (P.cur(0).Type == TokenType::OpenSquareBracket && P.cur(1).Type == TokenType::OpenSquareBracket) {
                P.expect(TokenType::OpenSquareBracket);
                P.expect(TokenType::OpenSquareBracket);
                attrs = p_attributes();
                P.expect(TokenType::ClosedSquareBracket);
                P.expect(TokenType::ClosedSquareBracket);
            }

            if (P.accept(TokenType::Let)) {
                // Could be regular variable or destructuring declaration
                if (P.cur(0).Type == TokenType::Mul && P.cur(1).Type == TokenType::OpenSquareBracket) {
                    // Destructuring declaration: let *[pattern] = expr;
                    closure->addStatement(p_destructuring_statement(true, attrs));
                } else {
                    // Regular variable declaration
                    closure->addStatement(p_variable_statement(true, attrs));
                }
            } else if (P.accept(TokenType::Function)) {
                // Function
                closure->addStatement(p_function_statement(attrs));
            } else if (P.accept(TokenType::Using)) {
                // Type alias
                closure->addStatement(p_type_alias_statement(attrs));
            } else if (P.cur(0).Type == TokenType::Mul && P.cur(1).Type == TokenType::OpenSquareBracket) {
                // Destructuring assignment: *[pattern] = expr;
                closure->addStatement(p_destructuring_statement(false, attrs));
            } else if (P.cur(0).Type == TokenType::Identifier && P.cur(1).Type == TokenType::Assign) {
                // Regular variable assignment
                closure->addStatement(p_variable_statement(false, attrs));
            } else {
                break;
            }
        }

        if (P.cur().Type == TokenType::ClosedBraces) {
            P.signalError();
            P.mReporter.errorf(P.cur().Location, "Expected an expression at the end of a closure but got '%s' instead", Token::toString(TokenType::ClosedBraces).data());
            return closure;
        }

        closure->setExpression(p_expression());

        // Check for a trailing semicolon
        const auto semicolonLoc = P.cur().Location;
        if (P.accept(TokenType::Semicolon))
            P.mReporter.warningf(utils::RT_WARNING_TRAILING_SEMICOLON, semicolonLoc, "Trailing '%s' at the end of an expression", Token::toString(TokenType::Semicolon).data());

        mCurrentClosure = mCurrentClosure->parent();

        return closure;
    }

    // Regular variable statement (single identifier)
    inline Ptr<Statement> p_variable_statement(bool is_declaration, const AttributeList& attrs)
    {
        PEXPR_UNUSED(attrs);

        const auto loc = P.cur().Location;

        bool is_mutable = false;
        if (is_declaration)
            is_mutable = P.accept(TokenType::Mutable);

        const std::string varName = P.cur().Type == TokenType::Identifier ? std::get<std::string>(P.cur().Value) : "_unknown_";
        P.expect(TokenType::Identifier);

        // Optional explicit type annotation for declarations: ': TYPE'
        Type declaredType = Type(TypeKind::Unspecified);
        if (is_declaration && P.accept(TokenType::Colon))
            declaredType = p_type();

        P.expect(TokenType::Assign);

        auto expr = p_expression();

        P.expect(TokenType::Semicolon);

        if (is_declaration)
            return std::make_shared<VariableDeclarationStatement>(is_mutable, loc, varName, std::move(expr), declaredType);
        else
            return std::make_shared<VariableAssignmentStatement>(loc, varName, std::move(expr));
    }

    // Destructuring statement (pattern)
    inline Ptr<Statement> p_destructuring_statement(bool is_declaration, const AttributeList& attrs)
    {
        PEXPR_UNUSED(attrs);

        const auto loc = P.cur().Location;

        // Patterns need a * before [
        P.expect(TokenType::Mul);

        // Parse pattern (allow mut/type annotations only for declarations)
        auto pattern = p_pattern(is_declaration);
        if (!pattern)
            return nullptr;

        P.expect(TokenType::Assign);

        auto expr = p_expression();

        P.expect(TokenType::Semicolon);

        if (is_declaration)
            return std::make_shared<DestructuringDeclarationStatement>(loc, pattern, std::move(expr));
        else
            return std::make_shared<DestructuringAssignmentStatement>(loc, pattern, std::move(expr));
    }

    inline ParameterList p_parameter_def_list()
    {
        ParameterList list;

        if (P.cur().Type == TokenType::ClosedParentheses)
            return list; // Empty parameter list
        do {
            const std::string paramName = std::get<std::string>(P.cur().Value);
            P.expect(TokenType::Identifier);
            P.expect(TokenType::Colon);
            const auto type = p_type();
            list.push_back(Parameter{ paramName, type });
        } while (P.accept(TokenType::Comma));

        return list;
    }

    inline Ptr<Pattern> p_pattern(bool for_declaration = true)
    {
        const auto loc = P.cur().Location;
        P.expect(TokenType::OpenSquareBracket);

        Pattern::ElementList elements;
        if (P.cur().Type != TokenType::ClosedSquareBracket) {
            do {
                const auto elemLoc = P.cur().Location;

                // Check if this is a nested pattern (starts with '[') or a simple binding
                if (P.cur().Type == TokenType::OpenSquareBracket) {
                    // Nested pattern
                    auto nestedPattern = p_pattern(for_declaration);
                    if (!nestedPattern)
                        return nullptr;
                    elements.push_back(PatternElement::makeNested(elemLoc, nestedPattern));
                } else {
                    // Simple binding

                    // Check for 'mut' keyword (only allowed in declarations)
                    bool isMutable = false;
                    if (for_declaration && P.accept(TokenType::Mutable)) {
                        isMutable = true;
                    }

                    // Expect identifier
                    const std::string elemName = P.cur().Type == TokenType::Identifier ? std::get<std::string>(P.cur().Value) : "_unknown_";
                    P.expect(TokenType::Identifier);

                    // Optional type annotation (only allowed in declarations)
                    Type declaredType = Type(TypeKind::Unspecified);
                    if (P.accept(TokenType::Colon)) {
                        if (for_declaration) {
                            declaredType = p_type();
                        } else {
                            P.signalError();
                            P.mReporter.errorf(P.cur().Location, "Type annotations are not allowed in destructuring assignments");
                        }
                    }

                    elements.push_back(PatternElement::makeSimple(elemLoc, elemName, declaredType, isMutable));
                }
            } while (P.accept(TokenType::Comma));
        }

        P.expect(TokenType::ClosedSquareBracket);

        if (elements.empty()) {
            P.signalError();
            P.mReporter.errorf(loc, "Pattern must have at least one element");
            return nullptr;
        }

        return std::make_shared<Pattern>(loc, std::move(elements));
    }

    inline AttributeList p_attributes()
    {
        AttributeList attributes;

        if (P.cur().Type == TokenType::ClosedSquareBracket)
            return attributes;

        do {
            if (P.cur().Type != TokenType::Identifier)
                break;

            const std::string attrName = std::get<std::string>(P.cur().Value);
            P.expect(TokenType::Identifier);

            Attribute attr;
            attr.name = attrName;

            // Check for value assignment
            if (P.accept(TokenType::Assign)) {
                // Parse value based on token type
                if (P.cur().Type == TokenType::BooleanLiteral) {
                    attr.value = std::get<bool>(P.cur().Value);
                    P.expect(TokenType::BooleanLiteral);
                } else if (P.cur().Type == TokenType::IntegerLiteral) {
                    attr.value = std::get<Integer>(P.cur().Value);
                    P.expect(TokenType::IntegerLiteral);
                } else if (P.cur().Type == TokenType::NumberLiteral) {
                    attr.value = std::get<Number>(P.cur().Value);
                    P.expect(TokenType::NumberLiteral);
                } else if (P.cur().Type == TokenType::StringLiteral) {
                    attr.value = std::get<std::string>(P.cur().Value);
                    P.expect(TokenType::StringLiteral);
                } else {
                    P.signalError();
                    P.mReporter.errorf(P.cur().Location, "Expected boolean, integer, number, or string literal for attribute value");
                }
            } else {
                // Boolean attribute without value defaults to true
                attr.value = true;
            }

            attributes.push_back(attr);
        } while (P.accept(TokenType::Comma));

        return attributes;
    }

    struct FunctionAttributes {
        bool Extern        = false;
        bool HasSideEffect = false;

        static FunctionAttributes fromAttributes(const AttributeList& attrs)
        {
            FunctionAttributes result;
            bool hadPure = false;
            for (const auto& attr : attrs) {
                if (attr.name == "extern") {
                    result.Extern = attr.isBool() ? attr.getBool() : true;
                    if (!hadPure)
                        result.HasSideEffect = true; // Default we assume external functions have side effects
                } else if (attr.name == "pure") {
                    // pure attribute means no side effect
                    bool pure            = attr.isBool() ? attr.getBool() : true;
                    result.HasSideEffect = !pure;
                    hadPure              = true;
                }
            }
            return result;
        }
    };

    inline Ptr<Statement> p_function_statement(const AttributeList& attrs)
    {
        const auto loc = P.cur().Location;

        // Get attribute list
        FunctionAttributes attr = FunctionAttributes::fromAttributes(attrs);

        // Get function name
        std::string funcName;
        if (const auto fnptr = std::get_if<std::string>(&P.cur().Value))
            funcName = *fnptr;
        else
            funcName = "_error_"; // Should fail in the next line

        P.expect(TokenType::Identifier);
        P.expect(TokenType::OpenParentheses);

        const auto parameters = p_parameter_def_list();

        P.expect(TokenType::ClosedParentheses);

        Type returnType = Type(TypeKind::Unspecified);
        if (P.accept(TokenType::ArrowRight))
            returnType = p_type();

        // Build mangled name from declared parameter types (do NOT include return type).
        std::vector<Type> paramTypes;
        paramTypes.reserve(parameters.size());
        for (const auto& p : parameters)
            paramTypes.push_back(p.ParamType);

        const std::string mangled = makeMangledNameFromTypes(funcName, paramTypes, mCurrentClosure);

        if (!attr.Extern) {
            auto closure    = std::make_shared<Closure>(loc, mCurrentClosure);
            mCurrentClosure = closure.get();

            P.expect(TokenType::Assign);
            Ptr<Expression> expr = p_expression();
            closure->setExpression(expr);
            P.expect(TokenType::Semicolon);

            mCurrentClosure = closure->parent();

            if (expr->type() == ExpressionType::Closure) {
                // Remove the previous closure to directly use this one.
                closure = std::reinterpret_pointer_cast<ClosureExpression>(expr)->closure();
                closure->setParent(mCurrentClosure);
            }

            return std::make_shared<FunctionDeclarationStatement>(loc, funcName, parameters, closure, returnType, mangled, false);
        } else {
            if (returnType.kind() == TypeKind::Unspecified) {
                P.signalError();
                P.mReporter.errorf(P.cur().Location, "Expected an explicit return type for the given function");
            }
            P.expect(TokenType::Semicolon);
            return std::make_shared<FunctionDeclarationStatement>(loc, funcName, parameters, nullptr, returnType, mangled, attr.HasSideEffect);
        }
    }

    // Expressions
    inline Ptr<Expression> p_expression()
    {
        return p_binary_expression();
    }

    static inline std::pair<BinaryOperation, int> binaryOpFromToken(TokenType type)
    {
        switch (type) {
        case TokenType::Or:
            return { BinaryOperation::Or, 6 };
        case TokenType::And:
            return { BinaryOperation::And, 5 };
        case TokenType::Equal:
            return { BinaryOperation::Equal, 4 };
        case TokenType::NotEqual:
            return { BinaryOperation::NotEqual, 4 };
        case TokenType::Less:
            return { BinaryOperation::Less, 4 };
        case TokenType::Greater:
            return { BinaryOperation::Greater, 4 };
        case TokenType::LessEqual:
            return { BinaryOperation::LessEqual, 4 };
        case TokenType::GreaterEqual:
            return { BinaryOperation::GreaterEqual, 4 };
        case TokenType::Plus:
            return { BinaryOperation::Add, 3 };
        case TokenType::Minus:
            return { BinaryOperation::Sub, 3 };
        case TokenType::Mul:
            return { BinaryOperation::Mul, 2 };
        case TokenType::Div:
            return { BinaryOperation::Div, 2 };
        case TokenType::Mod:
            return { BinaryOperation::Mod, 2 };
        case TokenType::Pow:
            return { BinaryOperation::Pow, 1 };
        default:
            // Other tokens, fallthrough
            return { BinaryOperation::Add, -1 };
        }
    }

    inline Ptr<Expression> p_binary_expression(int max_precedence = 6 /*Max precedence*/)
    {
        auto left = p_unary_expression();
        while (true) {
            const auto loc = P.cur().Location;
            const auto bin = binaryOpFromToken(P.cur().Type);
            const auto op  = std::get<0>(bin);
            const int prec = std::get<1>(bin);

            if (prec > max_precedence || prec <= 0)
                break;
            P.next();

            auto right = p_binary_expression(prec - 1);
            left       = std::make_shared<BinaryExpression>(loc, op, left, right);
        }

        return left;
    }

    inline Ptr<Expression> p_unary_expression()
    {
        const auto loc = P.cur().Location;
        if (P.accept(TokenType::Plus))
            return std::make_shared<UnaryExpression>(loc, UnaryOperation::Pos, p_unary_expression());
        if (P.accept(TokenType::Minus))
            return std::make_shared<UnaryExpression>(loc, UnaryOperation::Neg, p_unary_expression());
        if (P.accept(TokenType::ExclamationMark))
            return std::make_shared<UnaryExpression>(loc, UnaryOperation::Not, p_unary_expression());

        return p_postfix_expression();
    }

    inline Ptr<Expression> p_postfix_expression()
    {
        auto expr = p_call_expression();

        // Loop to handle chained postfix operators
        while (true) {
            if (P.cur().Type == TokenType::Dot) {
                const auto loc = P.cur().Location;
                auto swizzle   = p_swizzle();

                if (!checkSwizzle(swizzle)) {
                    P.signalError();
                    P.mReporter.errorf(loc, "Given access '%s' is invalid", std::string(swizzle).c_str());
                }

                expr = std::make_shared<SwizzleExpression>(loc, expr, swizzle);
                continue;
            }

            // explicit cast syntax: "<expr> as <type>"
            if (P.accept(TokenType::As)) {
                // Use the expression's original location for the cast node
                const auto loc    = expr->location();
                const auto toType = p_type();
                expr              = std::make_shared<CastExpression>(loc, toType, expr);
                continue;
            }

            // [i]
            if (P.accept(TokenType::OpenSquareBracket)) {
                size_t index     = 0;
                const auto token = P.cur();
                P.accept(TokenType::IntegerLiteral);
                const Integer i = std::get<Integer>(token.Value);
                if (i < 0) {
                    P.signalError();
                    P.mReporter.errorf(token.Location, "Negative index given for vector lookup");
                } else {
                    index = (size_t)i;
                }
                P.expect(TokenType::ClosedSquareBracket);

                expr = std::make_shared<AccessExpression>(token.Location, expr, index);
                continue;
            }

            // No more postfix operators
            break;
        }

        return expr;
    }

    inline Ptr<Expression> p_call_expression()
    {
        if (P.cur(0).Type == TokenType::Identifier
            && P.cur(1).Type == TokenType::OpenParentheses) {
            const auto loc             = P.cur().Location;
            const std::string funcName = std::get<std::string>(P.cur().Value);

            P.expect(TokenType::Identifier);
            P.expect(TokenType::OpenParentheses);

            std::vector<Ptr<Expression>> parameters;

            if (!P.accept(TokenType::ClosedParentheses)) {
                p_parameter_list(parameters);
                P.expect(TokenType::ClosedParentheses);
            }

            return std::make_shared<CallExpression>(loc, funcName, std::move(parameters));
        }

        return p_enclosed_expression();
    }

    inline void p_parameter_list(std::vector<Ptr<Expression>>& list)
    {
        do {
            auto expr = p_expression();
            PEXPR_ASSERT(expr != nullptr, "Got empty parameter value");
            list.push_back(expr);
        } while (P.accept(TokenType::Comma));
    }

    inline Ptr<Expression> p_if_expression()
    {
        const auto loc = P.cur().Location;
        BranchExpression::ClosureList branches;

        P.expect(TokenType::If);
        const auto firstCondition = p_expression();
        P.expect(TokenType::OpenBraces);
        const auto firstClosure = p_closure();
        P.expect(TokenType::ClosedBraces);

        branches.push_back(BranchExpression::SingleBranch{ firstCondition, firstClosure });

        while (P.cur().Type == TokenType::Elif) {
            P.expect(TokenType::Elif);
            const auto condition = p_expression();
            P.expect(TokenType::OpenBraces);
            const auto closure = p_closure();
            P.expect(TokenType::ClosedBraces);

            branches.push_back(BranchExpression::SingleBranch{ condition, closure });
        }

        P.expect(TokenType::Else);
        P.expect(TokenType::OpenBraces);
        const auto elseClosure = p_closure();
        P.expect(TokenType::ClosedBraces);

        return std::make_shared<BranchExpression>(loc, branches, elseClosure);
    }

    inline Ptr<Expression> p_enclosed_expression()
    {
        // if ... { ... } else { ... }
        if (P.cur(0).Type == TokenType::If) {
            return p_if_expression();
        }

        // { ... }
        if (P.accept(TokenType::OpenBraces)) {
            auto closure = p_closure();
            P.expect(TokenType::ClosedBraces);
            return std::make_shared<ClosureExpression>(closure->location(), closure);
        }

        // ( ... )
        if (P.accept(TokenType::OpenParentheses)) {
            auto expr = p_expression();
            P.expect(TokenType::ClosedParentheses);
            return expr;
        }

        // [ ... ]
        if (P.accept(TokenType::OpenSquareBracket)) {
            auto expr = p_tuple_expression();
            P.expect(TokenType::ClosedSquareBracket);
            return expr;
        }

        return p_primary_expression();
    }

    inline Ptr<Expression> p_tuple_expression()
    {
        const auto loc = P.cur().Location;
        std::vector<Ptr<Expression>> vector;
        do {
            auto expr = p_expression();
            PEXPR_ASSERT(expr != nullptr, "Got empty parameter value");
            vector.push_back(expr);
        } while (P.accept(TokenType::Comma));

        if (vector.size() == 0) {
            P.signalError();
            P.mReporter.errorf(loc, "Invalid empty tuple given");
        }

        return std::make_shared<TupleExpression>(loc, std::move(vector));
    }

    inline Ptr<Expression> p_primary_expression()
    {
        const auto value = P.cur();
        if (P.accept(TokenType::BooleanLiteral))
            return std::make_shared<LiteralExpression>(value.Location, Type(TypeKind::Boolean), value.Value);

        if (P.accept(TokenType::NumberLiteral))
            return std::make_shared<LiteralExpression>(value.Location, Type(TypeKind::Number), value.Value);

        if (P.accept(TokenType::IntegerLiteral))
            return std::make_shared<LiteralExpression>(value.Location, Type(TypeKind::Integer), value.Value);

        if (P.accept(TokenType::StringLiteral))
            return std::make_shared<LiteralExpression>(value.Location, Type(TypeKind::String), value.Value);

        if (P.accept(TokenType::Identifier))
            return std::make_shared<VariableExpression>(value.Location, std::get<std::string>(value.Value));

        // Only print error if error was not introduced by lexer
        if (P.cur().Type != TokenType::Error)
            P.error(std::array<TokenType, 8>{ TokenType::OpenParentheses, TokenType::OpenBraces, TokenType::If, TokenType::BooleanLiteral, TokenType::NumberLiteral, TokenType::IntegerLiteral, TokenType::StringLiteral, TokenType::Identifier });
        return std::make_shared<VariableExpression>(value.Location, "__error__");
    }

    inline bool checkSwizzle(std::string_view swizzle)
    {
        if (swizzle.size() < 1 || swizzle.size() > 4)
            return false;
        for (auto c : swizzle) {
            if (c != 'x' && c != 'y' && c != 'z' && c != 'w' && c != 'r' && c != 'g' && c != 'b' && c != 'a')
                return false;
        }
        return true;
    }

    inline std::string p_swizzle()
    {
        P.expect(TokenType::Dot);
        auto token = P.cur();
        if (P.expect(TokenType::Identifier))
            return std::get<std::string>(token.Value);
        else
            return {};
    }

    inline Ptr<Statement> p_type_alias_statement(const AttributeList& attrs)
    {
        PEXPR_UNUSED(attrs);

        const auto loc              = P.cur().Location;
        const std::string aliasName = P.cur().Type == TokenType::Identifier ? std::get<std::string>(P.cur().Value) : "_unknown_";
        P.expect(TokenType::Identifier);
        P.expect(TokenType::Assign);
        const auto aliasedType = p_type();
        P.expect(TokenType::Semicolon);

        // Register the type alias in the current symbol table
        if (mCurrentClosure) {
            if (!mCurrentClosure->symbols().addTypeAlias(aliasName, aliasedType)) {
                P.signalError();
                P.mReporter.errorf(loc, "Type alias '%s' already defined in the current scope", aliasName.c_str());
            }
        }

        return std::make_shared<TypeAliasStatement>(loc, aliasName, aliasedType);
    }

    inline Type p_type()
    {
        // First check if it's an identifier (could be a type alias)
        if (P.cur().Type == TokenType::Identifier) {
            const std::string name = std::get<std::string>(P.cur().Value);
            P.next();

            // Try to resolve as a type alias from the current symbol table
            if (mCurrentClosure) {
                if (const auto alias = mCurrentClosure->symbols().lookupTypeAlias(name); alias.has_value())
                    return *alias;
            }

            // Not a known type alias
            P.mReporter.errorf(P.cur().Location, "Unknown type name '%s'", name.c_str());
            return Type(TypeKind::Error);
        } else if (P.cur().Type == TokenType::OpenSquareBracket) {
            // Tuple type: [T1, T2, ...]
            P.expect(TokenType::OpenSquareBracket);
            std::vector<Type> components;
            if (P.cur().Type != TokenType::ClosedSquareBracket) {
                do {
                    components.push_back(p_type());
                } while (P.accept(TokenType::Comma));
            }
            P.expect(TokenType::ClosedSquareBracket);
            if (components.empty()) {
                P.signalError();
                P.mReporter.errorf(P.cur().Location, "Tuple type must have at least one component");
                return Type(TypeKind::Error);
            }
            return Type(std::move(components));
        } else {
            P.error(std::to_array<TokenType>({ TokenType::Identifier, TokenType::OpenSquareBracket }));
            return Type(TypeKind::Error);
        }
    }
};

Ptr<Closure> parse_translation_unit(Parser& parser, const SymbolTable* globals)
{
    return ParserGrammar(parser, globals).parse();
}
} // namespace PExpr::parser