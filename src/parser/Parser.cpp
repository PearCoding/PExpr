#include "Parser.h"
#include "ast/Expression.h"
#include "ast/Pattern.h"
#include "ast/Statement.h"
#include "type/Mangler.h"

namespace PExpr::parser {
using namespace ast;
using namespace type;

Parser::Parser(Lexer& lexer, utils::Reporter& reporter)
    : mLexer(lexer)
    , mReporter(reporter)
    , mCurrentToken()
{
}

Ptr<Closure> parse_translation_unit(Parser& parser, const SymbolTable* globals);
Ptr<Closure> Parser::parse(const SymbolTable* globals)
{
    for (size_t i = 0; i < mCurrentToken.size(); ++i) {
        mCurrentToken[i] = mLexer.next();
        if (mCurrentToken[i].Type == TokenType::Error)
            return nullptr;
    }

    return parse_translation_unit(*this, globals);
}

bool Parser::expect(TokenType type)
{
    bool same = cur().Type == type;
    if (!same) {
        if (cur().Type == TokenType::Eof)
            mReporter.errorf(cur().Location, "Expected '%s' but input terminated early", Token::toString(type).data());
        else
            mReporter.errorf(cur().Location, "Expected '%s' but got '%s'", Token::toString(type).data(), Token::toString(cur().Type).data());
    }

    next();
    return same;
}

template <size_t N>
void Parser::error(const std::array<TokenType, N>& types)
{
    std::string expectation;
    for (size_t i = 0; i < types.size(); ++i) {
        expectation += Token::toString(types[i]);
        if (i != types.size() - 1)
            expectation += ", ";
    }

    if (cur().Type == TokenType::Eof)
        mReporter.errorf(cur().Location, "Expected {%s} but input terminated early", expectation.c_str());
    else
        mReporter.errorf(cur().Location, "Expected {%s} but got '%s'", expectation.c_str(), Token::toString(cur().Type).data());
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

    const auto nextToken                    = mLexer.next();
    mCurrentToken[mCurrentToken.size() - 1] = nextToken;
}

// --------------------------------------- Grammar
class ParserGrammar {
public:
    inline explicit ParserGrammar(Parser& parser, const SymbolTable* globals)
        : P(parser)
        , mGlobals(globals)
    {
    }

    Parser& P;

    inline Ptr<Closure> parse()
    {
        auto closure = p_closure();
        if (!P.hasError() && P.cur().Type != TokenType::Eof)
            P.mReporter.errorf(Location(0), "Parsing stopped before end of stream!");

        return closure;
    }

private:
    // Attribute value variant
    struct Attribute {
        std::string name;
        std::variant<bool, Integer, Number, std::string> value;

        bool isBool() const { return std::holds_alternative<bool>(value); }
        bool isInt() const { return std::holds_alternative<Integer>(value); }
        bool isNum() const { return std::holds_alternative<Number>(value); }
        bool isString() const { return std::holds_alternative<std::string>(value); }

        bool getBool() const { return std::get<bool>(value); }
        Integer getInt() const { return std::get<Integer>(value); }
        Number getNum() const { return std::get<Number>(value); }
        const std::string& getString() const { return std::get<std::string>(value); }
    };

    using AttributeList = std::vector<Attribute>;

    Closure* mCurrentClosure = nullptr;
    const SymbolTable* mGlobals;

