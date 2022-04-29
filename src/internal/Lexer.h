#pragma once

#include "Token.h"

#include <istream>

namespace PExpr {
class Lexer {
public:
    Lexer(std::istream& stream);

    Token next();

    inline size_t currentPosition() const { return mPosition; }

private:
    void eat();
    void eatSpaces();
    void eatComments();
    Token parseNumber();
    Token parseString(uint8_t mark);

    void appendDigits(int base);

    void append();
    void appendChar();
    bool accept(uint8_t c);

    inline uint8_t peek() const { return mChar; }
    inline bool eof() const { return mStream.eof(); }

    std::istream& mStream;
    uint8_t mChar;
    size_t mPosition;
    std::string mTemp; // Contains identifiers etc
};
} // namespace PExpr