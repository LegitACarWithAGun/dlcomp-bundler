#include "Luau/Ast.h"
#include "Luau/Parser.h"
#include "iostream"
#include "sstream"
#include "utils.cpp"
#include <cassert>
#include <format>
#include <numeric>
#include <ranges>
#include <vector>

// TODO: generate a source map
// TODO: probably missing a bunch of nodes, go over the list

using namespace Luau;

typedef AstExprBinary::Op BinaryOperator;
static std::unordered_map<BinaryOperator, std::string> binaryOperatorLookup = {
    {BinaryOperator::Add, "+"},        {BinaryOperator::Sub, "-"},        {BinaryOperator::Mul, "*"},       {BinaryOperator::Div, "/"},
    {BinaryOperator::FloorDiv, "//"},  {BinaryOperator::Mod, "%"},        {BinaryOperator::Pow, "^"},       {BinaryOperator::Concat, ".."},
    {BinaryOperator::CompareNe, "~="}, {BinaryOperator::CompareEq, "=="}, {BinaryOperator::CompareLt, "<"}, {BinaryOperator::CompareLe, "<="},
    {BinaryOperator::CompareGt, ">"},  {BinaryOperator::CompareGe, ">="}, {BinaryOperator::And, " and "},   {BinaryOperator::Or, " or "},
};

typedef AstExprUnary::Op UnaryOperator;
static std::unordered_map<UnaryOperator, std::string> unaryOperatorLookup = {
    {UnaryOperator::Not, "not "},
    {UnaryOperator::Minus, "-"},
    {UnaryOperator::Len, "#"},
};

typedef AstExprConstantString::QuoteStyle QuoteStyle;
static std::unordered_map<QuoteStyle, std::string> quoteStyleLookup = {
    {QuoteStyle::QuotedSimple, "\"{}\""},
    {QuoteStyle::QuotedSingle, "'{}'"},
    {QuoteStyle::QuotedRaw, "[[{}]]"},
    {QuoteStyle::Unquoted, "{}"},
};

static std::unordered_map<char, std::string> luaEscapeSequences = {
    {'"', "\\\""}, {'\\', "\\\\"}, {'\n', "\\n"}, {'\b', "\\b"}, {'\f', "\\f"}, {'\r', "\\r"}, {'\t', "\\t"},
};

std::string getAstLocalName(const AstLocal* local) {
    return local->name.value;
}

auto getAstLocalArrayNames(const AstArray<AstLocal*>& args) {
    std::vector<std::string> output;
    for (auto arg : args) {
        output.push_back(getAstLocalName(arg));
    }
    return output;
}

std::string implode(const std::vector<std::string>& list, const std::string& seperator) {
    if (list.size() == 0) {
        return "";
    }
    std::string output;
    for (auto itr = list.begin(); itr < list.end(); itr++) {
        if (itr != list.begin()) {
            output += seperator;
        }
        output += *itr;
    }
    return output;
}

auto AstArrayCharToString(const AstArray<char>& value) {
    return std::string{value.data, value.size};
}

// always escapes everything that can be escaped, regardless of whether or not it was in the original
// slightly inefficent, not that big of a deal
// ie single quotes in a double quoted string
auto EscapeLuaString(const std::string& str) {
    std::string output;
    for (char c : str) {
        if (luaEscapeSequences.contains(c)) {
            output += luaEscapeSequences[c];
            continue;
        }
        output += c;
    }
    return output;
}

class MinifyVisitor : public AstVisitor {

  private:
    // functions, loops, and DO blocks all keep their body in a AstStatBlock
    // we only care about recreating the DO block
    bool ignoreNextBlock = true;
    bool funcIsAnyonymous = true;
    const std::string& scriptContents;

  public:
    // TODO: smarter writing to minimize output size
    std::string output;

    MinifyVisitor(const std::string& scriptContents) : scriptContents(scriptContents) {};

    void debugPrintLocation(class AstNode* node) {
        if (true)
            return;
        std::cout << "debug @ " << fmtLocation(node->location) << " (" << node->classIndex << "): " << typeid(*node).name() << std::endl;
    }

    // FIXME: unclear name
    void appendAstExprArray(const AstArray<AstExpr*>& list) {
        for (auto itr = list.begin(); itr < list.end(); itr++) {
            if (itr != list.begin()) {
                output += ",";
            }
            auto expression = *itr;
            debugPrintLocation(expression);
            expression->visit(this);
        }
    }

    // stack block
    virtual bool visit(class AstStatBlock* node) {
        debugPrintLocation(node);

        auto isDoBlock = !ignoreNextBlock;
        ignoreNextBlock = false;
        if (isDoBlock) {
            output += "do ";
        }

        for (auto children : node->body) {
            children->visit(this);
        }

        if (isDoBlock) {
            output += " end; ";
        }

        return false;
    }