    inline Ptr<Closure> p_closure()
    {
        Ptr<Closure> closure = std::make_shared<Closure>(P.cur().Location, mCurrentClosure);
        mCurrentClosure      = closure.get();
        if (mCurrentClosure->isTranslationUnit())
            mCurrentClosure->symbols().setParent(mGlobals); // Inject the global symbol table

        if (P.cur().Type == TokenType::Eof)
            return closure;

        while (true) {
            // Check for attributes before statements/expressions
            AttributeList attrs;
            if (P.cur(0).Type == TokenType::OpenSquareBracket && P.cur(1).Type == TokenType::OpenSquareBracket) {
                P.expect(TokenType::OpenSquareBracket);
                P.expect(TokenType::OpenSquareBracket);
                attrs = p_attributes();
                P.expect(TokenType::ClosedSquareBracket);
                P.expect(TokenType::ClosedSquareBracket);
            }

            if (P.accept(TokenType::Let)) {
                // Could be regular variable or destructuring declaration
                if (P.cur(0).Type == TokenType::Mul && P.cur(1).Type == TokenType::OpenSquareBracket) {
                    // Destructuring declaration: let *[pattern] = expr;
                    closure->addExpression(p_destructuring_statement(true, attrs));
                } else {
                    // Regular variable declaration
                    closure->addExpression(p_variable_statement(true, attrs));
                }
            } else if (P.accept(TokenType::Function)) {
                // Function
                closure->addExpression(p_function_statement(attrs));
            } else if (P.accept(TokenType::Using)) {
                // Type alias
                closure->addExpression(p_type_alias_statement(attrs));
            } else if (P.cur(0).Type == TokenType::Mul && P.cur(1).Type == TokenType::OpenSquareBracket) {
                // Destructuring assignment: *[pattern] = expr;
                closure->addExpression(p_destructuring_statement(false, attrs));
            } else if (P.cur(0).Type == TokenType::Identifier
                       && (P.cur(1).Type == TokenType::Assign || P.cur(1).Type == TokenType::PlusAssign || P.cur(1).Type == TokenType::MinusAssign || P.cur(1).Type == TokenType::MulAssign || P.cur(1).Type == TokenType::DivAssign)) {
                // Regular variable assignment
                closure->addExpression(p_variable_statement(false, attrs));
            } else if (P.cur(0).Type == TokenType::ClosedBraces && mCurrentClosure->parent()) {
                // We are not the translation unit and want to close out the expression. Do it!
                break;
            } else {
                closure->addExpression(p_expression());
                if (!P.accept(TokenType::Semicolon))
                    break;
            }
        }

        // Check for a trailing semicolon
        const auto semicolonLoc = P.cur().Location;
        if (P.accept(TokenType::Semicolon) && closure->hasFinalExpression())
            P.mReporter.warningf(utils::RT_WARNING_TRAILING_SEMICOLON, semicolonLoc, "Trailing '%s' at the end of an return expression", Token::toString(TokenType::Semicolon).data());

        mCurrentClosure = mCurrentClosure->parent();

        return closure;
    }

    // Unified variable statement (single identifier or pattern)
    inline Ptr<Expression> p_variable_statement(bool is_declaration, const AttributeList& attrs)
    {
        PEXPR_UNUSED(attrs);

        const auto loc = P.cur().Location;

        bool is_mutable = false;
        if (is_declaration)
            is_mutable = P.accept(TokenType::Mutable);

        const auto idLoc          = P.cur().Location;
        const std::string varName = P.cur().Type == TokenType::Identifier ? std::get<std::string>(P.cur().Value) : "_unknown_";
        P.expect(TokenType::Identifier);

        // Optional explicit type annotation for declarations: ': TYPE'
        Type declaredType = Type(TypeKind::Unspecified);
        if (is_declaration && P.accept(TokenType::Colon))
            declaredType = p_type();

        // Get the assignment operator (could be =, +=, -=, *=, /=)
        const TokenType assignOp = P.cur().Type;
        if (assignOp != TokenType::Assign && assignOp != TokenType::PlusAssign && assignOp != TokenType::MinusAssign && assignOp != TokenType::MulAssign && assignOp != TokenType::DivAssign) {
            P.mReporter.errorf(P.cur().Location, "Expected assignment operator");
            return std::make_shared<ErrorExpression>(loc);
        }
        P.next(); // Consume the assignment operator

        auto rightExpr = p_expression();

        P.expect(TokenType::Semicolon);

        // Create a pattern with a single simple binding
        if (is_declaration) {
            auto variableDef = std::make_shared<type::VariableDef>(varName, declaredType, is_mutable, idLoc);

            if (!mCurrentClosure->symbols().addVariable(variableDef))
                P.mReporter.errorf(idLoc, "Variable '%s' already exists in the current scope", variableDef->name().c_str());

            Pattern::ElementList elements;
            elements.push_back(PatternElement::makeSimple(idLoc, variableDef));
            auto pattern = std::make_shared<Pattern>(loc, std::move(elements));

            // For declarations, only simple assignment is allowed
            if (assignOp != TokenType::Assign) {
                P.mReporter.errorf(idLoc, "Compound assignment operators are not allowed in declarations");
                return std::make_shared<ErrorExpression>(loc);
            }

            return std::make_shared<VariableDeclarationStatement>(loc, pattern, std::move(rightExpr));
        } else {
            if (auto lkp = mCurrentClosure->symbols().lookupVariable(idLoc, varName)) {
                Pattern::ElementList elements;
                elements.push_back(PatternElement::makeSimple(idLoc, lkp));
                auto pattern = std::make_shared<Pattern>(loc, std::move(elements));

                Ptr<Expression> assignmentExpr = rightExpr;

                // Transform compound assignments to binary operations
                if (assignOp != TokenType::Assign) {
                    // Map token type to binary operation
                    BinaryOperation binOp;
                    switch (assignOp) {
                    case TokenType::PlusAssign:
                        binOp = BinaryOperation::Add;
                        break;
                    case TokenType::MinusAssign:
                        binOp = BinaryOperation::Sub;
                        break;
                    case TokenType::MulAssign:
                        binOp = BinaryOperation::Mul;
                        break;
                    case TokenType::DivAssign:
                        binOp = BinaryOperation::Div;
                        break;
                    default:
                        PEXPR_ASSERT(false, "Unhandled compound assignment operator");
                        binOp = BinaryOperation::Add;
                        break;
                    }

                    // Create variable expression for the left side
                    auto varExpr = std::make_shared<VariableExpression>(idLoc, lkp);

                    // Create binary expression: left op right
                    assignmentExpr = std::make_shared<BinaryExpression>(loc, binOp, varExpr, rightExpr);
                }

                return std::make_shared<VariableAssignmentStatement>(loc, pattern, std::move(assignmentExpr));
            } else {
                P.mReporter.errorf(idLoc, "Unknown variable '%s' in the current scope", varName.c_str());
                return std::make_shared<ErrorExpression>(loc);
            }
        }
    }

