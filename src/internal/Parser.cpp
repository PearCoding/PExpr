#include "Parser.h"
#include "Expression.h"
#include "Logger.h"

namespace PExpr {
Parser::Parser(Lexer& lexer)
    : mLexer(lexer)
    , mCurrentToken()
{
}

Ptr<Expression> parse_translation_unit(Parser& parser);
Ptr<Expression> Parser::parse()
{
    for (size_t i = 0; i < mCurrentToken.size(); ++i)
        mCurrentToken[i] = mLexer.next();
    return parse_translation_unit(*this);
}

bool Parser::expect(TokenType type)
{
    bool same = cur().Type == type;
    if (!same)
        PEXPR_LOG(LogLevel::Error) << "At " << cur().Location << ": Expected '" << Token::toString(type) << "' but got '" << Token::toString(cur().Type) << "'" << std::endl;
    next();
    return same;
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
    mCurrentToken[mCurrentToken.size() - 1] = mLexer.next();
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
        if (P.cur().Type != TokenType::Eof)
            PEXPR_LOG(LogLevel::Error) << "Parsing stopped before end of stream!" << std::endl;

        return expr;
    }

private:
    inline Ptr<Expression> p_expression()
    {
        if (P.cur().Type == TokenType::Eof)
            return nullptr;

        return p_logical_expression();
    }

    inline Ptr<Expression> p_logical_expression()
    {
        auto left = p_equality_expression();
        if (P.accept(TokenType::And))
            return std::make_shared<BinaryExpression>(BinaryOperation::And, left, p_logical_expression());
        if (P.accept(TokenType::Or))
            return std::make_shared<BinaryExpression>(BinaryOperation::Or, left, p_logical_expression());

        return left;
    }

    inline Ptr<Expression> p_equality_expression()
    {
        auto left = p_relational_expression();
        if (P.accept(TokenType::Equal))
            return std::make_shared<BinaryExpression>(BinaryOperation::Equal, left, p_equality_expression());
        if (P.accept(TokenType::NotEqual))
            return std::make_shared<BinaryExpression>(BinaryOperation::NotEqual, left, p_equality_expression());

        return left;
    }

    inline Ptr<Expression> p_relational_expression()
    {
        auto left = p_additive_expression();
        if (P.accept(TokenType::Less))
            return std::make_shared<BinaryExpression>(BinaryOperation::Less, left, p_relational_expression());
        if (P.accept(TokenType::Greater))
            return std::make_shared<BinaryExpression>(BinaryOperation::Greater, left, p_relational_expression());
        if (P.accept(TokenType::LessEqual))
            return std::make_shared<BinaryExpression>(BinaryOperation::LessEqual, left, p_relational_expression());
        if (P.accept(TokenType::GreaterEqual))
            return std::make_shared<BinaryExpression>(BinaryOperation::GreaterEqual, left, p_relational_expression());

        return left;
    }

    inline Ptr<Expression> p_additive_expression()
    {
        auto left = p_multiplicative_expression();
        if (P.accept(TokenType::Plus))
            return std::make_shared<BinaryExpression>(BinaryOperation::Add, left, p_additive_expression());
        if (P.accept(TokenType::Minus))
            return std::make_shared<BinaryExpression>(BinaryOperation::Sub, left, p_additive_expression());

        return left;
    }

    inline Ptr<Expression> p_multiplicative_expression()
    {
        auto left = p_unary_expression();
        if (P.accept(TokenType::Mul))
            return std::make_shared<BinaryExpression>(BinaryOperation::Mul, left, p_multiplicative_expression());
        if (P.accept(TokenType::Div))
            return std::make_shared<BinaryExpression>(BinaryOperation::Div, left, p_multiplicative_expression());
        if (P.accept(TokenType::Mod))
            return std::make_shared<BinaryExpression>(BinaryOperation::Mod, left, p_multiplicative_expression());
        if (P.accept(TokenType::Pow))
            return std::make_shared<BinaryExpression>(BinaryOperation::Pow, left, p_multiplicative_expression());

        return left;
    }

    inline Ptr<Expression> p_unary_expression()
    {
        if (P.accept(TokenType::Plus))
            return p_unary_expression(); // Ignore
        if (P.accept(TokenType::Minus))
            return std::make_shared<UnaryExpression>(UnaryOperation::Neg, p_unary_expression());
        if (P.accept(TokenType::ExclamationMark))
            return std::make_shared<UnaryExpression>(UnaryOperation::Not, p_unary_expression());

        return p_postfix_expression();
    }

    inline Ptr<Expression> p_postfix_expression()
    {
        // Call
        if (P.cur(0).Type == TokenType::Identifier
            && P.cur(1).Type == TokenType::OpenParanthese) {
            auto call = p_call_expression();

            if (P.cur(0).Type == TokenType::Dot) {
                auto swizzle = p_swizzle();
                return std::make_shared<AccessExpression>(call, swizzle);
            }
            return call;
        }

        return p_primary_expression();
    }

    inline Ptr<Expression> p_call_expression()
    {
        const std::string funcName = std::get<std::string>(P.cur(0).Value);
        
        P.expect(TokenType::Identifier);
        P.expect(TokenType::OpenParanthese);

        std::vector<Ptr<Expression>> parameters;

        if (!P.accept(TokenType::ClosedParanthese)) {
            p_parameter_list(parameters);
            P.expect(TokenType::ClosedParanthese);
        }

        return std::make_shared<CallExpression>(funcName, std::move(parameters));
    }

    inline void p_parameter_list(std::vector<Ptr<Expression>>& list)
    {
        do {
            auto expr = p_logical_expression();
            PEXPR_ASSERT(expr != nullptr, "Got empty parameter value");
            list.push_back(expr);
        } while (P.accept(TokenType::Comma));
    }

    inline Ptr<Expression> p_primary_expression()
    {
        if (P.accept(TokenType::OpenParanthese)) {
            auto expr = p_expression();
            P.expect(TokenType::ClosedParanthese);

            if (P.cur(0).Type == TokenType::Dot) {
                auto swizzle = p_swizzle();
                return std::make_shared<AccessExpression>(expr, swizzle);
            }
            return expr;
        }

        const auto value = P.cur();
        if (P.accept(TokenType::Boolean))
            return std::make_shared<ConstExpression>(ElementaryType::Boolean, value.Value);

        if (P.accept(TokenType::Float))
            return std::make_shared<ConstExpression>(ElementaryType::Number, value.Value);

        if (P.accept(TokenType::Integer))
            return std::make_shared<ConstExpression>(ElementaryType::Integer, value.Value);

        if (P.accept(TokenType::String))
            return std::make_shared<ConstExpression>(ElementaryType::String, value.Value);

        P.expect(TokenType::Identifier);
        auto var = std::make_shared<VariableExpression>(std::get<std::string>(value.Value));

        if (P.cur(0).Type == TokenType::Dot) {
            auto swizzle = p_swizzle();
            return std::make_shared<AccessExpression>(var, swizzle);
        }
        return var;
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
} // namespace PExpr