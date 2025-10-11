#pragma once

#include <regex>
#include <utility>
#include <vector>

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
    std::vector<std::pair<Token, std::regex>> patterns = {
        {Token::kas, std::regex("as")},
        {Token::kbreak, std::regex("break")},
        {Token::kconst, std::regex("const")},
        {Token::kcontinue, std::regex("continue")},
        {Token::kcrate, std::regex("crate")},
        {Token::kelse, std::regex("else")},
        {Token::kenum, std::regex("enum")},
        // {Token::kextern, std::regex("extern")},
        {Token::kfalse, std::regex("false")},
        {Token::kfn, std::regex("fn")},
        {Token::kfor, std::regex("for")},
        {Token::kif, std::regex("if")},
        {Token::kimpl, std::regex("impl")},
        {Token::kin, std::regex("in")},
        {Token::klet, std::regex("let")},
        {Token::kloop, std::regex("loop")},
        {Token::kmatch, std::regex("match")},
        {Token::kmod, std::regex("mod")},
        {Token::kmove, std::regex("move")},
        {Token::kmut, std::regex("mut")},
        // {Token::kpub, std::regex("pub")},
        {Token::kref, std::regex("ref")},
        {Token::kreturn, std::regex("return")},
        {Token::kself, std::regex("self")},
        {Token::kSelf, std::regex("Self")},
        {Token::kstatic, std::regex("static")},
        {Token::kstruct, std::regex("struct")},
        {Token::ksuper, std::regex("super")},
        {Token::ktrait, std::regex("trait")},
        {Token::ktrue, std::regex("true")},
        {Token::ktype, std::regex("type")},
        {Token::kunsafe, std::regex("unsafe")},
        {Token::kuse, std::regex("use")},
        {Token::kwhere, std::regex("where")},
        {Token::kwhile, std::regex("while")},
        // {Token::kasync, std::regex("async")},
        // {Token::kawait, std::regex("await")},
        {Token::kdyn, std::regex("dyn")},

        {Token::kabstract, std::regex("abstract")},
        {Token::kbecome, std::regex("become")},
        {Token::kbox, std::regex("box")},
        {Token::kdo, std::regex("do")},
        {Token::kfinal, std::regex("final")},
        {Token::kmacro, std::regex("macro")},
        {Token::koverride, std::regex("override")},
        {Token::kpriv, std::regex("priv")},
        {Token::ktypeof, std::regex("typeof")},
        {Token::kunsized, std::regex("unsized")},
        {Token::kvirtual, std::regex("virtual")},
        {Token::kyield, std::regex("yield")},
        {Token::ktry, std::regex("try")},
        {Token::kgen, std::regex("gen")},

        {Token::kIDENTIFIER, std::regex("[a-zA-Z][a-zA-Z0-9_]*")},
        {Token::kCHAR_LITERAL, std::regex(R"('(([^'\\\n\r\t])|(\\')|(\\")|(\\x[0-7][0-9a-fA-F])|(\\n)|(\\r)|(\\t)|(\\\\)|(\\0))'([a-zA-Z][a-zA-Z0-9_]*)?)")},
        {Token::kSTRING_LITERAL, std::regex(R"("(([^"\\\r\t])|(\\')|(\\")|((\\x[0-7][0-9a-fA-F])|(\\n)|(\\r)|(\\t)|(\\\\)|(\\0))|(\\\n))*"([a-zA-Z][a-zA-Z0-9_]*)?)")},
        {Token::kRAW_STRING_LITERAL, std::regex(R"(r([#]+)([^\r])*?(\1))")},
        {Token::kBYTE_LITERAL, std::regex(R"(b'(([^'\\\r\t\n])|(0x[0-7][0-9a-fA-F]|\n|\r|\t|\\\\|\0)|(\\\n)|(\\')|(\\"))'([a-zA-Z][a-zA-Z0-9_]*)?)")},
        {Token::kBYTE_STRING_LITERAL, std::regex(R"delimeter(b"(([^"\\\r])|(0x[0-7][0-9a-fA-F]|\n|\r|\t|\\\\|\0)|((\\\n)|(\\')|(\\")|(\\\n)))*"([a-zA-Z][a-zA-Z0-9_]*)?)delimeter")},
        {Token::kRAW_BYTE_STRING_LITERAL, std::regex(R"(br([#]+)([^\r])*?(\1))")},
        {Token::kC_STRING_LITERAL, std::regex(R"(c"(([^"\\\r\0])|(\\x[0-7][0-9a-fA-F])|(\\n)|(\\r)|(\\t)|\\\\|(\\\n))*")")},
        {Token::kRAW_C_STRING_LITERAL, std::regex(R"(cr([#]+)([^\r\0])*?(\1))")},
        {Token::kINTEGER_LITERAL, std::regex("((0b[0-1_]*[0-1][0-1_]*)|(0o[0-7_]*[0-7][0-7_]*)|(0x[0-9a-fA-F_]*[0-9a-fA-F][0-9a-fA-F_]*)|([0-9][0-9_]*))((u32)|(i32)|(usize)|(isize))?")},
        // {Token::kFLOAT_LITERAL, std::regex("(([0-9][0-9_]*).(?![._a-zA-Z]))|(([0-9][0-9_]*).([0-9][0-9_]*)([a-df-zA-DF-Z][a-zA-Z0-9_]*)?)")},
        // {Token::kPUNCTUATION, std::regex()}, 
        // {Token::kRESERVED_TOKEN, std::regex(R"delimiter((#+"(([^'\\\r\t])|(\\'\\")|(0x[0-7][0-9a-fA-F]|\n|\r|\t|\\\\|\0)|(\\\n))*"([a-zA-Z][a-zA-Z0-9_]*)?)|((0b[0-1_]*[0-1][0-1_]*[2-9])|(0o[0-7_]*[0-7][0-7_]*[8-9])|((0b[0-1_]*[0-1][0-1_]*)|(0o[0-7_]*[0-7][0-7_]*)|(0x[0-9a-fA-F_]*[0-9a-fA-F][0-9a-fA-F_]*).(?![._a-zA-Z]))|))delimiter")},

        {Token::kPlus, std::regex(R"(\+)")},
        {Token::kMinus, std::regex("-")},
        {Token::kStar, std::regex(R"(\*)")},
        {Token::kSlash, std::regex("/")},
        {Token::kPercent, std::regex("%")},
        {Token::kCaret, std::regex(R"(\^)")},
        {Token::kNot, std::regex(R"(\!)")},
        {Token::kAnd, std::regex("&")},
        {Token::kOr, std::regex(R"(\|)")},
        {Token::kAndAnd, std::regex("&&")},
        {Token::kOrOr, std::regex(R"(\|\|)")},
        {Token::kShl, std::regex("<<")},
        {Token::kShr, std::regex(">>")},
        {Token::kPlusEq, std::regex(R"(\+=)")},
        {Token::kMinusEq, std::regex("-=")},
        {Token::kStarEq, std::regex(R"(\*=)")},
        {Token::kSlashEq, std::regex("/=")},
        {Token::kPercentEq, std::regex("%=")},
        {Token::kCaretEq, std::regex(R"(\^=)")},
        {Token::kAndEq, std::regex("&=")},
        {Token::kOrEq, std::regex(R"(\|=)")},
        {Token::kShlEq, std::regex("<<=")},
        {Token::kShrEq, std::regex(">>=")},
        {Token::kEq, std::regex("=")},
        {Token::kEqEq, std::regex("==")},
        {Token::kNe, std::regex(R"(\!=)")},
        {Token::kGt, std::regex(">")},
        {Token::kLt, std::regex("<")},
        {Token::kGe, std::regex(">=")},
        {Token::kLe, std::regex("<=")},
        {Token::kAt, std::regex("@")},
        {Token::kUnderscore, std::regex("_")},
        {Token::kDot, std::regex(R"(\.)")},
        {Token::kDotDot, std::regex(R"(\.\.)")},
        {Token::kDotDotDot, std::regex(R"(\.\.\.)")},
        {Token::kDotDotEq, std::regex(R"(\.\.=)")},
        {Token::kComma, std::regex(",")},
        {Token::kSemi, std::regex(";")},
        {Token::kColon, std::regex(":")},
        {Token::kPathSep, std::regex("::")},
        {Token::kRArrow, std::regex("->")},
        {Token::kFatArrow, std::regex("=>")},
        {Token::kLArrow, std::regex("<-")},
        {Token::kPound, std::regex("#")},
        {Token::kDollar, std::regex(R"(\$)")},
        {Token::kQuestion, std::regex(R"(\?)")},
        {Token::kTilde, std::regex("~")},

        {Token::kleftCurly, std::regex(R"(\{)")},
        {Token::krightCurly, std::regex(R"(\})")},
        {Token::kleftSquare, std::regex(R"(\[)")},
        {Token::kRightSquare, std::regex(R"(\])")},
        {Token::kleftParenthe, std::regex(R"(\()")},
        {Token::krightParenthe, std::regex(R"(\))")},

        {Token::kCOMMENT, std::regex(R"((//([^\n])*(\n)?)|(/\*[\s\S]*\*/))")},
    };

public:
    std::vector<std::pair<Token, std::string>> lexString(std::string); 

    std::string getString();
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