    // Destructuring statement (pattern)
    inline Ptr<Expression> p_destructuring_statement(bool is_declaration, const AttributeList& attrs)
    {
        PEXPR_UNUSED(attrs);

        const auto loc = P.cur().Location;

        // Patterns need a * before [
        P.expect(TokenType::Mul);

        // Parse pattern (allow mut/type annotations only for declarations)
        auto pattern = p_pattern(is_declaration);
        if (!pattern)
            return std::make_shared<ErrorExpression>(loc);

        // Get the assignment operator (could be =, +=, -=, *=, /=)
        const TokenType assignOp = P.cur().Type;
        if (assignOp != TokenType::Assign && assignOp != TokenType::PlusAssign && assignOp != TokenType::MinusAssign && assignOp != TokenType::MulAssign && assignOp != TokenType::DivAssign) {
            P.mReporter.errorf(P.cur().Location, "Expected assignment operator");
            return std::make_shared<ErrorExpression>(loc);
        }
        P.next(); // Consume the assignment operator

        // For destructuring, only simple assignment is allowed
        if (assignOp != TokenType::Assign) {
            P.mReporter.errorf(loc, "Compound assignment operators are not allowed with destructuring patterns");
            return std::make_shared<ErrorExpression>(loc);
        }

        auto expr = p_expression();

        P.expect(TokenType::Semicolon);

        if (is_declaration)
            return std::make_shared<VariableDeclarationStatement>(loc, pattern, std::move(expr));
        else
            return std::make_shared<VariableAssignmentStatement>(loc, pattern, std::move(expr));
    }

    inline ParameterList p_parameter_def_list()
    {
        ParameterList list;

        if (P.cur().Type == TokenType::ClosedParentheses)
            return list; // Empty parameter list
        do {
            bool isMutable              = P.accept(TokenType::Mutable);
            const std::string paramName = P.cur().Type == TokenType::Identifier ? std::get<std::string>(P.cur().Value) : "__unknown__";
            const auto loc              = P.cur().Location;
            P.expect(TokenType::Identifier);
            P.expect(TokenType::Colon);
            const auto type = p_type();
            if (type.isVoid())
                P.mReporter.errorf(loc, "Parameter '%s' can not be of type 'void'", paramName.c_str());
            list.push_back(std::make_shared<VariableDef>(paramName, type, isMutable, loc));
        } while (P.accept(TokenType::Comma));

        return list;
    }

