#include "Parser.h"
#include "Logger.h"

namespace PExpr::internal {
Parser::Parser(Lexer& lexer)
    : mLexer(lexer)
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
            PEXPR_LOG(LogLevel::Error) << cur().Location << ": Expected '" << Token::toString(type) << "' but input terminated early" << std::endl;
        else
            PEXPR_LOG(LogLevel::Error) << cur().Location << ": Expected '" << Token::toString(type) << "' but got '" << Token::toString(cur().Type) << "'" << std::endl;
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
        PEXPR_LOG(LogLevel::Error) << cur().Location << ": Expected {" << expectation << "} but input terminated early" << std::endl;
    else
        PEXPR_LOG(LogLevel::Error) << cur().Location << ": Expected {" << expectation << "} but got '" << Token::toString(cur().Type) << "'" << std::endl;
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
            PEXPR_LOG(LogLevel::Error) << "Parsing stopped before end of stream!" << std::endl;

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
            if (P.accept(TokenType::Mutable)) {
                // Variable
                closure->addStatement(p_variable_statement(true));
            } else if (P.accept(TokenType::Extern)) {
                // Function (extern)
                P.accept(TokenType::Function);
                closure->addStatement(p_function_statement(true));
            } else if (P.accept(TokenType::Function)) {
                // Function (intern)
                closure->addStatement(p_function_statement(false));
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
            PEXPR_LOG(LogLevel::Error) << P.cur().Location << ": Expected an expression at the end of a closure but got '" << Token::toString(TokenType::ClosedBraces) << "' instead" << std::endl;
            return closure;
        }

        closure->setExpression(p_expression());

        // TODO: The location of the warning is incorrect and slightly off (a single token wide)
        if (P.accept(TokenType::Semicolon)) {
            PEXPR_LOG(LogLevel::Warning) << P.cur().Location << ": Trailing '" << Token::toString(TokenType::Semicolon) << "' at the end of an expression" << std::endl;
        }

        mCurrentClosure = mCurrentClosure->parent();

        return closure;
    }

    // Statements
    inline Ptr<Statement> p_variable_statement(bool is_mutable)
    {
        const auto loc            = P.cur().Location;
        const std::string varName = std::get<std::string>(P.cur().Value);

        P.expect(TokenType::Identifier);
        P.expect(TokenType::Assign);

        const auto expr = p_expression();

        P.expect(TokenType::Semicolon);

        return std::make_shared<VariableStatement>(is_mutable, loc, varName, expr);
    }

    inline FunctionStatement::ParameterList p_parameter_def_list()
    {
        FunctionStatement::ParameterList list;

        if (P.cur().Type == TokenType::ClosedParentheses)
            return list; // Empty parameter list
        do {
            const std::string paramName = std::get<std::string>(P.cur().Value);
            P.expect(TokenType::Identifier);

            // if (P.cur().Type == TokenType::Colon) {
            P.expect(TokenType::Colon);
            const ElementaryType type = p_elementary_type();
            list.push_back(FunctionStatement::Parameter{ paramName, type });
            // } else {
            //     list.push_back(FunctionStatement::Parameter{ paramName, ElementaryType::Unspecified });
            // }
        } while (P.accept(TokenType::Comma));

        return list;
    }

    inline Ptr<Statement> p_function_statement(bool is_extern)
    {
        const auto loc = P.cur().Location;

        // TODO: Add support for constructors?
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

        Ptr<Expression> expr;
        if (!is_extern) {
            P.expect(TokenType::Assign);
            expr = p_expression();
        } else if (returnType == ElementaryType::Unspecified) {
            P.signalError();
            PEXPR_LOG(LogLevel::Error) << P.cur().Location << ": Expected an explicit return type for the given function" << std::endl;
        }

        P.expect(TokenType::Semicolon);

        return std::make_shared<FunctionStatement>(loc, funcName, parameters, expr, returnType);
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
                PEXPR_LOG(LogLevel::Error) << loc << ": Given access '" << swizzle << "' is invalid" << std::endl;
            }

            // TODO: Only makes sense for vectors
            return std::make_shared<AccessExpression>(loc, expr, swizzle);
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

        return p_primary_expression();
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
            return ElementaryType::Unspecified;
        }
    }
};

Ptr<Closure> parse_translation_unit(Parser& parser, const SymbolTable* globals)
{
    return ParserGrammar(parser, globals).parse();
}
} // namespace PExpr::internal