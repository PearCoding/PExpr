#include "Parser.h"
#include "Logger.h"

namespace PExpr::internal {
Parser::Parser(Lexer& lexer)
    : mLexer(lexer)
    , mCurrentToken()
    , mHasError(false)
{
}

Ptr<Expression> parse_translation_unit(Parser& parser);
Ptr<Expression> Parser::parse()
{
    mHasError = false;
    for (size_t i = 0; i < mCurrentToken.size(); ++i) {
        mCurrentToken[i] = mLexer.next();
        if (mCurrentToken[i].Type == TokenType::Error) {
            mHasError = true;
            return nullptr;
        }
    }

    return parse_translation_unit(*this);
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
    inline explicit ParserGrammar(Parser& parser)
        : P(parser)
    {
    }

    Parser& P;

    inline Ptr<Expression> parse()
    {
        auto expr = p_expression();
        if (!P.hasError() && P.cur().Type != TokenType::Eof)
            PEXPR_LOG(LogLevel::Error) << "Parsing stopped before end of stream!" << std::endl;

        return expr;
    }

private:
    inline Ptr<Expression> p_expression()
    {
        if (P.cur().Type == TokenType::Eof)
            return nullptr;

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
        // Call
        if (P.cur(0).Type == TokenType::Identifier
            && P.cur(1).Type == TokenType::OpenParanthese) {
            auto call = p_call_expression();

            if (P.cur().Type == TokenType::Dot) {
                const auto loc = P.cur().Location;
                auto swizzle   = p_swizzle();
                return std::make_shared<AccessExpression>(loc, call, swizzle);
            }
            return call;
        }

        return p_primary_expression();
    }

    inline Ptr<Expression> p_call_expression()
    {
        const auto loc             = P.cur().Location;
        const std::string funcName = std::get<std::string>(P.cur().Value);

        P.expect(TokenType::Identifier);
        P.expect(TokenType::OpenParanthese);

        std::vector<Ptr<Expression>> parameters;

        if (!P.accept(TokenType::ClosedParanthese)) {
            p_parameter_list(parameters);
            P.expect(TokenType::ClosedParanthese);
        }

        return std::make_shared<CallExpression>(loc, funcName, std::move(parameters));
    }

    inline void p_parameter_list(std::vector<Ptr<Expression>>& list)
    {
        do {
            auto expr = p_binary_expression();
            PEXPR_ASSERT(expr != nullptr, "Got empty parameter value");
            list.push_back(expr);
        } while (P.accept(TokenType::Comma));
    }

    inline Ptr<Expression> p_primary_expression()
    {
        if (P.accept(TokenType::OpenParanthese)) {
            auto expr = p_binary_expression();
            P.expect(TokenType::ClosedParanthese);

            if (P.cur().Type == TokenType::Dot) {
                const auto loc = P.cur().Location;
                auto swizzle   = p_swizzle();
                return std::make_shared<AccessExpression>(loc, expr, swizzle);
            }
            return expr;
        }

        const auto value = P.cur();
        if (P.accept(TokenType::Boolean))
            return std::make_shared<LiteralExpression>(value.Location, ElementaryType::Boolean, value.Value);

        if (P.accept(TokenType::Float))
            return std::make_shared<LiteralExpression>(value.Location, ElementaryType::Number, value.Value);

        if (P.accept(TokenType::Integer))
            return std::make_shared<LiteralExpression>(value.Location, ElementaryType::Integer, value.Value);

        if (P.accept(TokenType::String))
            return std::make_shared<LiteralExpression>(value.Location, ElementaryType::String, value.Value);

        if (P.accept(TokenType::Identifier)) {
            auto var = std::make_shared<VariableExpression>(value.Location, std::get<std::string>(value.Value));

            if (P.cur().Type == TokenType::Dot) {
                const auto loc = P.cur().Location;
                auto swizzle   = p_swizzle();
                return std::make_shared<AccessExpression>(loc, var, swizzle);
            }
            return var;
        }

        // Only print error if error was not introduced by lexer
        if (P.cur().Type != TokenType::Error)
            P.error(std::array<TokenType, 5>{ TokenType::Boolean, TokenType::Float, TokenType::Integer, TokenType::String, TokenType::Identifier });
        return std::make_shared<ErrorExpression>(value.Location);
    }

    std::string p_swizzle()
    {
        P.expect(TokenType::Dot);
        auto token = P.cur();
        if (P.expect(TokenType::Identifier))
            return std::get<std::string>(token.Value);
        else
            return {};
    }
};

Ptr<Expression> parse_translation_unit(Parser& parser)
{
    return ParserGrammar(parser).parse();
}
} // namespace PExpr::internal