    inline Ptr<Pattern> p_pattern(bool is_declaration)
    {
        const auto loc = P.cur().Location;
        P.expect(TokenType::OpenSquareBracket);

        Pattern::ElementList elements;
        if (P.cur().Type != TokenType::ClosedSquareBracket) {
            do {
                const auto elemLoc = P.cur().Location;

                // Check if this is a nested pattern (starts with '[') or a simple binding
                if (P.cur().Type == TokenType::OpenSquareBracket) {
                    // Nested pattern
                    auto nestedPattern = p_pattern(is_declaration);
                    if (!nestedPattern)
                        return nullptr;
                    elements.push_back(PatternElement::makeNested(elemLoc, nestedPattern));
                } else {
                    // Simple binding

                    // Check for 'mut' keyword (only allowed in declarations)
                    bool isMutable = false;
                    if (is_declaration && P.accept(TokenType::Mutable))
                        isMutable = true;

                    // Expect identifier
                    const std::string elemName = P.cur().Type == TokenType::Identifier ? std::get<std::string>(P.cur().Value) : "_unknown_";
                    P.expect(TokenType::Identifier);

                    // Optional type annotation (only allowed in declarations)
                    Type declaredType = Type(TypeKind::Unspecified);
                    if (P.accept(TokenType::Colon)) {
                        if (is_declaration) {
                            declaredType = p_type();
                            if (declaredType.isVoid())
                                P.mReporter.errorf(elemLoc, "Declared variable '%s' can not be of type 'void'", elemName.c_str());
                        } else {
                            P.mReporter.errorf(P.cur().Location, "Type annotations are not allowed in destructuring assignments");
                        }
                    }

                    if (is_declaration) {
                        auto variableDef = std::make_shared<type::VariableDef>(elemName, declaredType, isMutable, elemLoc);

                        if (!mCurrentClosure->symbols().addVariable(variableDef))
                            P.mReporter.errorf(elemLoc, "Variable '%s' already exists in the current scope", variableDef->name().c_str());

                        elements.push_back(PatternElement::makeSimple(elemLoc, variableDef));
                    } else {
                        if (auto lkp = mCurrentClosure->symbols().lookupVariable(elemLoc, elemName))
                            elements.push_back(PatternElement::makeSimple(elemLoc, lkp));
                        else
                            P.mReporter.errorf(elemLoc, "Unknown variable '%s' in the current scope", elemName.c_str());
                    }
                }
            } while (P.accept(TokenType::Comma));
        }

        P.expect(TokenType::ClosedSquareBracket);

        if (elements.empty()) {
            P.mReporter.errorf(loc, "Pattern must have at least one element");
            return nullptr;
        }

        return std::make_shared<Pattern>(loc, std::move(elements));
    }

    inline AttributeList p_attributes()
    {
        AttributeList attributes;

        if (P.cur().Type == TokenType::ClosedSquareBracket)
            return attributes;

        do {
            if (P.cur().Type != TokenType::Identifier)
                break;

            const std::string attrName = std::get<std::string>(P.cur().Value);
            P.expect(TokenType::Identifier);

            Attribute attr;
            attr.name = attrName;

            // Check for value assignment
            if (P.accept(TokenType::Assign)) {
                // Parse value based on token type
                if (P.cur().Type == TokenType::BooleanLiteral) {
                    attr.value = std::get<bool>(P.cur().Value);
                    P.expect(TokenType::BooleanLiteral);
                } else if (P.cur().Type == TokenType::IntegerLiteral) {
                    attr.value = std::get<Integer>(P.cur().Value);
                    P.expect(TokenType::IntegerLiteral);
                } else if (P.cur().Type == TokenType::NumberLiteral) {
                    attr.value = std::get<Number>(P.cur().Value);
                    P.expect(TokenType::NumberLiteral);
                } else if (P.cur().Type == TokenType::StringLiteral) {
                    attr.value = std::get<std::string>(P.cur().Value);
                    P.expect(TokenType::StringLiteral);
                } else {
                    P.mReporter.errorf(P.cur().Location, "Expected boolean, integer, number, or string literal for attribute value");
                }
            } else {
                // Boolean attribute without value defaults to true
                attr.value = true;
            }

            attributes.push_back(attr);
        } while (P.accept(TokenType::Comma));

        return attributes;
    }

