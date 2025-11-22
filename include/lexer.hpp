#pragma once

#include <utility>
#include <vector>
#include <boost/regex.hpp>

enum class Token {
    // tokens
    kIDENTIFIER,
    kCHAR_LITERAL,
    kSTRING_LITERAL,
    kRAW_STRING_LITERAL,
    kBYTE_LITERAL,
    kBYTE_STRING_LITERAL,
    kRAW_BYTE_STRING_LITERAL,
    kC_STRING_LITERAL,
    kRAW_C_STRING_LITERAL,
    kINTEGER_LITERAL,
    kRESERVED_TOKEN,

    // restrict keyword
    kas,
    kbreak,
    kconst,
    kcontinue,
    kcrate,
    kelse,
    kenum,
    kfalse,
    kfn,
    kfor,
    kif,
    kimpl,
    kin,
    klet,
    kloop,
    kmatch,
    kmod,
    kmove,
    kmut,
    kref,
    kreturn,
    kself,
    kSelf,
    kstatic,
    kstruct,
    ksuper,
    ktrait,
    ktrue,
    ktype,
    kunsafe,
    kuse,
    kwhere,
    kwhile,
    kdyn,

    // reserved keywords
    kabstract,
    kbecome,
    kbox,
    kdo,
    kfinal,
    kmacro,
    koverride,
    kpriv,
    ktypeof,
    kunsized,
    kvirtual,
    kyield,
    ktry,
    kgen,

    // punctuation
    kPlus,
    kMinus,
    kStar,
    kSlash,
    kPercent,
    kCaret,
    kNot,
    kAnd,
    kOr,
    kAndAnd,
    kOrOr,
    kShl,
    kShr,
    kPlusEq,
    kMinusEq,
    kStarEq,
    kSlashEq,
    kPercentEq,
    kCaretEq,
    kAndEq,
    kOrEq,
    kShlEq,
    kShrEq,
    kEq,
    kEqEq,
    kNe,
    kGt,
    kLt,
    kGe,
    kLe,
    kAt,
    kUnderscore,
    kDot,
    kDotDot,
    kDotDotDot,
    kDotDotEq,
    kComma,
    kSemi,
    kColon,
    kPathSep,
    kRArrow,
    kFatArrow,
    kLArrow,
    kPound,
    kDollar,
    kQuestion,
    kTilde,

    // delimeters
    kleftCurly,
    krightCurly,
    kleftSquare,
    kRightSquare,
    kleftParenthe,
    krightParenthe,

    kCOMMENT,

    kEnd
};

