#include "Lexer.h"
#include "Logger.h"

namespace PExpr::internal {
Lexer::Lexer(std::istream& stream)
    : mStream(stream)
    , mChar(0)
    , mLocation(0)
    , mTemp{}
{
    eat();
}

Token Lexer::next()
{
    while (true) {
        mTemp.clear();

        eatSpaces();

        if (eof())
            return Token(mLocation, TokenType::Eof);

        if (accept('('))
            return Token(mLocation - 1, TokenType::OpenParanthese);
        if (accept(')'))
            return Token(mLocation - 1, TokenType::ClosedParanthese);
        if (accept('+'))
            return Token(mLocation - 1, TokenType::Plus);
        if (accept('-'))
            return Token(mLocation - 1, TokenType::Minus);
        if (accept('*'))
            return Token(mLocation - 1, TokenType::Mul);
        if (accept('/')) {
            if (accept('*')) {
                eatComments();
                continue;
            }
            return Token(mLocation - 1, TokenType::Div);
        }
        if (accept('%'))
            return Token(mLocation - 1, TokenType::Mod);
        if (accept('^'))
            return Token(mLocation - 1, TokenType::Pow);
        if (accept('.'))
            return Token(mLocation - 1, TokenType::Dot);
        if (accept(','))
            return Token(mLocation - 1, TokenType::Comma);

        if (accept('=')) {
            if (accept('='))
                return Token(mLocation - 2, TokenType::Equal);
        }

        if (accept('&')) {
            if (accept('&'))
                return Token(mLocation - 2, TokenType::And);
        }

        if (accept('|')) {
            if (accept('|'))
                return Token(mLocation - 2, TokenType::Or);
        }

        if (accept('!')) {
            if (accept('='))
                return Token(mLocation - 2, TokenType::NotEqual);
            return Token(mLocation - 1, TokenType::ExclamationMark);
        }

        if (accept('<')) {
            if (accept('='))
                return Token(mLocation - 2, TokenType::Less);
            return Token(mLocation - 1, TokenType::LessEqual);
        }

        if (accept('>')) {
            if (accept('='))
                return Token(mLocation - 2, TokenType::Greater);
            return Token(mLocation - 1, TokenType::GreaterEqual);
        }

        if (accept('\"'))
            return parseString('\"');
        if (accept('\''))
            return parseString('\'');

        if (std::isdigit(peek()))
            return parseNumber();

        if (std::isalpha(peek()) || peek() == '_') {
            append();
            while ((std::isalnum(peek()) || peek() == '_') && !eof())
                append();

            if (mTemp == "true")
                return Token(mLocation - mTemp.size(), TokenType::Boolean).With(true);
            if (mTemp == "false")
                return Token(mLocation - mTemp.size(), TokenType::Boolean).With(false);

            return Token(mLocation - mTemp.size(), TokenType::Identifier).With(mTemp);
        }

        append();
        PEXPR_LOG(LogLevel::Error) << mLocation << ": Unknown token '" << mTemp << "'" << std::endl;
        return Token(mLocation, TokenType::Error);
    }
}

void Lexer::eat()
{
    ++mLocation;
    mChar = (uint8)mStream.get();
}

void Lexer::eatSpaces()
{
    while (!eof() && std::isspace(peek()))
        eat();
}

void Lexer::eatComments()
{
    while (true) {
        while (!eof() && peek() != '*')
            eat();
        if (eof())
            break;
        eat();
        if (accept('/'))
            break;
    }
}

Token Lexer::parseNumber()
{
    const Location startLoc = mLocation;

    int base = 10;
    // Prefix starting with '0'
    if (accept('0')) {
        if (accept('b'))
            base = 2;
        else if (accept('x'))
            base = 16;
        else if (accept('o'))
            base = 8;
    }

    appendDigits(base);

    bool exp = false, fractional = false;
    if (base == 10) {
        // Parse fractional part
        if (accept('.')) {
            fractional = true;
            appendDigits(base);
        }

        // Parse exponent
        if (accept('e')) {
            exp = true;
            if (!accept('+'))
                accept('-');
            appendDigits(base);
        }
    }

    // Skip prefix for strtol and friends
    const char* digit_ptr = mTemp.c_str() + (base == 10 ? 0 : 2);
    const char* last_ptr  = mTemp.c_str() + mTemp.size();
    auto invalid_digit    = [=](char c) { return c - '0' >= base; };

    // Check digits
    if (base < 10 && std::find_if(digit_ptr, last_ptr, invalid_digit) != last_ptr)
        PEXPR_LOG(LogLevel::Error) << startLoc << ": Invalid literal '" << mTemp << "'" << std::endl;

    if (exp || fractional)
        return Token(startLoc, TokenType::Float).With(Number(std::strtod(digit_ptr, nullptr)));
    return Token(startLoc, TokenType::Integer).With(Integer(std::strtoull(digit_ptr, nullptr, base)));
}

Token Lexer::parseString(uint8_t mark)
{
    const Location startLoc = mLocation;

    std::string str;
    while (true) {
        size_t pos = mTemp.size();
        while (!eof() && peek() != mark)
            appendChar();
        if (eof() || !accept(mark)) {
            PEXPR_LOG(LogLevel::Error) << mLocation << ": Unterminated string literal" << std::endl;
            return Token(mLocation, TokenType::Error);
        }
        str += mTemp.substr(pos, mTemp.size() - (pos + 1));
        eatSpaces();
        if (!accept(mark))
            break;
    }
    return Token(startLoc, TokenType::String).With(str);
}

void Lexer::append()
{
    mTemp += mChar;
    eat();
}

void Lexer::appendChar()
{
    if (peek() == '\\') {
        eat();

        switch (peek()) {
        case 't':
            mTemp += '\t';
            eat();
            break;
        case 'n':
            mTemp += '\n';
            eat();
            break;
        case 'a':
            mTemp += '\a';
            eat();
            break;
        case 'b':
            mTemp += '\b';
            eat();
            break;
        case 'r':
            mTemp += '\r';
            eat();
            break;
        case 'v':
            mTemp += '\v';
            eat();
            break;
        case 'f':
            mTemp += '\f';
            eat();
            break;
        case '\\':
            mTemp += '\\';
            eat();
            break;
        case '\'':
            mTemp += '\'';
            eat();
            break;
        case '\"':
            mTemp += '\"';
            eat();
            break;
        case 'x': // Binary [2]
        case 'u': // Unicode [4]
        case 'U': // Unicode [8]
        {
            size_t length = peek() == 'x' ? 2 : (peek() == 'u' ? 4 : 8);
            eat();
            std::string uni_val;
            for (size_t i = 0; i < length; ++i) {
                if (eof()) {
                    PEXPR_LOG(LogLevel::Error) << mLocation - 1 << ": Invalid use of Unicode escape sequence" << std::endl;
                    break;
                }

                uni_val += peek();
                eat();
            }

            if (uni_val.length() == length) {
                size_t r = 0;
                unsigned long uni;
                try {
                    uni = std::stoul(uni_val, &r, 16);
                } catch (const std::exception&) {
                    PEXPR_LOG(LogLevel::Error) << mLocation - 1 << ": Given Unicode escape sequence is invalid" << std::endl;
                    break;
                }

                if (r != length) {
                    PEXPR_LOG(LogLevel::Error) << mLocation - 1 << ": Given Unicode escape sequence is invalid" << std::endl;
                } else if (length != 2) {
                    if (uni <= 0x7F) {
                        mTemp += (char)uni;
                    } else if (uni <= 0x7FF) {
                        unsigned long d = uni & 0x7FF;
                        mTemp += char(0xC0 | ((d & 0x7C0) >> 6));
                        mTemp += char(0x80 | (d & 0x3F));
                    } else if (uni <= 0xFFFF) {
                        unsigned long d = uni & 0xFFFF;
                        mTemp += char(0xE0 | ((d & 0xF000) >> 12));
                        mTemp += char(0x80 | ((d & 0xFC0) >> 6));
                        mTemp += char(0x80 | (d & 0x3F));
                    } else if (uni <= 0x10FFFF) {
                        unsigned long d = uni & 0x10FFFF;
                        mTemp += char(0xF0 | ((d & 0x1C0000) >> 18));
                        mTemp += char(0x80 | ((d & 0x3F000) >> 12));
                        mTemp += char(0x80 | ((d & 0xFC0) >> 6));
                        mTemp += char(0x80 | (d & 0x3F));
                    } else {
                        PEXPR_LOG(LogLevel::Error) << mLocation - 1 << ": Given Unicode range" << std::endl;
                    }
                } else { // Binary
                    mTemp += (char)uni;
                }
            } else {
                PEXPR_LOG(LogLevel::Error) << mLocation - 1 << ": Invalid length of Unicode escape sequence" << std::endl;
            }
        } break;
        default:
            PEXPR_LOG(LogLevel::Error) << mLocation - 1 << ": Invalid escape sequence '\\" << peek() << "'" << std::endl;
            eat();
            break;
        }
    } else {
        append();
    }
}

void Lexer::appendDigits(int base)
{
    while (std::isdigit(peek()) || (base == 16 && peek() >= 'a' && peek() <= 'f') || (base == 16 && peek() >= 'A' && peek() <= 'F'))
        append();
}

bool Lexer::accept(uint8_t c)
{
    if (peek() == c) {
        append();
        return true;
    } else {
        return false;
    }
}

} // namespace PExpr