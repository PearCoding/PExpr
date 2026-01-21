#include "Parser.h"
#include "Expression.h"
#include "Logger.h"
#include "Mangler.h"
#include "Parameter.h"
#include "Statement.h"

namespace PExpr::internal {
Parser::Parser(Lexer& lexer, Reporter& reporter)
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
        if (!P.hasError() && P.cur().Type != TokenType::Eof)
            P.mReporter.errorf(Location(0), "Parsing stopped before end of stream!");

        return closure;
    }

private:
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
            if (P.accept(TokenType::Let)) {
                // Variable declaration
                closure->addStatement(p_variable_statement(true));
            } else if (P.accept(TokenType::Function)) {
                // Function
                closure->addStatement(p_function_statement());
            } else if (P.cur(0).Type == TokenType::Identifier) {
                if (P.cur(1).Type == TokenType::Assign) {
                    // Variable
                    closure->addStatement(p_variable_statement(false));
                } else {
                    break;
                }
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

        // TODO: The location of the warning is incorrect and slightly off (a single token wide)
        if (P.accept(TokenType::Semicolon))
            P.mReporter.warningf(RT_WARNING_TRAILING_SEMICOLON, P.cur().Location, "Trailing '%s' at the end of an expression", Token::toString(TokenType::Semicolon).data());

        mCurrentClosure = mCurrentClosure->parent();

        return closure;
    }

    // Statements
    inline Ptr<Statement> p_variable_statement(bool is_declaration)
    {
        const auto loc = P.cur().Location;

        bool is_mutable = false;
        if (is_declaration)
            is_mutable = P.accept(TokenType::Mutable);

        const std::string varName = P.cur().Type == TokenType::Identifier ? std::get<std::string>(P.cur().Value) : "_unknown_";
        P.expect(TokenType::Identifier);

        // Optional explicit type annotation for declarations: ': TYPE'
        ElementaryType declaredType = ElementaryType::Unspecified;
        if (is_declaration && P.accept(TokenType::Colon))
            declaredType = p_elementary_type();

        P.expect(TokenType::Assign);

        auto expr = p_expression();

        P.expect(TokenType::Semicolon);

        if (is_declaration)
            return std::make_shared<VariableDeclarationStatement>(is_mutable, loc, varName, std::move(expr), declaredType);
        else
            return std::make_shared<VariableAssignmentStatement>(loc, varName, std::move(expr));
    }

    inline ParameterList p_parameter_def_list()
    {
        ParameterList list;

        if (P.cur().Type == TokenType::ClosedParentheses)
            return list; // Empty parameter list
        do {
            const std::string paramName = std::get<std::string>(P.cur().Value);
            P.expect(TokenType::Identifier);

            // if (P.cur().Type == TokenType::Colon) {
            P.expect(TokenType::Colon);
            const ElementaryType type = p_elementary_type();
            list.push_back(Parameter{ paramName, type });
            // } else {
            //     list.push_back(FunctionStatement::Parameter{ paramName, ElementaryType::Unspecified });
            // }
        } while (P.accept(TokenType::Comma));

        return list;
    }

    struct FunctionAttributes {
        bool Extern        = false;
        bool HasSideEffect = false;
    };
    inline FunctionAttributes p_function_attributes()
    {
        if (P.cur().Type == TokenType::ClosedSquareBracket)
            return FunctionAttributes{};

        bool hadExtern = false;
        bool hadPure   = false;
        do {
            const std::string attrName = std::get<std::string>(P.cur().Value);
            P.expect(TokenType::Identifier);

            if (attrName == "extern") {
                hadExtern = true;
            } else if (attrName == "pure") {
                hadPure = true;
            } else {
                P.signalError();
                P.mReporter.errorf(P.cur().Location, "Unknown attribute '%s'", attrName.c_str());
            }
        } while (P.accept(TokenType::Comma));

        if (hadExtern) {
            return FunctionAttributes{
                .Extern        = true,
                .HasSideEffect = !hadPure
            };
        } else {
            if (hadPure)
                P.mReporter.warningf(RT_WARNING_PURE_INTERNAL_FUNCTIONS, P.cur().Location, "No need to mark an internal function as pure");
            return FunctionAttributes{
                .Extern        = false,
                .HasSideEffect = false
            };
        }
    }

    inline Ptr<Statement> p_function_statement()
    {
        const auto loc = P.cur().Location;

        // Get attribute list
        FunctionAttributes attr;
        if (P.accept(TokenType::OpenSquareBracket) && P.accept(TokenType::OpenSquareBracket)) {
            attr = p_function_attributes();
            P.expect(TokenType::ClosedSquareBracket);
            P.expect(TokenType::ClosedSquareBracket);
        }

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

        ElementaryType returnType = ElementaryType::Unspecified;
        if (P.accept(TokenType::ArrowRight))
            returnType = p_elementary_type();

        // Build mangled name from declared parameter types (do NOT include return type).
        std::vector<ElementaryType> paramTypes;
        paramTypes.reserve(parameters.size());
        for (const auto& p : parameters)
            paramTypes.push_back(p.Type);

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
            if (returnType == ElementaryType::Unspecified) {
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
        if (P.cur().Type == TokenType::Dot) {
            const auto loc = P.cur().Location;
            auto swizzle   = p_swizzle();

            if (!checkSwizzle(swizzle)) {
                P.signalError();
                P.mReporter.errorf(loc, "Given access '%s' is invalid", std::string(swizzle).c_str());
            }

            return std::make_shared<AccessExpression>(loc, expr, swizzle);
        }

        // explicit cast syntax: "<expr> as <type>"
        if (P.accept(TokenType::As)) {
            // Use the expression's original location for the cast node
            const auto loc              = expr->location();
            const ElementaryType toType = p_elementary_type();
            return std::make_shared<CastExpression>(loc, toType, expr);
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

    inline Ptr<Expression> p_if_branch()
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
            return p_if_branch();
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
            auto expr = p_vector_expression();
            P.expect(TokenType::ClosedSquareBracket);
            return expr;
        }

        return p_primary_expression();
    }

    inline Ptr<Expression> p_vector_expression()
    {
        const auto loc = P.cur().Location;
        std::vector<Ptr<Expression>> vector;
        do {
            auto expr = p_expression();
            PEXPR_ASSERT(expr != nullptr, "Got empty parameter value");
            vector.push_back(expr);
        } while (P.accept(TokenType::Comma));

        if (vector.size() < 2 || vector.size() > 4) {
            P.signalError();
            P.mReporter.errorf(loc, "Invalid size vector of %zu given", vector.size());
        }

        return std::make_shared<VectorExpression>(loc, std::move(vector));
    }

    inline Ptr<Expression> p_primary_expression()
    {
        const auto value = P.cur();
        if (P.accept(TokenType::BooleanLiteral))
            return std::make_shared<LiteralExpression>(value.Location, ElementaryType::Boolean, value.Value);

        if (P.accept(TokenType::NumberLiteral))
            return std::make_shared<LiteralExpression>(value.Location, ElementaryType::Number, value.Value);

        if (P.accept(TokenType::IntegerLiteral))
            return std::make_shared<LiteralExpression>(value.Location, ElementaryType::Integer, value.Value);

        if (P.accept(TokenType::StringLiteral))
            return std::make_shared<LiteralExpression>(value.Location, ElementaryType::String, value.Value);

        if (P.accept(TokenType::Identifier))
            return std::make_shared<VariableExpression>(value.Location, std::get<std::string>(value.Value));

        // Only print error if error was not introduced by lexer
        if (P.cur().Type != TokenType::Error)
            P.error(std::array<TokenType, 8>{ TokenType::OpenParentheses, TokenType::OpenBraces, TokenType::If, TokenType::BooleanLiteral, TokenType::NumberLiteral, TokenType::IntegerLiteral, TokenType::StringLiteral, TokenType::Identifier });
        return std::make_shared<ErrorExpression>(value.Location);
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

    inline ElementaryType p_elementary_type()
    {
        switch (P.cur().Type) {
        case TokenType::BooleanType:
            P.next();
            return ElementaryType::Boolean;
        case TokenType::IntegerType:
            P.next();
            return ElementaryType::Integer;
        case TokenType::NumberType:
            P.next();
            return ElementaryType::Number;
        case TokenType::Vec2Type:
            P.next();
            return ElementaryType::Vec2;
        case TokenType::Vec3Type:
            P.next();
            return ElementaryType::Vec3;
        case TokenType::Vec4Type:
            P.next();
            return ElementaryType::Vec4;
        case TokenType::StringType:
            P.next();
            return ElementaryType::String;
        default:
            P.error(std::array<TokenType, 7>{ TokenType::BooleanType, TokenType::IntegerType, TokenType::NumberType,
                                              TokenType::Vec2Type, TokenType::Vec3Type, TokenType::Vec4Type,
                                              TokenType::StringType });
            return ElementaryType::Error;
        }
    }
};

Ptr<Closure> parse_translation_unit(Parser& parser, const SymbolTable* globals)
{
    return ParserGrammar(parser, globals).parse();
}
} // namespace PExpr::internal
