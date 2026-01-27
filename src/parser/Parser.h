#pragma once

#include "Lexer.h"
#include "ast/Closure.h"
#include "utils/Reporter.h"

#include <array>

namespace PExpr::parser {
class Parser {
    friend class ParserGrammar;

public:
    Parser(Lexer& lexer, utils::Reporter& reporter);

    [[nodiscard]] Ptr<ast::Closure> parse(const type::SymbolTable* globals);

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
    utils::Reporter& mReporter;
    std::array<Token, 2> mCurrentToken;
    bool mHasError;
};
} // namespace PExpr::parser