    struct FunctionAttributes {
        bool Extern        = false;
        bool HasSideEffect = false;

        static FunctionAttributes fromAttributes(const AttributeList& attrs)
        {
            FunctionAttributes result;
            bool hadPure = false;
            for (const auto& attr : attrs) {
                if (attr.name == "extern") {
                    result.Extern = attr.isBool() ? attr.getBool() : true;
                    if (!hadPure)
                        result.HasSideEffect = true; // Default we assume external functions have side effects
                } else if (attr.name == "pure") {
                    // pure attribute means no side effect
                    bool pure            = attr.isBool() ? attr.getBool() : true;
                    result.HasSideEffect = !pure;
                    hadPure              = true;
                }
            }
            return result;
        }
    };

    inline Ptr<Expression> p_function_statement(const AttributeList& attrs)
    {
        const auto loc = P.cur().Location;

        // Get attribute list
        FunctionAttributes attr = FunctionAttributes::fromAttributes(attrs);

        // Get function name
        std::string funcName;
        if (const auto fnptr = std::get_if<std::string>(&P.cur().Value))
            funcName = *fnptr;
        else
            funcName = "_error_"; // Should fail in the next line

        P.expect(TokenType::Identifier);
        P.expect(TokenType::OpenParentheses);

        const auto parameters = p_parameter_def_list();

        P.expect(TokenType::ClosedParentheses);

        Type returnType = Type(TypeKind::Unspecified);
        if (P.accept(TokenType::ArrowRight))
            returnType = p_type();

        const std::string mangled = makeMangledNameFromTypes(funcName, parameters, mCurrentClosure);

        if (!attr.Extern) {
            auto closure    = std::make_shared<Closure>(loc, mCurrentClosure);
            mCurrentClosure = closure.get();

            // Add parameters to the symbol table
            for (const auto& p : parameters) {
                if (!mCurrentClosure->symbols().addVariable(p))
                    P.mReporter.errorf(p->location(), "Parameter '%s' with the same name already exists", p->name().c_str());
            }

            P.expect(TokenType::Assign);
            Ptr<Expression> expr = p_expression();
            closure->addExpression(expr);
            P.expect(TokenType::Semicolon);

            mCurrentClosure = closure->parent();

            if (expr->type() == ExpressionType::Closure) {
                // Try to remove the previous closure to directly use this one.
                auto innerClosure = std::reinterpret_pointer_cast<ClosureExpression>(expr)->closure();

                bool shadowedParameters = false;
                for (const auto& p : parameters) {
                    const SymbolTable* tbl = nullptr;
                    if (const auto def = innerClosure->symbols().lookupVariable(p->location(), p->name(), &tbl); def && tbl == &innerClosure->symbols()) {
                        P.mReporter.warningf(utils::RT_WARNING_SHADOWED_PARAMETER, p->location(), "Parameter '%s' is shadowed by the ", p->name().c_str());
                        shadowedParameters = true;
                    }
                }

                if (!shadowedParameters) {
                    // Remove the previous closure to directly use this one.
                    closure = innerClosure;
                    closure->setParent(mCurrentClosure);

                    // Readd the parameters for the later passes as the previous one got removed
                    for (const auto& p : parameters) {
                        if (!closure->symbols().addVariable(p))
                            P.mReporter.errorf(p->location(), "Parameter '%s' with the same name already exists", p->name().c_str()); //< This should never happen, but better be safe
                    }
                }
            }

            return std::make_shared<FunctionDeclarationStatement>(loc, funcName, parameters, closure, returnType, mangled, false);
        } else {
            if (returnType.kind() == TypeKind::Unspecified)
                P.mReporter.errorf(P.cur().Location, "Expected an explicit return type for the given function");
            P.expect(TokenType::Semicolon);
            return std::make_shared<FunctionDeclarationStatement>(loc, funcName, parameters, nullptr, returnType, mangled, attr.HasSideEffect);
        }
    }

    // Expressions
    inline Ptr<Expression> p_expression()
    {
        return p_binary_expression();
    }

