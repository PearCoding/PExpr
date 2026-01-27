#pragma once

#include "Location.h"
#include "Token.h"
#include "utils/Reporter.h"

#include <istream>

namespace PExpr::parser {
class Lexer {
public:
    Lexer(std::istream& stream, utils::Reporter& reporter);

    Token next();

    inline const Location& loc() const { return mLocation; }

private:
    void eat();
    void eatSpaces();
    void eatComments(bool multiline);
    [[nodiscard]] Token parseNumber();
    [[nodiscard]] Token parseString(uint8_t mark);

    void appendDigits(int base);

    void append();
    void appendChar();
    bool accept(uint8_t c);

    [[nodiscard]] inline uint8_t peek() const { return mChar; }
    [[nodiscard]] inline bool eof() const { return mStream.eof(); }

    std::istream& mStream;
    uint8_t mChar;
    Location mLocation;
    std::string mTemp; // Contains identifiers etc
    utils::Reporter& mReporter;
};
} // namespace PExpr::parser
