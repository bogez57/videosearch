// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BGZ_BASE_CORE_H
#define BGZ_BASE_CORE_H

////////////////////////////////
//~ bgz: Keywords
//
// raddebugger's base has no notion of a dll: everything in it is `internal`.
// bgz's layers are compiled into core.dll and offer their functions to the
// app, so they mark those `exported`. In the app, which only declares them,
// the same word imports them - see src/core/core_exports.h.

#if !defined(exported)
# if OS_WINDOWS
#  define exported __declspec(dllexport)
# else
#  define exported
# endif
#endif

#define UnusedVariable(name) (void)(name)

////////////////////////////////
//~ bgz: Warnings
//
// Around code bgz does not want to hold to its own warning flags.

#if COMPILER_CLANG
# define DISABLE_CLANG_WARNINGS()                                           \
_Pragma("clang diagnostic push")                                           \
_Pragma("clang diagnostic ignored \"-Wvisibility\"")                        \
_Pragma("clang diagnostic ignored \"-Wmicrosoft-include\"")                 \
_Pragma("clang diagnostic ignored \"-Wabsolute-value\"")                    \
_Pragma("clang diagnostic ignored \"-Wmacro-redefined\"")                   \
_Pragma("clang diagnostic ignored \"-Wswitch\"")                            \
_Pragma("clang diagnostic ignored \"-Wsign-conversion\"")                   \
_Pragma("clang diagnostic ignored \"-Wimplicit-float-conversion\"")         \
_Pragma("clang diagnostic ignored \"-Wimplicit-int-conversion\"")           \
_Pragma("clang diagnostic ignored \"-Wshadow\"")                            \
_Pragma("clang diagnostic ignored \"-Winitializer-overrides\"")             \
_Pragma("clang diagnostic ignored \"-Wmissing-braces\"")                    \
_Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")                  \
_Pragma("clang diagnostic ignored \"-Wshorten-64-to-32\"")                  \
_Pragma("clang diagnostic ignored \"-Wunused-but-set-variable\"")           \
_Pragma("clang diagnostic ignored \"-Wunused-parameter\"")                  \
_Pragma("clang diagnostic ignored \"-Wunused-variable\"")                   \
_Pragma("clang diagnostic ignored \"-Wunused-function\"")                   \
_Pragma("clang diagnostic ignored \"-Wsign-compare\"")                      \
_Pragma("clang diagnostic ignored \"-Wmissing-field-initializers\"")        \
_Pragma("clang diagnostic ignored \"-Wgnu-null-pointer-arithmetic\"")       \
_Pragma("clang diagnostic ignored \"-Wunknown-pragmas\"")
# define RESTORE_CLANG_WARNINGS() _Pragma("clang diagnostic pop")
#else
# define DISABLE_CLANG_WARNINGS()
# define RESTORE_CLANG_WARNINGS()
#endif

////////////////////////////////
//~ bgz: raddebugger's Core
//
// bgz's functions for raddebugger's core (rad/base/base_core.h). Each only
// calls raddebugger's function of the same name.

exported U64    NowTimeUS(void);
exported void   SleepMS(U32 ms);

exported TxtPt  TxtPtMake(S64 line, S64 column);
exported B32    TxtPtMatch(TxtPt a, TxtPt b);
exported B32    TxtPtLessThan(TxtPt a, TxtPt b);
exported TxtPt  TxtPtMin(TxtPt a, TxtPt b);
exported TxtPt  TxtPtMax(TxtPt a, TxtPt b);
exported TxtRng TxtRngMake(TxtPt min, TxtPt max);

#define Axis2Flip(a) axis2_flip(a)

#define InsAtomicU64Eval(x)                  ins_atomic_u64_eval(x)
#define InsAtomicU64IncEval(x)               ins_atomic_u64_inc_eval(x)
#define InsAtomicU64DecEval(x)               ins_atomic_u64_dec_eval(x)
#define InsAtomicU64EvalAssign(x, c)         ins_atomic_u64_eval_assign((x), (c))
#define InsAtomicU64EvalCondAssign(x, k, c)  ins_atomic_u64_eval_cond_assign((x), (k), (c))

#endif // BGZ_BASE_CORE_H
