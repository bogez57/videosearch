
// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////////////////////////////////////
//~ rjf: Test Tree Building & Iterating

function void
PushTest(String8 name)
{
 TestNode *parent = test_ctx->active_parent;
 TestNode *node = PushArray(test_ctx->arena, TestNode, 1);
 node->name = PushStr8Copy(test_ctx->arena, name);
 node->parent = parent;
 if(parent != 0)
 {
  QueuePush(parent->first, parent->last, node);
 }
 else
 {
  QueuePush(test_ctx->first_root, test_ctx->last_root, node);
 }
 test_ctx->active_parent = node;
}

function void
PushTestF(char *fmt, ...)
{
 Temp scratch = ScratchBegin(0, 0);
 va_list args;
 va_start(args, fmt);
 String8 string = PushStr8FV(scratch.arena, fmt, args);
 PushTest(string);
 va_end(args);
 ScratchEnd(scratch);
}

function void
PopTest(void)
{
 if(test_ctx->active_parent != 0)
 {
  test_ctx->active_parent = test_ctx->active_parent->parent;
 }
}

function void
TestResult(B32 check)
{
 TestNode *parent = test_ctx->active_parent;
 TestNode *node = PushArray(test_ctx->arena, TestNode, 1);
 node->parent = parent;
 if(parent != 0)
 {
  QueuePush(parent->first, parent->last, node);
 }
 else
 {
  QueuePush(test_ctx->first_root, test_ctx->last_root, node);
 }
 for(TestNode *n = node; n != 0; n = n->parent)
 {
  n->good_count += !!check;
  n->total_count += 1;
 }
}

function TestNodeRec
TestNodeRecDepthFirst(TestNode *node)
{
 TestNodeRec rec = {0};
 if(node->first)
 {
  rec.next = node->first;
  rec.push_count += 1;
 }
 else for(TestNode *p = node; p != 0; p = p->parent)
 {
  if(p->next)
  {
   rec.next = p->next;
   break;
  }
  rec.pop_count += 1;
 }
 return rec;
}

////////////////////////////////////////////////////////////////
//~ rjf: Metadesk Test Tree Building

function MD_Node *
MakeTestNode(MD_NodeKind kind, MD_NodeFlags flags, String8 string)
{
 MD_Node *node = MD_PushNode(test_ctx->arena, kind, flags, string, string, 0);
 return node;
}

function B32
MatchParsedWithNode(String8 string, MD_Node *tree)
{
 MD_TokenizeResult tokenize = MD_TokenizeFromText(test_ctx->arena, string);
 MD_ParseResult parse = MD_ParseFromTextTokens(test_ctx->arena, Str8Lit("test"), string, tokenize.tokens);
 B32 result = MD_NodeDeepMatch(tree, parse.root->first, 0);
 return result;
}