class Lexer {
private:
    std::vector<std::pair<Token, boost::regex>> letterpatterns = {
        {Token::kas, boost::regex("as")},
        {Token::kbreak, boost::regex("break")},
        {Token::kconst, boost::regex("const")},
        {Token::kcontinue, boost::regex("continue")},
        {Token::kcrate, boost::regex("crate")},
        {Token::kelse, boost::regex("else")},
        {Token::kenum, boost::regex("enum")},
        // {Token::kextern, boost::regex("extern")},
        {Token::kfalse, boost::regex("false")},
        {Token::kfn, boost::regex("fn")},
        {Token::kfor, boost::regex("for")},
        {Token::kif, boost::regex("if")},
        {Token::kimpl, boost::regex("impl")},
        {Token::kin, boost::regex("in")},
        {Token::klet, boost::regex("let")},
        {Token::kloop, boost::regex("loop")},
        {Token::kmatch, boost::regex("match")},
        {Token::kmod, boost::regex("mod")},
        {Token::kmove, boost::regex("move")},
        {Token::kmut, boost::regex("mut")},
        // {Token::kpub, boost::regex("pub")},
        {Token::kref, boost::regex("ref")},
        {Token::kreturn, boost::regex("return")},
        {Token::kself, boost::regex("self")},
        {Token::kSelf, boost::regex("Self")},
        {Token::kstatic, boost::regex("static")},
        {Token::kstruct, boost::regex("struct")},
        {Token::ksuper, boost::regex("super")},
        {Token::ktrait, boost::regex("trait")},
        {Token::ktrue, boost::regex("true")},
        {Token::ktype, boost::regex("type")},
        {Token::kunsafe, boost::regex("unsafe")},
        {Token::kuse, boost::regex("use")},
        {Token::kwhere, boost::regex("where")},
        {Token::kwhile, boost::regex("while")},
        // {Token::kasync, boost::regex("async")},
        // {Token::kawait, boost::regex("await")},
        {Token::kdyn, boost::regex("dyn")},

        {Token::kabstract, boost::regex("abstract")},
        {Token::kbecome, boost::regex("become")},
        {Token::kbox, boost::regex("box")},
        {Token::kdo, boost::regex("do")},
        {Token::kfinal, boost::regex("final")},
        {Token::kmacro, boost::regex("macro")},
        {Token::koverride, boost::regex("override")},
        {Token::kpriv, boost::regex("priv")},
        {Token::ktypeof, boost::regex("typeof")},
        {Token::kunsized, boost::regex("unsized")},
        {Token::kvirtual, boost::regex("virtual")},
        {Token::kyield, boost::regex("yield")},
        {Token::ktry, boost::regex("try")},
        {Token::kgen, boost::regex("gen")},

        {Token::kIDENTIFIER, boost::regex("[a-zA-Z][a-zA-Z0-9_]*")},
    };
    std::vector<std::pair<Token, boost::regex>> nonletterpatterns = {
        {Token::kCHAR_LITERAL, boost::regex(R"('(([^'\\\n\r\t])|(\\')|(\\")|(\\x[0-7][0-9a-fA-F])|(\\n)|(\\r)|(\\t)|(\\\\)|(\\0))'([a-zA-Z][a-zA-Z0-9_]*)?)")},
        {Token::kSTRING_LITERAL, boost::regex(R"("([^"\\\r]|\\[nrt'"\\0]|\\x[0-9a-fA-F]{2}|\\\r)*")")},
        {Token::kRAW_STRING_LITERAL, boost::regex(R"(r([#]+)([^\r])*?(\1))")},
        {Token::kC_STRING_LITERAL, boost::regex(R"(c"(([^"\\\r\0])|(\\x[0-7][0-9a-fA-F])|(\\n)|(\\r)|(\\t)|\\\\|(\\\n))*")")},
        {Token::kRAW_C_STRING_LITERAL, boost::regex(R"(cr([#]+)([^\r\0])*?(\1))")},
        {Token::kINTEGER_LITERAL, boost::regex("((0b[0-1_]*[0-1][0-1_]*)|(0o[0-7_]*[0-7][0-7_]*)|(0x[0-9a-fA-F_]*[0-9a-fA-F][0-9a-fA-F_]*)|([0-9][0-9_]*))((u32)|(i32)|(usize)|(isize))?")},
        // {Token::kFLOAT_LITERAL, boost::regex("(([0-9][0-9_]*).(?![._a-zA-Z]))|(([0-9][0-9_]*).([0-9][0-9_]*)([a-df-zA-DF-Z][a-zA-Z0-9_]*)?)")},
        // {Token::kPUNCTUATION, boost::regex()}, 
        // {Token::kRESERVED_TOKEN, boost::regex(R"delimiter((#+"(([^'\\\r\t])|(\\'\\")|(0x[0-7][0-9a-fA-F]|\n|\r|\t|\\\\|\0)|(\\\n))*"([a-zA-Z][a-zA-Z0-9_]*)?)|((0b[0-1_]*[0-1][0-1_]*[2-9])|(0o[0-7_]*[0-7][0-7_]*[8-9])|((0b[0-1_]*[0-1][0-1_]*)|(0o[0-7_]*[0-7][0-7_]*)|(0x[0-9a-fA-F_]*[0-9a-fA-F][0-9a-fA-F_]*).(?![._a-zA-Z]))|))delimiter")},

        {Token::kPlus, boost::regex(R"(\+)")},
        {Token::kMinus, boost::regex("-")},
        {Token::kStar, boost::regex(R"(\*)")},
        {Token::kSlash, boost::regex("/")},
        {Token::kPercent, boost::regex("%")},
        {Token::kCaret, boost::regex(R"(\^)")},
        {Token::kNot, boost::regex(R"(\!)")},
        {Token::kAnd, boost::regex("&")},
        {Token::kOr, boost::regex(R"(\|)")},
        {Token::kAndAnd, boost::regex("&&")},
        {Token::kOrOr, boost::regex(R"(\|\|)")},
        {Token::kShl, boost::regex("<<")},
        {Token::kShr, boost::regex(">>")},
        {Token::kPlusEq, boost::regex(R"(\+=)")},
        {Token::kMinusEq, boost::regex("-=")},
        {Token::kStarEq, boost::regex(R"(\*=)")},
        {Token::kSlashEq, boost::regex("/=")},
        {Token::kPercentEq, boost::regex("%=")},
        {Token::kCaretEq, boost::regex(R"(\^=)")},
        {Token::kAndEq, boost::regex("&=")},
        {Token::kOrEq, boost::regex(R"(\|=)")},
        {Token::kShlEq, boost::regex("<<=")},
        {Token::kShrEq, boost::regex(">>=")},
        {Token::kEq, boost::regex("=")},
        {Token::kEqEq, boost::regex("==")},
        {Token::kNe, boost::regex(R"(\!=)")},
        {Token::kGt, boost::regex(">")},
        {Token::kLt, boost::regex("<")},
        {Token::kGe, boost::regex(">=")},
        {Token::kLe, boost::regex("<=")},
        {Token::kAt, boost::regex("@")},
        {Token::kUnderscore, boost::regex("_")},
        {Token::kDot, boost::regex(R"(\.)")},
        {Token::kDotDot, boost::regex(R"(\.\.)")},
        {Token::kDotDotDot, boost::regex(R"(\.\.\.)")},
        {Token::kDotDotEq, boost::regex(R"(\.\.=)")},
        {Token::kComma, boost::regex(",")},
        {Token::kSemi, boost::regex(";")},
        {Token::kColon, boost::regex(":")},
        {Token::kPathSep, boost::regex("::")},
        {Token::kRArrow, boost::regex("->")},
        {Token::kFatArrow, boost::regex("=>")},
        {Token::kLArrow, boost::regex("<-")},
        {Token::kPound, boost::regex("#")},
        {Token::kDollar, boost::regex(R"(\$)")},
        {Token::kQuestion, boost::regex(R"(\?)")},
        {Token::kTilde, boost::regex("~")},

        {Token::kleftCurly, boost::regex(R"(\{)")},
        {Token::krightCurly, boost::regex(R"(\})")},
        {Token::kleftSquare, boost::regex(R"(\[)")},
        {Token::kRightSquare, boost::regex(R"(\])")},
        {Token::kleftParenthe, boost::regex(R"(\()")},
        {Token::krightParenthe, boost::regex(R"(\))")},

        // {Token::kCOMMENT, boost::regex(R"((//([^\n])*(\n)?)|(/\*[\s\S]*\*/))")},
    };

public:
    std::vector<std::pair<Token, std::string>> lexString(std::string); 

    std::string GetString();
};

inline std::string to_string(Token token) {
    switch (token) {
        // 标识符和字面量
        case Token::kIDENTIFIER: return "IDENTIFIER";
        case Token::kCHAR_LITERAL: return "CHAR_LITERAL";
        case Token::kSTRING_LITERAL: return "STRING_LITERAL";
        case Token::kRAW_STRING_LITERAL: return "RAW_STRING_LITERAL";
        case Token::kBYTE_LITERAL: return "BYTE_LITERAL";
        case Token::kBYTE_STRING_LITERAL: return "BYTE_STRING_LITERAL";
        case Token::kRAW_BYTE_STRING_LITERAL: return "RAW_BYTE_STRING_LITERAL";
        case Token::kC_STRING_LITERAL: return "C_STRING_LITERAL";
        case Token::kRAW_C_STRING_LITERAL: return "RAW_C_STRING_LITERAL";
        case Token::kINTEGER_LITERAL: return "INTEGER_LITERAL";
        // case Token::kFLOAT_LITERAL: return "FLOAT_LITERAL";
        case Token::kRESERVED_TOKEN: return "RESERVED_TOKEN";

        // 保留关键字
        case Token::kas: return "as";
        case Token::kbreak: return "break";
        case Token::kconst: return "const";
        case Token::kcontinue: return "continue";
        case Token::kcrate: return "crate";
        case Token::kelse: return "else";
        case Token::kenum: return "enum";
        // case Token::kextern: return "extern";
        case Token::kfalse: return "false";
        case Token::kfn: return "fn";
        case Token::kfor: return "for";
        case Token::kif: return "if";
        case Token::kimpl: return "impl";
        case Token::kin: return "in";
        case Token::klet: return "let";
        case Token::kloop: return "loop";
        case Token::kmatch: return "match";
        case Token::kmod: return "mod";
        case Token::kmove: return "move";
        case Token::kmut: return "mut";
        // case Token::kpub: return "pub";
        case Token::kref: return "ref";
        case Token::kreturn: return "return";
        case Token::kself: return "self";
        case Token::kSelf: return "Self";
        case Token::kstatic: return "static";
        case Token::kstruct: return "struct";
        case Token::ksuper: return "super";
        case Token::ktrait: return "trait";
        case Token::ktrue: return "true";
        case Token::ktype: return "type";
        case Token::kunsafe: return "unsafe";
        case Token::kuse: return "use";
        case Token::kwhere: return "where";
        case Token::kwhile: return "while";
        // case Token::kasync: return "async";
        // case Token::kawait: return "await";
        case Token::kdyn: return "dyn";

        // 弱关键字
        case Token::kabstract: return "abstract";
        case Token::kbecome: return "become";
        case Token::kbox: return "box";
        case Token::kdo: return "do";
        case Token::kfinal: return "final";
        case Token::kmacro: return "macro";
        case Token::koverride: return "override";
        case Token::kpriv: return "priv";
        case Token::ktypeof: return "typeof";
        case Token::kunsized: return "unsized";
        case Token::kvirtual: return "virtual";
        case Token::kyield: return "yield";
        case Token::ktry: return "try";
        case Token::kgen: return "gen";

        // 标点符号
        case Token::kPlus: return "Plus";
        case Token::kMinus: return "Minus";
        case Token::kStar: return "Star";
        case Token::kSlash: return "Slash";
        case Token::kPercent: return "Percent";
        case Token::kCaret: return "Caret";
        case Token::kNot: return "Not";
        case Token::kAnd: return "And";
        case Token::kOr: return "Or";
        case Token::kAndAnd: return "AndAnd";
        case Token::kOrOr: return "OrOr";
        case Token::kShl: return "Shl";
        case Token::kShr: return "Shr";
        case Token::kPlusEq: return "PlusEq";
        case Token::kMinusEq: return "MinusEq";
        case Token::kStarEq: return "StarEq";
        case Token::kSlashEq: return "SlashEq";
        case Token::kPercentEq: return "PercentEq";
        case Token::kCaretEq: return "CaretEq";
        case Token::kAndEq: return "AndEq";
        case Token::kOrEq: return "OrEq";
        case Token::kShlEq: return "ShlEq";
        case Token::kShrEq: return "ShrEq";
        case Token::kEq: return "Eq";
        case Token::kEqEq: return "EqEq";
        case Token::kNe: return "Ne";
        case Token::kGt: return "Gt";
        case Token::kLt: return "Lt";
        case Token::kGe: return "Ge";
        case Token::kLe: return "Le";
        case Token::kAt: return "At";
        case Token::kUnderscore: return "Underscore";
        case Token::kDot: return "Dot";
        case Token::kDotDot: return "DotDot";
        case Token::kDotDotDot: return "DotDotDot";
        case Token::kDotDotEq: return "DotDotEq";
        case Token::kComma: return "Comma";
        case Token::kSemi: return "Semi";
        case Token::kColon: return "Colon";
        case Token::kPathSep: return "PathSep";
        case Token::kRArrow: return "RArrow";
        case Token::kFatArrow: return "FatArrow";
        case Token::kLArrow: return "LArrow";
        case Token::kPound: return "Pound";
        case Token::kDollar: return "Dollar";
        case Token::kQuestion: return "Question";
        case Token::kTilde: return "Tilde";

        // 分隔符
        case Token::kleftCurly: return "leftCurly";
        case Token::krightCurly: return "rightCurly";
        case Token::kleftSquare: return "leftSquare";
        case Token::kRightSquare: return "RightSquare";
        case Token::kleftParenthe: return "leftParenthe";
        case Token::krightParenthe: return "rightParenthe";

        case Token::kCOMMENT: return "COMMENT";
        default: return "UNKNOWN_TOKEN"; // 处理未覆盖的情况
    }
}
