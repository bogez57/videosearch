
// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef TESTS_H
#define TESTS_H

////////////////////////////////////////////////////////////////
//~ rjf: Test Tree Building Types

typedef struct TestNode TestNode;
struct TestNode
{
 TestNode *next;
 TestNode *first;
 TestNode *last;
 TestNode *parent;
 String8 name;
 S64 good_count;
 S64 total_count;
};

typedef struct TestNodeRec TestNodeRec;
struct TestNodeRec
{
 TestNode *next;
 S32 push_count;
 S32 pop_count;
};

typedef struct TestCtx TestCtx;
struct TestCtx
{
 Arena *arena;
 TestNode *first_root;
 TestNode *last_root;
 TestNode *active_parent;
};

////////////////////////////////////////////////////////////////
//~ rjf: Globals

global TestCtx *test_ctx = 0;

////////////////////////////////////////////////////////////////
//~ rjf: Test Tree Building & Iterating

function void PushTest(String8 name);
function void PushTestF(char *fmt, ...);
function void PopTest(void);
#define TestScope(name) DeferLoop(PushTest(name), PopTest())
#define TestScopeF(fmt, ...) DeferLoop(PushTestF(fmt, __VA_ARGS__), PopTest())

function void TestResult(B32 check);

function TestNodeRec TestNodeRecDepthFirst(TestNode *node);

////////////////////////////////////////////////////////////////
//~ rjf: Metadesk Test Tree Building

function MD_Node *MakeTestNode(MD_NodeKind kind, MD_NodeFlags flags, String8 string);
function B32 MatchParsedWithNode(String8 string, MD_Node *tree);

#endif //TESTS_H