    // assign
    // create a local variable
    virtual bool visit(class AstStatLocal* node) {
        debugPrintLocation(node);

        output += "local ";
        // lvalue
        auto lvalueNames = getAstLocalArrayNames(node->vars);
        output += implode(lvalueNames, ",");

        // rvalue
        if (node->values.size > 0) {
            output += "=";
            appendAstExprArray(node->values);
        }

        // end line
        output += ";";
        return false;
    }
    // assign to a previously created variable
    virtual bool visit(class AstStatAssign* node) {
        debugPrintLocation(node);

        // lvalue
        appendAstExprArray(node->vars);
        // eq
        output += "=";
        // rvalue
        appendAstExprArray(node->values);

        // end line
        output += ";";
        return false;
    }
    // compount assign; a+=b, a%=b
    virtual bool visit(class AstStatCompoundAssign* node) {
        BinaryOperator op = node->op;
        assert(binaryOperatorLookup.contains(op));

        node->var->visit(this);
        output += binaryOperatorLookup[op] + "=";
        node->value->visit(this);
        output += ";";
        return false;
    }

    // function header
    virtual bool visit(class AstStatLocalFunction* node) {
        output += "local function ";
        output += getAstLocalName(node->name);

        funcIsAnyonymous = false;
        node->func->visit(this);

        output += " ";
        return false;
    }
    virtual bool visit(class AstStatFunction* node) {
        output += "function ";
        node->name->visit(this);

        funcIsAnyonymous = false;
        node->func->visit(this);

        output += " ";
        return false;
    }
    // function body
    virtual bool visit(class AstExprFunction* node) {
        // workaround for functions without a name
        if (funcIsAnyonymous) {
            output += "function";
        }
        funcIsAnyonymous = true;

        // args
        output += "(";
        auto argNames = getAstLocalArrayNames(node->args);
        if (node->vararg) {
            argNames.push_back("...");
        }
        output += implode(argNames, ", ");
        output += ")";

        ignoreNextBlock = true;
        node->body->visit(this);
        ignoreNextBlock = false;

        // TODO: I would like this space to not be here in certain cases
        output += "end";
        return false;
    }
    // function call `(args)`
    virtual bool visit(class AstExprCall* node) {
        node->func->visit(this);
        output += "(";
        this->appendAstExprArray(node->args);

        output += ")";
        return false;
    }
    // return
    virtual bool visit(class AstStatReturn* node) {
        output += "return ";
        appendAstExprArray(node->list);
        output += " ";
        return false;
    }
    // seemingly just wraps AstExprCall when its at the start of the line?
    virtual bool visit(class AstStatExpr* node) {
        node->expr->visit(this);
        output += ";";
        return false;
    }

    // control flow / conditionals
    // if statment
    virtual bool visit(class AstStatIf* node) {
        output += "if ";
        node->condition->visit(this);
        output += " then ";

        // then body
        ignoreNextBlock = true;
        node->thenbody->visit(this);
        ignoreNextBlock = false;

        bool hasElseIf = false;
        if (node->elsebody) {
            hasElseIf = node->elsebody->is<AstStatIf>();

            output += " else";
            if (!hasElseIf) {
                output += " ";
            }

            node->elsebody->visit(this);
        }

        if (!hasElseIf) {
            output += " end;";
        }
        return false;
    }
    // basic for loop
    virtual bool visit(class AstStatFor* node) {
        output += "for ";
        // iterator
        output += getAstLocalName(node->var);
        output += "=";
        node->from->visit(this);
        // to
        output += ",";
        node->to->visit(this);
        // step
        if (node->step) {
            output += ",";
            node->step->visit(this);
        }
        output += " do ";

        // body
        ignoreNextBlock = true;
        node->body->visit(this);
        ignoreNextBlock = false;

        output += " end;";
        return false;
    }
    // for-in
    virtual bool visit(class AstStatForIn* node) {
        output += "for ";
        auto vars = getAstLocalArrayNames(node->vars);
        output += implode(vars, ",");

        output += " in ";
        appendAstExprArray(node->values);
        output += " do ";

        ignoreNextBlock = true;
        node->body->visit(this);
        ignoreNextBlock = false;

        output += " end;";
        return false;
    }
    // while
    virtual bool visit(class AstStatWhile* node) {
        output += "while ";
        node->condition->visit(this);
        output += " do ";

        ignoreNextBlock = true;
        node->body->visit(this);
        ignoreNextBlock = false;

        output += " end;";
        return false;
    }
    // repeat
    virtual bool visit(class AstStatRepeat* node) {
        output += "repeat ";

        ignoreNextBlock = true;
        node->body->visit(this);
        ignoreNextBlock = false;

        output += " until ";
        node->condition->visit(this);

        output += ";";
        return false;
    }
    // continue
    virtual bool visit(class AstStatContinue* node) {
        output += "continue;";
        return false;
    }
    // break
    virtual bool visit(class AstStatBreak* node) {
        output += "break;";
        return false;
    }

