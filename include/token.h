#pragma once
#include <string>
#include "source.h"

namespace cviz {

enum class Tok {
    Ident, IntLit, CharLit, StringLit, FloatLit,

    KwAuto, KwBreak, KwCase, KwChar, KwConst, KwContinue,
    KwDefault, KwDo, KwDouble, KwElse, KwEnum, KwExtern,
    KwFloat, KwFor, KwGoto, KwIf, KwInline, KwInt,
    KwLong, KwRegister, KwRestrict, KwReturn, KwShort, KwSigned,
    KwSizeof, KwStatic, KwStruct, KwSwitch, KwTypedef, KwUnion,
    KwUnsigned, KwVoid, KwVolatile, KwWhile, KwBool, KwComplex,
    KwImaginary,

    LShiftAssign, RShiftAssign, Ellipsis,

    Arrow, PlusPlus, MinusMinus, LShift, RShift,
    Le, Ge, Eq, Ne, AndAnd, OrOr,
    PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
    AmpAssign, CaretAssign, PipeAssign,

    LBracket, RBracket, LParen, RParen, LBrace, RBrace,
    Dot, Amp, Star, Plus, Minus, Tilde, Bang,
    Slash, Percent, Lt, Gt, Caret, Pipe,
    Question, Colon, Semi, Assign, Comma, Hash,

    EndOfFile,
    Error,   
};

const char* tok_name(Tok t);

struct Token {
    Tok         kind = Tok::Error;
    std::string lexeme;
    Span        span;

    long long   int_value = 0;
    std::string suffix;

    bool is(Tok t) const { return kind == t; }
};

Tok keyword_or_ident(const std::string& s);

} 