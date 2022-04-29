#pragma once

#include "Lexer.h"

namespace PExpr {
class Expression;
class Parser {
    friend class ParserGrammar;

public:
    Parser(Lexer& lexer);

    Ptr<Expression> parse();

protected:
    bool expect(TokenType type);
    void eat(TokenType type);
    bool accept(TokenType type);
    void next();
    inline const Token& cur(size_t i = 0) const { return mCurrentToken[i]; }

    Lexer& mLexer;
    std::array<Token, 2> mCurrentToken;
};
} // namespace PExpr