    static inline std::pair<BinaryOperation, int> binaryOpFromToken(TokenType type)
    {
        switch (type) {
        case TokenType::Or:
            return { BinaryOperation::Or, 6 };
        case TokenType::And:
            return { BinaryOperation::And, 5 };
        case TokenType::Equal:
            return { BinaryOperation::Equal, 4 };
        case TokenType::NotEqual:
            return { BinaryOperation::NotEqual, 4 };
        case TokenType::Less:
            return { BinaryOperation::Less, 4 };
        case TokenType::Greater:
            return { BinaryOperation::Greater, 4 };
        case TokenType::LessEqual:
            return { BinaryOperation::LessEqual, 4 };
        case TokenType::GreaterEqual:
            return { BinaryOperation::GreaterEqual, 4 };
        case TokenType::Plus:
            return { BinaryOperation::Add, 3 };
        case TokenType::Minus:
            return { BinaryOperation::Sub, 3 };
        case TokenType::Mul:
            return { BinaryOperation::Mul, 2 };
        case TokenType::Div:
            return { BinaryOperation::Div, 2 };
        case TokenType::Mod:
            return { BinaryOperation::Mod, 2 };
        case TokenType::Pow:
            return { BinaryOperation::Pow, 1 };
        default:
            // Other tokens, fallthrough
            return { BinaryOperation::Add, -1 };
        }
    }

    inline Ptr<Expression> p_binary_expression(int max_precedence = 6 /*Max precedence*/)
    {
        auto left = p_unary_expression();
        while (true) {
            const auto loc = P.cur().Location;
            const auto bin = binaryOpFromToken(P.cur().Type);
            const auto op  = std::get<0>(bin);
            const int prec = std::get<1>(bin);

            if (prec > max_precedence || prec <= 0)
                break;
            P.next();

            auto right = p_binary_expression(prec - 1);
            left       = std::make_shared<BinaryExpression>(loc, op, left, right);
        }

        return left;
    }

    inline Ptr<Expression> p_unary_expression()
    {
        const auto loc = P.cur().Location;
        if (P.accept(TokenType::Plus))
            return std::make_shared<UnaryExpression>(loc, UnaryOperation::Pos, p_unary_expression());
        if (P.accept(TokenType::Minus))
            return std::make_shared<UnaryExpression>(loc, UnaryOperation::Neg, p_unary_expression());
        if (P.accept(TokenType::ExclamationMark))
            return std::make_shared<UnaryExpression>(loc, UnaryOperation::Not, p_unary_expression());

        return p_postfix_expression();
    }

    inline Ptr<Expression> p_postfix_expression()
    {
        auto expr = p_call_expression();

        // Loop to handle chained postfix operators
        while (true) {
            if (P.cur().Type == TokenType::Dot) {
                const auto loc = P.cur().Location;
                auto swizzle   = p_swizzle();

                if (!checkSwizzle(swizzle)) {
                    P.mReporter.errorf(loc, "Given access '%s' is invalid", std::string(swizzle).c_str());
                    expr = std::make_shared<ErrorExpression>(loc);
                } else {
                    expr = std::make_shared<SwizzleExpression>(loc, expr, swizzle);
                }
                continue;
            }

            // explicit cast syntax: "<expr> as <type>"
            if (P.accept(TokenType::As)) {
                // Use the expression's original location for the cast node
                const auto loc    = expr->location();
                const auto toType = p_type();
                if (toType.isVoid())
                    P.mReporter.errorf(loc, "Can not cast to 'void'");
                expr = std::make_shared<CastExpression>(loc, toType, expr);
                continue;
            }

            // [i]
            if (P.accept(TokenType::OpenSquareBracket)) {
                size_t index     = 0;
                const auto token = P.cur();
                if (P.accept(TokenType::IntegerLiteral)) {
                    const Integer i = std::get<Integer>(token.Value);
                    if (i < 0)
                        P.mReporter.errorf(token.Location, "Negative index given for vector lookup");
                    else
                        index = (size_t)i;
                    P.expect(TokenType::ClosedSquareBracket);

                    expr = std::make_shared<AccessExpression>(token.Location, expr, index);
                } else {
                    expr = std::make_shared<ErrorExpression>(token.Location);
                }
                continue;
            }

            // No more postfix operators
            break;
        }

        return expr;
    }

