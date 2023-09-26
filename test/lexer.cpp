#include "internal/Lexer.h"

using namespace PExpr;
using namespace PExpr::internal;
int main(int, char**)
{
    std::stringstream stream("abc(231*22.231*2.42e-3).xyz");
    Lexer lexer(stream);
    if (lexer.next().Type != TokenType::Identifier)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::OpenParanthese)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::IntegerLiteral)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::Mul)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::NumberLiteral)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::Mul)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::NumberLiteral)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::ClosedParanthese)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::Dot)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::Identifier)
        return EXIT_FAILURE;
    if (lexer.next().Type != TokenType::Eof)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}