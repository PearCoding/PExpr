#pragma once

#include "Closure.h"
#include "Lexer.h"
#include "Reporter.h"
#include <array>

namespace PExpr::internal {
class Parser {
    friend class ParserGrammar;

public:
    Parser(Lexer& lexer, Reporter& reporter);

    [[nodiscard]] Ptr<Closure> parse(const SymbolTable* globals);

    [[nodiscard]] inline bool hasError() const { return mHasError; }

    inline void signalError() { mHasError = true; }

protected:
    bool expect(TokenType type);
    template <size_t N>
    void error(const std::array<TokenType, N>&);
    void eat(TokenType type);
    bool accept(TokenType type);
    void next();
    [[nodiscard]] inline const Token& cur(size_t i = 0) const { return mCurrentToken[i]; }

    Lexer& mLexer;
    Reporter& mReporter;
    std::array<Token, 2> mCurrentToken;
    bool mHasError;
};
} // namespace PExpr::internal
