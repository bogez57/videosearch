
// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////////////////////////////////////
//~ rjf: Build Options

#define BUILD_COMMAND_LINE_INTERFACE 1

////////////////////////////////////////////////////////////////
//~ rjf: Includes

//- rjf: [h] root
#include "base/base_inc.h"
#include "window_manager/window_manager_inc.h" // @Changed(no_os_layer) was os/os_inc.h
#include "shell/shell_inc.h"
#include "socket/socket_inc.h"
#include "mdesk/mdesk.h"

//- rjf: [h] tests
#include "tests.h"

//- rjf: [c] root
#include "base/base_inc.c"
#include "window_manager/window_manager_inc.c" // @Changed(no_os_layer) was os/os_inc.c
#include "shell/shell_inc.c"
#include "socket/socket_inc.c"
#include "mdesk/mdesk.c"

//- rjf: [c] tests
#include "tests.c"

////////////////////////////////////////////////////////////////
//~ rjf: Entry Point

function void
EntryPoint(CmdLine *cmdln)
{
 Arena *arena = ArenaAlloc(Gigabytes(64));
 test_ctx = PushArray(arena, TestCtx, 1);
 test_ctx->arena = arena;
 
 ///////////////////////////////
 //- rjf: metadesk parser tests
 //
 TestScope(Str8Lit("Metadesk Parser"))
 {
  TestScope(Str8Lit("Lexer"))
  {
   String8 string = Str8Lit("abc def 123 456 123_456 abc123 123abc +-*");
   MD_TokenizeResult tokenize = MD_TokenizeFromText(arena, string);
   MD_Token tokens[256] = {0};
   for(U64 idx = 0; idx < ArrayCount(tokens) && idx < tokenize.tokens.count; idx += 1)
   {
    tokens[idx] = tokenize.tokens.v[idx];
   }
   TestResult(MD_TokenMatch(tokens[0],  MD_TokenMake(R1U64(0, 3),   MD_TokenFlag_Identifier)));
   TestResult(MD_TokenMatch(tokens[1],  MD_TokenMake(R1U64(3, 4),   MD_TokenFlag_Whitespace)));
   TestResult(MD_TokenMatch(tokens[2],  MD_TokenMake(R1U64(4, 7),   MD_TokenFlag_Identifier)));
   TestResult(MD_TokenMatch(tokens[3],  MD_TokenMake(R1U64(7, 8),   MD_TokenFlag_Whitespace)));
   TestResult(MD_TokenMatch(tokens[4],  MD_TokenMake(R1U64(8, 11),  MD_TokenFlag_Numeric)));
   TestResult(MD_TokenMatch(tokens[5],  MD_TokenMake(R1U64(11, 12), MD_TokenFlag_Whitespace)));
   TestResult(MD_TokenMatch(tokens[6],  MD_TokenMake(R1U64(12, 15), MD_TokenFlag_Numeric)));
   TestResult(MD_TokenMatch(tokens[7],  MD_TokenMake(R1U64(15, 16), MD_TokenFlag_Whitespace)));
   TestResult(MD_TokenMatch(tokens[8],  MD_TokenMake(R1U64(16, 23), MD_TokenFlag_Numeric)));
   TestResult(MD_TokenMatch(tokens[9],  MD_TokenMake(R1U64(23, 24), MD_TokenFlag_Whitespace)));
   TestResult(MD_TokenMatch(tokens[10], MD_TokenMake(R1U64(24, 30), MD_TokenFlag_Identifier)));
   TestResult(MD_TokenMatch(tokens[11], MD_TokenMake(R1U64(30, 31), MD_TokenFlag_Whitespace)));
   TestResult(MD_TokenMatch(tokens[12], MD_TokenMake(R1U64(31, 37), MD_TokenFlag_Numeric)));
   TestResult(MD_TokenMatch(tokens[13], MD_TokenMake(R1U64(37, 38), MD_TokenFlag_Whitespace)));
   TestResult(MD_TokenMatch(tokens[14], MD_TokenMake(R1U64(38, 41), MD_TokenFlag_Symbol)));
  }
  TestScope(Str8Lit("Empty Sets"))
  {
   TestResult(MatchParsedWithNode(Str8Lit("{}"), MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasBraceLeft|MD_NodeFlag_HasBraceRight, Str8Lit(""))));
   TestResult(MatchParsedWithNode(Str8Lit("()"), MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasParenLeft|MD_NodeFlag_HasParenRight, Str8Lit(""))));
   TestResult(MatchParsedWithNode(Str8Lit("[]"), MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasBracketLeft|MD_NodeFlag_HasBracketRight, Str8Lit(""))));
   TestResult(MatchParsedWithNode(Str8Lit("[)"), MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasBracketLeft|MD_NodeFlag_HasParenRight, Str8Lit(""))));
   TestResult(MatchParsedWithNode(Str8Lit("(]"), MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasParenLeft|MD_NodeFlag_HasBracketRight, Str8Lit(""))));
  }
  TestScope(Str8Lit("Simple Unnamed Sets"))
  {
   {
    String8 string = Str8Lit("{a, b, c}");
    MD_Node *tree = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasBraceLeft|MD_NodeFlag_HasBraceRight, Str8Lit(""));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_IsBeforeComma, Str8Lit("a")));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_IsAfterComma|MD_NodeFlag_IsBeforeComma, Str8Lit("b")));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_IsAfterComma, Str8Lit("c")));
    TestResult(MatchParsedWithNode(string, tree));
   }
   {
    String8 string = Str8Lit("(1 2 3 4 5)");
    MD_Node *tree = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasParenLeft|MD_NodeFlag_HasParenRight, Str8Lit(""));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("1")));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("2")));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("3")));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("4")));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("5")));
    TestResult(MatchParsedWithNode(string, tree));
   }
   {
    String8 string = Str8Lit("{a}");
    MD_Node *tree = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasBraceLeft|MD_NodeFlag_HasBraceRight, Str8Lit(""));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier, Str8Lit("a")));
    TestResult(MatchParsedWithNode(string, tree));
   }
  }
  TestScope(Str8Lit("Simple Named Sets"))
  {
   String8 string = Str8Lit("simple_set: {a; b; c}");
   MD_Node *tree = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_HasBraceLeft|MD_NodeFlag_HasBraceRight, Str8Lit("simple_set"));
   MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_IsBeforeSemicolon, Str8Lit("a")));
   MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_IsBeforeSemicolon|MD_NodeFlag_IsAfterSemicolon, Str8Lit("b")));
   MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_IsAfterSemicolon, Str8Lit("c")));
   TestResult(MatchParsedWithNode(string, tree));
  }
  TestScope(Str8Lit("Nested Sets"))
  {
   {
    String8 string = Str8Lit("{a b:{1 2 3} c}");
    MD_Node *tree = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasBraceLeft|MD_NodeFlag_HasBraceRight, Str8Lit(""));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier, Str8Lit("a")));
    {
     MD_Node *sub = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_HasBraceLeft|MD_NodeFlag_HasBraceRight, Str8Lit("b"));
     MD_NodePushChild(sub, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("1")));
     MD_NodePushChild(sub, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("2")));
     MD_NodePushChild(sub, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("3")));
     MD_NodePushChild(tree, sub);
    }
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier, Str8Lit("c")));
    TestResult(MatchParsedWithNode(string, tree));
   }
   {
    String8 string = Str8Lit("foo: { (size: u64) -> *void }");
    MD_Node *tree = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier|MD_NodeFlag_HasBraceLeft|MD_NodeFlag_HasBraceRight, Str8Lit("foo"));
    MD_Node *params = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_HasParenLeft|MD_NodeFlag_HasParenRight, Str8Lit(""));
    MD_Node *size = MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier, Str8Lit("size"));
    MD_NodePushChild(size, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier, Str8Lit("u64")));
    MD_NodePushChild(params, size);
    MD_NodePushChild(tree, params);
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Symbol, Str8Lit("->")));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Symbol, Str8Lit("*")));
    MD_NodePushChild(tree, MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier, Str8Lit("void")));
    TestResult(MatchParsedWithNode(string, tree));
   }
  }
  TestScope(Str8Lit("Non-Sets"))
  {
   TestResult(MatchParsedWithNode(Str8Lit("foo"), MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Identifier, Str8Lit("foo"))));
   TestResult(MatchParsedWithNode(Str8Lit("123"), MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Numeric, Str8Lit("123"))));
   TestResult(MatchParsedWithNode(Str8Lit("+"),   MakeTestNode(MD_NodeKind_Main, MD_NodeFlag_Symbol, Str8Lit("+"))));
  }
 }
 
 ///////////////////////////////
 //- rjf: dump test tree
 //
 {
  S32 depth = 0;
  char *indentation = "                                                        ";
  for(TestNode *root = test_ctx->first_root; root != 0; root = root->next)
  {
   printf("--------------------------------------------------------------------------------\n");
   for(TestNode *node = root, *next = 0; node != 0; node = next)
   {
    TestNodeRec rec = TestNodeRecDepthFirst(node);
    next = rec.next;
    if(node->name.size != 0)
    {
     printf("%.*s[%s] %.*s (%I64d/%I64d)\n",
            depth*2, indentation,
            node->good_count == node->total_count ? "." : "X",
            Str8VArg(node->name),
            node->good_count,
            node->total_count);
    }
    else
    {
     printf("%.*s[%s]\n", depth*2, indentation, node->good_count == node->total_count ? "." : "X");
    }
    depth += rec.push_count-rec.pop_count;
   }
  }
 }
}