    inline Ptr<Expression> p_call_expression()
    {
        if (P.cur(0).Type == TokenType::Identifier
            && P.cur(1).Type == TokenType::OpenParentheses) {
            const auto loc             = P.cur().Location;
            const std::string funcName = std::get<std::string>(P.cur().Value);

            P.expect(TokenType::Identifier);
            P.expect(TokenType::OpenParentheses);

            std::vector<Ptr<Expression>> parameters;

            if (!P.accept(TokenType::ClosedParentheses)) {
                p_parameter_list(parameters);
                P.expect(TokenType::ClosedParentheses);
            }

            return std::make_shared<CallExpression>(loc, funcName, std::move(parameters));
        }

        return p_enclosed_expression();
    }

    inline void p_parameter_list(std::vector<Ptr<Expression>>& list)
    {
        do {
            auto expr = p_expression();
            PEXPR_ASSERT(expr != nullptr, "Got empty parameter value");
            list.push_back(expr);
        } while (P.accept(TokenType::Comma));
    }

    inline Ptr<Expression> p_if_expression()
    {
        const auto loc = P.cur().Location;
        BranchExpression::ClosureList branches;

        P.expect(TokenType::If);
        const auto firstCondition = p_expression();
        P.expect(TokenType::OpenBraces);
        const auto firstClosure = p_closure();
        P.expect(TokenType::ClosedBraces);

        branches.push_back(BranchExpression::SingleBranch{ firstCondition, firstClosure });

        while (P.cur().Type == TokenType::Elif) {
            P.expect(TokenType::Elif);
            const auto condition = p_expression();
            P.expect(TokenType::OpenBraces);
            const auto closure = p_closure();
            P.expect(TokenType::ClosedBraces);

            branches.push_back(BranchExpression::SingleBranch{ condition, closure });
        }

        Ptr<Closure> elseClosure;
        if (P.accept(TokenType::Else)) {
            P.expect(TokenType::OpenBraces);
            elseClosure = p_closure();
            P.expect(TokenType::ClosedBraces);
        }

        // Check if we even have a correct if expression
        if (branches.empty())
            return std::make_shared<ErrorExpression>(loc);

        return std::make_shared<BranchExpression>(loc, branches, elseClosure);
    }

    inline Ptr<Expression> p_enclosed_expression()
    {
        // if ... { ... } else { ... }
        if (P.cur(0).Type == TokenType::If) {
            return p_if_expression();
        }

        // { ... }
        if (P.accept(TokenType::OpenBraces)) {
            auto closure = p_closure();
            P.expect(TokenType::ClosedBraces);
            return std::make_shared<ClosureExpression>(closure->location(), closure);
        }

        // ( ... )
        if (P.accept(TokenType::OpenParentheses)) {
            auto expr = p_expression();
            P.expect(TokenType::ClosedParentheses);
            return expr;
        }

        // [ ... ]
        if (P.accept(TokenType::OpenSquareBracket)) {
            auto expr = p_tuple_expression();
            P.expect(TokenType::ClosedSquareBracket);
            return expr;
        }

        return p_primary_expression();
    }

    inline Ptr<Expression> p_tuple_expression()
    {
        const auto loc = P.cur().Location;
        std::vector<Ptr<Expression>> vector;
        do {
            auto expr = p_expression();
            PEXPR_ASSERT(expr != nullptr, "Got empty parameter value");
            vector.push_back(expr);
        } while (P.accept(TokenType::Comma));

        if (vector.size() == 0)
            P.mReporter.errorf(loc, "Invalid empty tuple given");

        return std::make_shared<TupleExpression>(loc, std::move(vector));
    }

