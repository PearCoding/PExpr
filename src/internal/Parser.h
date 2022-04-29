#pragma once

#include "Lexer.h"
#include <array>

namespace PExpr {
class Expression;
class Parser {
    friend class ParserGrammar;

public:
    Parser(Lexer& lexer);

    Ptr<Expression> parse();

    inline bool hasError() const { return mHasError; }

protected:
    bool expect(TokenType type);
    template <size_t N>
    void error(const std::array<TokenType, N>&);
    void eat(TokenType type);
    bool accept(TokenType type);
    void next();
    inline const Token& cur(size_t i = 0) const { return mCurrentToken[i]; }

    Lexer& mLexer;
    std::array<Token, 2> mCurrentToken;
    bool mHasError;
};
} // namespace PExpr