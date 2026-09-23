#include "token.h"
#include <unordered_map>

namespace cviz {

Tok keyword_or_ident(const std::string& s) {
    static const std::unordered_map<std::string, Tok> kw = {
        {"auto", Tok::KwAuto},         {"break", Tok::KwBreak},
        {"case", Tok::KwCase},         {"char", Tok::KwChar},
        {"const", Tok::KwConst},       {"continue", Tok::KwContinue},
        {"default", Tok::KwDefault},   {"do", Tok::KwDo},
        {"double", Tok::KwDouble},     {"else", Tok::KwElse},
        {"enum", Tok::KwEnum},         {"extern", Tok::KwExtern},
        {"float", Tok::KwFloat},       {"for", Tok::KwFor},
        {"goto", Tok::KwGoto},         {"if", Tok::KwIf},
        {"inline", Tok::KwInline},     {"int", Tok::KwInt},
        {"long", Tok::KwLong},         {"register", Tok::KwRegister},
        {"restrict", Tok::KwRestrict}, {"return", Tok::KwReturn},
        {"short", Tok::KwShort},       {"signed", Tok::KwSigned},
        {"sizeof", Tok::KwSizeof},     {"static", Tok::KwStatic},
        {"struct", Tok::KwStruct},     {"switch", Tok::KwSwitch},
        {"typedef", Tok::KwTypedef},   {"union", Tok::KwUnion},
        {"unsigned", Tok::KwUnsigned}, {"void", Tok::KwVoid},
        {"volatile", Tok::KwVolatile}, {"while", Tok::KwWhile},
        {"_Bool", Tok::KwBool},        {"_Complex", Tok::KwComplex},
        {"_Imaginary", Tok::KwImaginary},
    };
    auto it = kw.find(s);
    return it == kw.end() ? Tok::Ident : it->second;
}

const char* tok_name(Tok t) {
    switch (t) {
        case Tok::Ident:        return "IDENT";
        case Tok::IntLit:       return "INT_LIT";
        case Tok::CharLit:      return "CHAR_LIT";
        case Tok::StringLit:    return "STRING_LIT";
        case Tok::FloatLit:     return "FLOAT_LIT";
        case Tok::KwAuto:       return "KW_AUTO";
        case Tok::KwBreak:      return "KW_BREAK";
        case Tok::KwCase:       return "KW_CASE";
        case Tok::KwChar:       return "KW_CHAR";
        case Tok::KwConst:      return "KW_CONST";
        case Tok::KwContinue:   return "KW_CONTINUE";
        case Tok::KwDefault:    return "KW_DEFAULT";
        case Tok::KwDo:         return "KW_DO";
        case Tok::KwDouble:     return "KW_DOUBLE";
        case Tok::KwElse:       return "KW_ELSE";
        case Tok::KwEnum:       return "KW_ENUM";
        case Tok::KwExtern:     return "KW_EXTERN";
        case Tok::KwFloat:      return "KW_FLOAT";
        case Tok::KwFor:        return "KW_FOR";
        case Tok::KwGoto:       return "KW_GOTO";
        case Tok::KwIf:         return "KW_IF";
        case Tok::KwInline:     return "KW_INLINE";
        case Tok::KwInt:        return "KW_INT";
        case Tok::KwLong:       return "KW_LONG";
        case Tok::KwRegister:   return "KW_REGISTER";
        case Tok::KwRestrict:   return "KW_RESTRICT";
        case Tok::KwReturn:     return "KW_RETURN";
        case Tok::KwShort:      return "KW_SHORT";
        case Tok::KwSigned:     return "KW_SIGNED";
        case Tok::KwSizeof:     return "KW_SIZEOF";
        case Tok::KwStatic:     return "KW_STATIC";
        case Tok::KwStruct:     return "KW_STRUCT";
        case Tok::KwSwitch:     return "KW_SWITCH";
        case Tok::KwTypedef:    return "KW_TYPEDEF";
        case Tok::KwUnion:      return "KW_UNION";
        case Tok::KwUnsigned:   return "KW_UNSIGNED";
        case Tok::KwVoid:       return "KW_VOID";
        case Tok::KwVolatile:   return "KW_VOLATILE";
        case Tok::KwWhile:      return "KW_WHILE";
        case Tok::KwBool:       return "KW_BOOL";
        case Tok::KwComplex:    return "KW_COMPLEX";
        case Tok::KwImaginary:  return "KW_IMAGINARY";
        case Tok::LShiftAssign: return "LSHIFT_ASSIGN";
        case Tok::RShiftAssign: return "RSHIFT_ASSIGN";
        case Tok::Ellipsis:     return "ELLIPSIS";
        case Tok::Arrow:        return "ARROW";
        case Tok::PlusPlus:     return "PLUSPLUS";
        case Tok::MinusMinus:   return "MINUSMINUS";
        case Tok::LShift:       return "LSHIFT";
        case Tok::RShift:       return "RSHIFT";
        case Tok::Le:           return "LE";
        case Tok::Ge:           return "GE";
        case Tok::Eq:           return "EQ";
        case Tok::Ne:           return "NE";
        case Tok::AndAnd:       return "ANDAND";
        case Tok::OrOr:         return "OROR";
        case Tok::PlusAssign:   return "PLUS_ASSIGN";
        case Tok::MinusAssign:  return "MINUS_ASSIGN";
        case Tok::StarAssign:   return "STAR_ASSIGN";
        case Tok::SlashAssign:  return "SLASH_ASSIGN";
        case Tok::PercentAssign:return "PERCENT_ASSIGN";
        case Tok::AmpAssign:    return "AMP_ASSIGN";
        case Tok::CaretAssign:  return "CARET_ASSIGN";
        case Tok::PipeAssign:   return "PIPE_ASSIGN";
        case Tok::LBracket:     return "LBRACKET";
        case Tok::RBracket:     return "RBRACKET";
        case Tok::LParen:       return "LPAREN";
        case Tok::RParen:       return "RPAREN";
        case Tok::LBrace:       return "LBRACE";
        case Tok::RBrace:       return "RBRACE";
        case Tok::Dot:          return "DOT";
        case Tok::Amp:          return "AMP";
        case Tok::Star:         return "STAR";
        case Tok::Plus:         return "PLUS";
        case Tok::Minus:        return "MINUS";
        case Tok::Tilde:        return "TILDE";
        case Tok::Bang:         return "BANG";
        case Tok::Slash:        return "SLASH";
        case Tok::Percent:      return "PERCENT";
        case Tok::Lt:           return "LT";
        case Tok::Gt:           return "GT";
        case Tok::Caret:        return "CARET";
        case Tok::Pipe:         return "PIPE";
        case Tok::Question:     return "QUESTION";
        case Tok::Colon:        return "COLON";
        case Tok::Semi:         return "SEMI";
        case Tok::Assign:       return "ASSIGN";
        case Tok::Comma:        return "COMMA";
        case Tok::Hash:         return "HASH";
        case Tok::EndOfFile:    return "EOF_TOKEN";
        case Tok::Error:        return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace cviz