    // variable names
    virtual bool visit(class AstExprGlobal* node) {
        output += node->name.value;
        return false;
    }
    virtual bool visit(class AstExprLocal* node) {
        output += getAstLocalName(node->local);
        return false;
    }
    virtual bool visit(class AstExprIndexName* node) {
        node->expr->visit(this);
        output += node->op;
        output += node->index.value;
        return false;
    }
    virtual bool visit(class AstExprIndexExpr* node) {
        node->expr->visit(this);
        output += "[";
        node->index->visit(this);
        output += "]";
        return false;
    }

    // operators
    // regular operators; a+b, a or b
    virtual bool visit(class AstExprBinary* node) {
        BinaryOperator op = node->op;
        assert(binaryOperatorLookup.contains(op));

        node->left->visit(this);
        output += binaryOperatorLookup[op];
        node->right->visit(this);
        return false;
    }
    // prefix operators; not, #, and -
    virtual bool visit(class AstExprUnary* node) {
        UnaryOperator op = node->op;
        assert(unaryOperatorLookup.contains(op));

        output += unaryOperatorLookup[op];
        node->expr->visit(this);
        return false;
    }

    // group; ex: `(yay and yipee)`
    virtual bool visit(class AstExprGroup* node) {
        output += "(";
        node->expr->visit(this);
        output += ")";
        return false;
    }

    // constants
    virtual bool visit(class AstExprConstantNil* node) {
        output += "nil";
        return false;
    }
    virtual bool visit(class AstExprConstantBool* node) {
        output += node->value ? "true" : "false";
        return false;
    }
    virtual bool visit(class AstExprConstantNumber* node) {
        output += std::format("{}", node->value);
        return false;
    }
    virtual bool visit(class AstExprConstantString* node) {
        // TODO: raw quotes are not all the same (ie: [=[]=])
        // probably just have to read the original file at that location (expensive..)
        // OR search the string for substrings that could close raw strings, picking the smallest that doesnt show up
        // TODO: raw quotes might have newlines embedded, figure out what to do in that case
        auto quoteStyle = node->quoteStyle;
        assert(quoteStyleLookup.contains(quoteStyle));
        auto quoteTemplate = quoteStyleLookup[quoteStyle];

        auto str = AstArrayCharToString(node->value);
        if (quoteStyle != QuoteStyle::QuotedRaw) {
            str = EscapeLuaString(str);
        }
        output += std::vformat(quoteTemplate, std::make_format_args(str));

        return false;
    }
    virtual bool visit(class AstExprVarargs* node) {
        output += "...";
        return false;
    }

    // table definiton
    virtual bool visit(class AstExprTable* node) {
        output += "{";
        typedef AstExprTable::Item::Kind Kind;
        for (auto item : node->items) {
            switch (item.kind) {
            case Kind::List:
                break;
            case Kind::Record:
                item.key->visit(this);
                output += "=";
                break;
            case Kind::General:
                output += "[";
                item.key->visit(this);
                output += "]";
                output += "=";
                break;
            default:
                // TODO: proper error msg
                assert(false);
                break;
            }
            item.value->visit(this);
            // TODO: dont write after last item
            output += ",";
        }
        output += "}";
        return false;
    }

    // interpolated string
    virtual bool visit(class AstExprInterpString* node) {
        output += "`";
        auto strings = node->strings.data;
        auto expressions = node->expressions.data;
        auto expressionCount = node->expressions.size;
        for (auto i = 0; i < expressionCount; i++) {
            auto str = AstArrayCharToString(strings[i]);
            output += EscapeLuaString(str);

            output += "{";
            expressions[i]->visit(this);
            output += "}";
        }
        // theres always exactly one more string than expressions, see the comment in Ast.h -> AstExprInterpString
        auto str = AstArrayCharToString(strings[expressionCount]);
        output += EscapeLuaString(str);
        output += "`";
        return false;
    }

    // ignore
    virtual bool visit(class Luau::AstExprTypeAssertion* node) {
        node->expr->visit(this);
        return false;
    }
    virtual bool visit(class Luau::AstStatTypeAlias* node) {
        return false;
    }

    // default (no handler found)
    // just report to the dev
    virtual bool visit(class Luau::AstNode* node) {
        std::cout << "unhandled node @ " << fmtLocation(node->location) << " (" << node->classIndex << "): " << typeid(*node).name() << std::endl;
        return false;
    }
};

std::string minify(std::string scriptContents) {
    Luau::Allocator luauAllocator;
    Luau::AstNameTable nameTable(luauAllocator);
    Luau::ParseOptions parseOptions;

    Luau::ParseResult parseResult = Luau::Parser::parse(scriptContents.data(), scriptContents.size(), nameTable, luauAllocator, parseOptions);
    if (parseResult.errors.size() > 0) {
        for (auto err : parseResult.errors) {
            std::cerr << "parse error:" << std::endl;
            std::cerr << "	- " << fmtLocation(err.getLocation()) << std::endl;
            std::cerr << "	- " << err.what() << std::endl;
            std::cerr << "	- " << err.getMessage() << std::endl;
        }
        throw std::runtime_error("parse errors");
    }

    MinifyVisitor visitor{scriptContents};
    parseResult.root->visit(&visitor);
    return visitor.output;
}
