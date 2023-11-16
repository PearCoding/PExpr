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

        const Location prevLoc = mLocation;
        if (accept('('))
            return Token(prevLoc, TokenType::OpenParanthese);
        if (accept(')'))
            return Token(prevLoc, TokenType::ClosedParanthese);
        if (accept('{'))
            return Token(prevLoc, TokenType::OpenBraces);
        if (accept('}'))
            return Token(prevLoc, TokenType::ClosedBraces);
        if (accept('+'))
            return Token(prevLoc, TokenType::Plus);
        if (accept('-'))
            return Token(prevLoc, TokenType::Minus);
        if (accept('*'))
            return Token(prevLoc, TokenType::Mul);
        if (accept(':'))
            return Token(prevLoc, TokenType::Colon);
        if (accept(';'))
            return Token(prevLoc, TokenType::Semicolon);
        if (accept('/')) {
            if (accept('*')) {
                eatComments(true);
                continue;
            } else if (accept('/')) {
                eatComments(false);
                continue;
            }
            return Token(prevLoc, TokenType::Div);
        }
        if (accept('%'))
            return Token(prevLoc, TokenType::Mod);
        if (accept('^'))
            return Token(prevLoc, TokenType::Pow);
        if (accept('.'))
            return Token(prevLoc, TokenType::Dot);
        if (accept(','))
            return Token(prevLoc, TokenType::Comma);

        if (accept('=')) {
            if (peek() != '=')
                return Token(prevLoc, TokenType::Assign);
            if (accept('='))
                return Token(prevLoc, TokenType::Equal);
        }

        if (accept('&')) {
            if (accept('&'))
                return Token(prevLoc, TokenType::And);
        }

        if (accept('|')) {
            if (accept('|'))
                return Token(prevLoc, TokenType::Or);
        }

        if (accept('!')) {
            if (accept('='))
                return Token(prevLoc, TokenType::NotEqual);
            return Token(prevLoc, TokenType::ExclamationMark);
        }

        if (accept('<')) {
            if (accept('='))
                return Token(prevLoc, TokenType::Less);
            return Token(prevLoc, TokenType::LessEqual);
        }

        if (accept('>')) {
            if (accept('='))
                return Token(prevLoc, TokenType::Greater);
            return Token(prevLoc, TokenType::GreaterEqual);
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
                return Token(prevLoc, TokenType::BooleanLiteral).With(true);
            if (mTemp == "false")
                return Token(prevLoc, TokenType::BooleanLiteral).With(false);
            if (mTemp == "if")
                return Token(prevLoc, TokenType::If);
            if (mTemp == "elif")
                return Token(prevLoc, TokenType::Elif);
            if (mTemp == "else")
                return Token(prevLoc, TokenType::Else);
            if (mTemp == "mut")
                return Token(prevLoc, TokenType::Mutable);
            if (mTemp == "fn")
                return Token(prevLoc, TokenType::Function);
            if (mTemp == "bool")
                return Token(prevLoc, TokenType::BooleanType);
            if (mTemp == "int")
                return Token(prevLoc, TokenType::IntegerType);
            if (mTemp == "num")
                return Token(prevLoc, TokenType::NumberType);
            if (mTemp == "vec2")
                return Token(prevLoc, TokenType::Vec2Type);
            if (mTemp == "vec3")
                return Token(prevLoc, TokenType::Vec3Type);
            if (mTemp == "vec4")
                return Token(prevLoc, TokenType::Vec4Type);
            if (mTemp == "str")
                return Token(prevLoc, TokenType::StringType);

            return Token(prevLoc, TokenType::Identifier).With(mTemp);
        }

        append();
        PEXPR_LOG(LogLevel::Error) << prevLoc << ": Unknown token '" << mTemp << "'" << std::endl;
        return Token(prevLoc, TokenType::Error);
    }
}

void Lexer::eat()
{
    ++mLocation;
    if (peek() == '\n')
        mLocation.newLine();
    mChar = (uint8)mStream.get();
}

void Lexer::eatSpaces()
{
    while (!eof() && std::isspace(peek()))
        eat();
}

void Lexer::eatComments(bool multiline)
{
    if (multiline) {
        while (true) {
            while (!eof() && peek() != '*')
                eat();
            if (eof())
                break;
            eat();
            if (accept('/'))
                break;
        }
    } else {
        while (!eof() && peek() != '\n')
            eat();
        if (!eof())
            eat();
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
        return Token(startLoc, TokenType::NumberLiteral).With(Number(std::strtod(digit_ptr, nullptr)));
    return Token(startLoc, TokenType::IntegerLiteral).With(Integer(std::strtoull(digit_ptr, nullptr, base)));
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
    return Token(startLoc, TokenType::StringLiteral).With(str);
}

void Lexer::append()
{
    mTemp += mChar;
    eat();
}

void Lexer::appendChar()
{
    const Location prevLoc = mLocation;

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
                    PEXPR_LOG(LogLevel::Error) << prevLoc << ": Invalid use of Unicode escape sequence" << std::endl;
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
                    PEXPR_LOG(LogLevel::Error) << prevLoc << ": Given Unicode escape sequence is invalid" << std::endl;
                    break;
                }

                if (r != length) {
                    PEXPR_LOG(LogLevel::Error) << prevLoc << ": Given Unicode escape sequence is invalid" << std::endl;
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
                        PEXPR_LOG(LogLevel::Error) << prevLoc << ": Given Unicode range" << std::endl;
                    }
                } else { // Binary
                    mTemp += (char)uni;
                }
            } else {
                PEXPR_LOG(LogLevel::Error) << prevLoc << ": Invalid length of Unicode escape sequence" << std::endl;
            }
        } break;
        default:
            PEXPR_LOG(LogLevel::Error) << prevLoc << ": Invalid escape sequence '\\" << peek() << "'" << std::endl;
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

} // namespace PExpr::internal