    inline Ptr<Expression> p_primary_expression()
    {
        const auto value = P.cur();
        if (P.accept(TokenType::BooleanLiteral))
            return std::make_shared<LiteralExpression>(value.Location, Type(TypeKind::Boolean), value.Value);

        if (P.accept(TokenType::NumberLiteral))
            return std::make_shared<LiteralExpression>(value.Location, Type(TypeKind::Number), value.Value);

        if (P.accept(TokenType::IntegerLiteral))
            return std::make_shared<LiteralExpression>(value.Location, Type(TypeKind::Integer), value.Value);

        if (P.accept(TokenType::StringLiteral))
            return std::make_shared<LiteralExpression>(value.Location, Type(TypeKind::String), value.Value);

        if (P.accept(TokenType::Identifier)) {
            const auto varName = std::get<std::string>(value.Value);
            if (const auto variable = mCurrentClosure->symbols().lookupVariable(value.Location, varName)) {
                return std::make_shared<VariableExpression>(value.Location, variable);
            } else {
                P.mReporter.errorf(value.Location, "Unknown variable '%s' in the current scope", varName.c_str());
                return std::make_shared<ErrorExpression>(value.Location);
            }
        }

        // Only print error if error was not introduced by lexer
        if (P.cur().Type != TokenType::Error)
            P.error(std::array<TokenType, 8>{ TokenType::OpenParentheses, TokenType::OpenBraces, TokenType::If, TokenType::BooleanLiteral, TokenType::NumberLiteral, TokenType::IntegerLiteral, TokenType::StringLiteral, TokenType::Identifier });
        return std::make_shared<ErrorExpression>(value.Location);
    }

    inline bool checkSwizzle(std::string_view swizzle)
    {
        if (swizzle.size() < 1 || swizzle.size() > 4)
            return false;
        for (auto c : swizzle) {
            if (c != 'x' && c != 'y' && c != 'z' && c != 'w' && c != 'r' && c != 'g' && c != 'b' && c != 'a')
                return false;
        }
        return true;
    }

    inline std::string p_swizzle()
    {
        P.expect(TokenType::Dot);
        auto token = P.cur();
        if (P.expect(TokenType::Identifier))
            return std::get<std::string>(token.Value);
        else
            return {};
    }

    inline Ptr<Expression> p_type_alias_statement(const AttributeList& attrs)
    {
        PEXPR_UNUSED(attrs);

        const auto loc              = P.cur().Location;
        const std::string aliasName = P.cur().Type == TokenType::Identifier ? std::get<std::string>(P.cur().Value) : "_unknown_";
        P.expect(TokenType::Identifier);
        P.expect(TokenType::Assign);
        const auto aliasedType = p_type();
        P.expect(TokenType::Semicolon);

        // Register the type alias in the current symbol table
        if (mCurrentClosure) {
            if (!mCurrentClosure->symbols().addTypeAlias(aliasName, aliasedType))
                P.mReporter.errorf(loc, "Type alias '%s' already defined in the current scope", aliasName.c_str());
        }

        return std::make_shared<TypeAliasStatement>(loc, aliasName, aliasedType);
    }

    inline Type p_type()
    {
        // First check if it's an identifier (could be a type alias)
        if (P.cur().Type == TokenType::Identifier) {
            const std::string name = std::get<std::string>(P.cur().Value);
            P.next();

            // Try to resolve as a type alias from the current symbol table
            if (mCurrentClosure) {
                if (const auto alias = mCurrentClosure->symbols().lookupTypeAlias(name); alias.has_value())
                    return *alias;
            }

            // Not a known type alias
            P.mReporter.errorf(P.cur().Location, "Unknown type name '%s'", name.c_str());
            return Type::Error();
        } else if (P.cur().Type == TokenType::OpenSquareBracket) {
            // Tuple type: [T1, T2, ...]
            P.expect(TokenType::OpenSquareBracket);
            std::vector<Type> components;
            if (P.cur().Type != TokenType::ClosedSquareBracket) {
                do {
                    const auto loc = P.cur().Location;
                    auto type      = p_type();
                    if (type.isVoid())
                        P.mReporter.errorf(loc, "Have type 'void' inside tuples");
                    components.push_back(std::move(type));
                } while (P.accept(TokenType::Comma));
            }
            P.expect(TokenType::ClosedSquareBracket);
            if (components.empty()) {
                P.mReporter.errorf(P.cur().Location, "Tuple type must have at least one component");
                return Type::Error();
            }
            return Type(std::move(components));
        } else {
            P.error(std::to_array<TokenType>({ TokenType::Identifier, TokenType::OpenSquareBracket }));
            return Type::Error();
        }
    }
};

Ptr<Closure> parse_translation_unit(Parser& parser, const SymbolTable* globals)
{
    return ParserGrammar(parser, globals).parse();
}
} // namespace PExpr::parser