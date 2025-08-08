#include "lexser.hpp"
#include <iostream>

std::string to_string(Token token) {
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
    }
    return "UNKNOWN_TOKEN"; // 处理未覆盖的情况
}

int main() {
    lexser lex;
    auto s = lex.getString();
    auto ve = lex.lexString(s);

    for (auto [token, s] : ve) {
        std::cout << to_string(token) << " " << s << "\n";
    }
}