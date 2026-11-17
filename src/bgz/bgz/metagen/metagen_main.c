// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////////////////////////////////////
//~ rjf: Includes

#if !defined(XXH_IMPLEMENTATION)
# define XXH_IMPLEMENTATION
# define XXH_STATIC_LINKING_ONLY
# include "third_party/xxHash/xxhash.h"
#endif

#include "metagen/metagen_base/metagen_base_inc.h"
#include "metagen/metagen_os/metagen_os_inc.h"
#include "metagen/metagen_c_lex/metagen_c_lex.h"
#include "mdesk/mdesk.h"
#include "metagen.h"

#include "metagen/metagen_base/metagen_base_inc.c"
#include "metagen/metagen_os/metagen_os_inc.c"
#include "metagen/metagen_c_lex/metagen_c_lex.c"
#include "mdesk/mdesk.c"
#include "metagen.c"

////////////////////////////////////////////////////////////////
//~ rjf: Entry Point

function void
EntryPoint(CmdLine *cmdln)
{
 ///////////////////////////////
 //- rjf: initialize state
 //
 mg_arena = ArenaAlloc();
 mg_state = PushArray(mg_arena, MG_State, 1);
 mg_state->layer_slots_count = 256;
 mg_state->layer_slots = PushArray(mg_arena, MG_LayerSlot, mg_state->layer_slots_count);
 mg_state->table_slots_count = 256;
 mg_state->table_slots = PushArray(mg_arena, MG_TableSlot, mg_state->table_slots_count);
 
 ///////////////////////////////
 //- rjf: find directories
 //
 // @Changed(scan required) layer keys are relative to the --scan folder.
 String8 scan_folder = CmdLineOptString(cmdln, Str8Lit("scan"));
 if(scan_folder.size == 0)
 {
  fprintf(stderr, "metagen: no folder to scan - usage: metagen --scan:<folder>\n");
  OS_Abort();
 }
 String8 project_path = OS_NormalizedPathFromStr8(mg_arena, scan_folder);
 String8 code_path = PushStr8F(mg_arena, "%S/", project_path);
 mg_scan_path = project_path;
 
 ///////////////////////////////
 //- rjf: gather all paths to look at
 //
 String8List path_list = {0};
 ProfScope("gather all paths to look at") // DeferLoop(printf("gathering paths..."), printf("%i found\n", (int)path_list.node_count))
 {
	 typedef struct Task Task;
	 struct Task
	 {
		 Task *next;
		 String8 dir;
	 };

	 Task start_task = {0, code_path};
	 Task *first_task = &start_task;
	 Task *last_task = &start_task;
	 for(Task *task = first_task; task != 0; task = task->next) {
		 OS_FileIter *it = OS_FileIterBegin(mg_arena, task->dir);
		 for(OS_FileInfo info = {0}; OS_FileIterNext(mg_arena, it, &info);) {
			 if(info.attributes.flags & OS_FileFlag_Directory) {
				 // @Changed(scan required) bgz is scanned on its own.
				 if(Str8Match(info.name, Str8Lit("bgz"), MatchFlag_CaseInsensitive)) {
					 continue;
				 }
				 // @Added(skip rad) raddebugger's generated files come with the copy.
				 if(Str8Match(info.name, Str8Lit("rad"), MatchFlag_CaseInsensitive)) {
					 continue;
				 }
				 String8 path = PushStr8F(mg_arena, "%S%S/", task->dir, info.name);
				 Task *task = PushArray(mg_arena, Task, 1);
				 QueuePush(first_task, last_task, task);
				 task->dir = path;
			 }
			 else {
				 String8 path = PushStr8F(mg_arena, "%S%S", task->dir, info.name);

				 //Don't put in paths for misc files like my the actual produced exe, etc.
				 B32 is_exe = FindSubstr8(path, Str8Lit(".exe"), 0, 0) < path.size;
				 B32 is_pdb = FindSubstr8(path, Str8Lit(".pdb"), 0, 0) < path.size;
				 B32 is_batch = FindSubstr8(path, Str8Lit(".bat"), 0, 0) < path.size;
				 B32 is_this_file = FindSubstr8(path, Str8Lit("file_generator.c"), 0, 0) < path.size;
				 if(is_exe || is_pdb || is_batch || is_this_file) {
					 continue;
				 }

				 Str8ListPush(mg_arena, &path_list, path);
			 }
		 }

		 OS_FileIterEnd(it);
	 }
 }
 
 ///////////////////////////////
 //- rjf: load & lex all C files
 //
 MG_CFileList c_files = {0};
 ProfScope("load & lex all C files") // DeferLoop(printf("loading & parsing all C files..."), printf("%i loaded\n", (int)c_files.count))
 {
	for(String8Node *n = path_list.first; n != 0; n = n->next) {
		String8 path = n->string;

		B32 is_metacode = FindSubstr8(path, Str8Lit(".meta."), 0, 0) < path.size;
		B32 is_third_party = FindSubstr8(path, Str8Lit("third_party/"), 0, 0) < path.size;
		if(is_metacode || is_third_party) {
			continue;
		}

		if(Str8Match(Str8PathSkipLastPeriod(path), Str8Lit("c"), MatchFlag_CaseInsensitive) || Str8Match(Str8PathSkipLastPeriod(path), Str8Lit("h"), MatchFlag_CaseInsensitive)) {
			MG_CFile *c_file = PushArray(mg_arena, MG_CFile, 1);
			QueuePush(c_files.first, c_files.last, c_file);
			c_file->path = path;
			c_file->data = OS_DataFromFilePath(mg_arena, path);
			c_file->tokens = CL_TokenArrayFromString(mg_arena, c_file->data);
			c_files.count += 1;	
		}
	}
 }
 
 ///////////////////////////////
 //- rjf: load & parse all metadesk files
 //
 MG_MDeskFileList mdesk_files = {0};
 ProfScope("load & parse all metadesk files") // DeferLoop(printf("loading & parsing all Metadesk files..."), printf("%i loaded\n", (int)mdesk_files.count))
 {
	for(String8Node *n = path_list.first; n != 0; n = n->next) {
		String8 path = n->string;

		if(Str8Match(Str8PathSkipLastPeriod(path), Str8Lit("mdesk"), MatchFlag_CaseInsensitive)) {
			String8 data = OS_DataFromFilePath(mg_arena, path);
			MD_TokenizeResult tokenize = MD_TokenizeFromText(mg_arena, data);
			MD_ParseResult parse = MD_ParseFromTextTokens(mg_arena, path, data, tokenize.tokens);

			MD_MsgListConcatInPlace(&mg_state->msgs, &tokenize.msgs);
			MD_MsgListConcatInPlace(&mg_state->msgs, &parse.msgs);

			MG_MDeskFile *file = PushArray(mg_arena, MG_MDeskFile, 1);
			QueuePush(mdesk_files.first, mdesk_files.last, file);
			file->path = path;
			file->data = data;
			file->parse = parse;

			mdesk_files.count += 1;
		}
	}
}
 
 ///////////////////////////////
 //- rjf: parse & gather all tables
 //
 U64 table_count = 0;
 ProfScope("parse & gather all tables") // DeferLoop(printf("parsing all tables..."), printf("%i parsed\n", (int)table_count))
 {
	 for(MG_MDeskFile *file = mdesk_files.first; file != 0; file = file->next)
	 {
		 for(MD_EachNode(top_level_node, file->parse.root->first))
		 {
			 MD_Node *table_tag = MD_TagFromString(top_level_node, Str8Lit("table"), 0);
			 if(!MD_NodeIsNil(table_tag))
			 {
				 U64 hash = MG_HashFromString(top_level_node->string);
				 U64 slot_idx = hash%mg_state->table_slots_count;
				 MG_TableSlot *slot = &mg_state->table_slots[slot_idx];

				 MG_TableNode *existing_node = 0;
				 for(MG_TableNode *n = slot->first; n != 0; n = n->next)
				 {
					 if(Str8Match(n->v.root->string, top_level_node->string, 0))
					 {
						 existing_node = n;
						 break;
					 }
				 }

				 if(existing_node != 0)
				 {
					 MD_MsgListPushF(mg_arena, &mg_state->msgs, top_level_node, MD_MsgKind_Error,
									 "Table '%S' redefined.", top_level_node->string);
				 }

				 if(existing_node == 0)
				 {
					 MG_TableNode *n = PushArray(mg_arena, MG_TableNode, 1);
					 MG_Table *table = &n->v;
					 QueuePush(slot->first, slot->last, n);

					 table->root = top_level_node;

					 // rjf: parse header
					 {
						 table->header.column_count = table_tag->child_count;
						 table->header.column_descs = PushArray(mg_arena, MG_ColumnDesc, table->header.column_count);

						 U64 idx = 0;
						 for(MD_EachNode(column_node, table_tag->first))
						 {
							 table->header.column_descs[idx].kind = MG_ColumnKind_Default;
							 table->header.column_descs[idx].name = column_node->string;

							 MD_Node *check_for_tag = MD_TagFromString(column_node, Str8Lit("check_for_tag"), 0);
							 if(!MD_NodeIsNil(check_for_tag))
							 {
								 table->header.column_descs[idx].kind = MG_ColumnKind_CheckForTag;
								 table->header.column_descs[idx].tag_string = check_for_tag->first->string;
							 }

							 MD_Node *default_value = MD_TagFromString(column_node, Str8Lit("default"), 0);
							 if(!MD_NodeIsNil(default_value))
							 {
								 table->header.column_descs[idx].default_value = default_value->first->string;
							 }

							 idx += 1;
						 }
					 }

					 // rjf: parse grid
					 {
						 // rjf: determine dimensions
						 U64 row_count = 0;
						 U64 column_count = 0;
						 {
							 for(MD_EachNode(row, top_level_node->first))
							 {
								 row_count += 1;
								 U64 cell_count_this_row = row->child_count;
								 column_count = Max(cell_count_this_row, column_count);
							 }
						 }

						 // rjf: allocate cells / row parents
						 {
							 table->grid.cells.count = row_count*column_count;
							 table->grid.row_parents.count = row_count;

							 table->grid.cells.v = PushArray(mg_arena, MD_Node *, table->grid.cells.count);
							 table->grid.row_parents.v = PushArray(mg_arena, MD_Node *, table->grid.row_parents.count);

							 for(U64 idx = 0; idx < table->grid.cells.count; idx += 1)
							 {
								 table->grid.cells.v[idx] = &md_nil_node;
							 }
							 for(U64 idx = 0; idx < table->grid.row_parents.count; idx += 1)
							 {
								 table->grid.row_parents.v[idx] = &md_nil_node;
							 }
						 }

						 // rjf: fill cells
						 {
							 U64 row_idx = 0;
							 for(MD_EachNode(row, top_level_node->first))
							 {
								 U64 col_idx = 0;
								 table->grid.row_parents.v[row_idx] = row;

								 for(MD_EachNode(cell, row->first))
								 {
									 table->grid.cells.v[row_idx * column_count + col_idx] = cell;
									 col_idx += 1;
								 }

								 row_idx += 1;
							 }
						 }
					 }
				 }

				 table_count += 1;
			 }
		 }
	 }
 }

 ///////////////////////////////
 //- rjf: parse & gather all C markup
 //
 MG_MarkupList tweak_b32s = {0};
 MG_MarkupList tweak_f32s = {0};
 MG_MarkupList embeds = {0};
 U64 markup_count = 0;
 ProfScope("parse & gather all C markup") // DeferLoop(printf("parsing all markup..."), printf("%i parsed\n", (int)markup_count))
 {
  for(MG_CFile *file = c_files.first; file != 0; file = file->next) {
		String8 path = file->path;
		String8 layer_key = MG_LayerKeyFromPath(path);
		MG_Layer *layer = MG_LayerFromKey(layer_key);

		MG_MarkupList layer_tweak_b32s = MG_MarkupListFromLayerTextTokensName(mg_arena, layer, file->data, file->tokens, Str8Lit("TweakB32"));
		MG_MarkupList layer_tweak_f32s = MG_MarkupListFromLayerTextTokensName(mg_arena, layer, file->data, file->tokens, Str8Lit("TweakF32"));
		MG_MarkupList layer_embeds = MG_MarkupListFromLayerTextTokensName(mg_arena, layer, file->data, file->tokens, Str8Lit("EmbedFile"));

		markup_count += layer_tweak_b32s.count;
		markup_count += layer_tweak_f32s.count;
		markup_count += layer_embeds.count;
		layer->tweak_b32_count += layer_tweak_b32s.count;
		layer->tweak_f32_count += layer_tweak_f32s.count;
		layer->embed_count += layer_embeds.count;

		MG_MarkupListConcatInPlace(&tweak_b32s, &layer_tweak_b32s);
		MG_MarkupListConcatInPlace(&tweak_f32s, &layer_tweak_f32s);
		MG_MarkupListConcatInPlace(&embeds, &layer_embeds);
	}
}
 
 ///////////////////////////////
 //- rjf: generate xlists
 //
 U64 xlist_count = 0;
 ProfScope("generate xlists") // DeferLoop(printf("generating all xlists..."), printf("%i generated\n", (int)xlist_count))
 {
  for(MG_MDeskFile *file = mdesk_files.first;
      file != 0;
      file = file->next)
  {
   String8 path = file->path;
   String8 layer_key = MG_LayerKeyFromPath(path);
   MG_Layer *layer = MG_LayerFromKey(layer_key);
   for(MD_EachNode(node, file->parse.root->first))
   {
    MD_Node *xlist_tag = MD_TagFromString(node, Str8Lit("xlist"), 0);
    if(!MD_NodeIsNil(xlist_tag))
    {
     Str8ListPushF(mg_arena, &layer->enums, "#define %S \\\n", node->string);
     String8List strings = MG_GenStringListFromNode(node);
     for(String8Node *n = strings.first; n != 0; n = n->next)
     {
      Str8ListPushF(mg_arena, &layer->enums, "X(%S)\\\n", n->string);
     }
     Str8ListPush(mg_arena, &layer->enums, Str8Lit("\n\n"));
     xlist_count += 1;
    }
   }
  }
 }
 
 ///////////////////////////////
 //- rjf: generate enums
 //
 U64 enum_count = 0;
 ProfScope("generate enums") // DeferLoop(printf("generating all enums..."), printf("%i generated\n", (int)enum_count))
 {
  for(MG_MDeskFile *file = mdesk_files.first;
      file != 0;
      file = file->next)
  {
   String8 path = file->path;
   String8 layer_key = MG_LayerKeyFromPath(path);
   MG_Layer *layer = MG_LayerFromKey(layer_key);
   for(MD_EachNode(node, file->parse.root->first))
   {
    MD_Node *enum_tag = MD_TagFromString(node, Str8Lit("enum"), 0);
    if(!MD_NodeIsNil(enum_tag))
    {
     String8 base_type = enum_tag->first->string;
     if(base_type.size != 0)
     {
      Str8ListPushF(mg_arena, &layer->enums, "typedef %S %S;\n", base_type, node->string);
      Str8ListPushF(mg_arena, &layer->enums, "typedef enum %SEnum\n", node->string);
     }
     else
     {
      Str8ListPushF(mg_arena, &layer->enums, "typedef enum %S\n", node->string);
     }
     Str8ListPush(mg_arena, &layer->enums, Str8Lit("{\n"));
     String8List strings = MG_GenStringListFromNode(node);
     for(String8Node *n = strings.first; n != 0; n = n->next)
     {
      Str8ListPushF(mg_arena, &layer->enums, "%S_%S,\n", node->string, n->string);
     }
     Str8ListPush(mg_arena, &layer->enums, Str8Lit("}"));
     if(base_type.size != 0)
     {
      Str8ListPushF(mg_arena, &layer->enums, "\n%SEnum;\n\n", node->string);
     }
     else
     {
      Str8ListPushF(mg_arena, &layer->enums, "\n%S;\n\n", node->string);
     }
     enum_count += 1;
    }
   }
  }
 }
 
 ///////////////////////////////
 //- rjf: generate flags
 //
 U64 flags_count = 0;
 ProfScope("generate flags") // DeferLoop(printf("generating all flags..."), printf("%i generated\n", (int)flags_count))
 {
  for(MG_MDeskFile *file = mdesk_files.first;
      file != 0;
      file = file->next)
  {
   String8 path = file->path;
   String8 layer_key = MG_LayerKeyFromPath(path);
   MG_Layer *layer = MG_LayerFromKey(layer_key);
   for(MD_EachNode(node, file->parse.root->first))
   {
    MD_Node *enum_tag = MD_TagFromString(node, Str8Lit("flags"), 0);
    if(!MD_NodeIsNil(enum_tag))
    {
     String8 base_type = enum_tag->first->string;
     if(base_type.size == 0)
     {
      base_type = Str8Lit("U32");
     }
     Str8ListPushF(mg_arena, &layer->enums, "typedef %S %S;\n", base_type, node->string);
     Str8ListPush(mg_arena, &layer->enums, Str8Lit("enum\n{\n"));
     String8List strings = MG_GenStringListFromNode(node);
     for(String8Node *n = strings.first; n != 0; n = n->next)
     {
      Str8ListPushF(mg_arena, &layer->enums, "%S_%S,\n", Str8Chop(node->string, 1), n->string);
     }
     Str8ListPush(mg_arena, &layer->enums, Str8Lit("};\n\n"));
     flags_count += 1;
    }
   }
  }
 }
 
 ///////////////////////////////
 //- rjf: generate structs
 //
 U64 struct_count = 0;
 ProfScope("generate structs") // DeferLoop(printf("generating all structs..."), printf("%i generated\n", (int)struct_count))
 {
  for(MG_MDeskFile *file = mdesk_files.first;
      file != 0;
      file = file->next)
  {
   String8 path = file->path;
   String8 layer_key = MG_LayerKeyFromPath(path);
   MG_Layer *layer = MG_LayerFromKey(layer_key);
   for(MD_EachNode(node, file->parse.root->first))
   {
    MD_Node *struct_tag = MD_TagFromString(node, Str8Lit("struct"), 0);
    if(!MD_NodeIsNil(struct_tag))
    {
     Str8ListPushF(mg_arena, &layer->enums, "typedef struct %S %S;\n", node->string, node->string);
     Str8ListPushF(mg_arena, &layer->enums, "struct %S\n", node->string);
     Str8ListPush(mg_arena, &layer->enums, Str8Lit("{\n"));
     String8List strings = MG_GenStringListFromNode(node);
     for(String8Node *n = strings.first; n != 0; n = n->next)
     {
      Str8ListPushF(mg_arena, &layer->enums, "%S;\n", n->string);
     }
     Str8ListPush(mg_arena, &layer->enums, Str8Lit("};\n\n"));
     struct_count += 1;
    }
   }
  }
 }
 
 ///////////////////////////////
 //- rjf: generate data tables
 //
 U64 data_table_count = 0;
 ProfScope("generate data tables") // DeferLoop(printf("generating all data tables..."), printf("%i generated\n", (int)data_table_count))
 {
  for(MG_MDeskFile *file = mdesk_files.first;
      file != 0;
      file = file->next)
  {
   String8 path = file->path;
   String8 layer_key = MG_LayerKeyFromPath(path);
   MG_Layer *layer = MG_LayerFromKey(layer_key);
   for(MD_EachNode(node, file->parse.root->first))
   {
    MD_Node *data_table_tag = MD_TagFromString(node, Str8Lit("data"), 0);
    if(!MD_NodeIsNil(data_table_tag))
    {
     B32 is_core = MD_NodeHasTag(node, Str8Lit("core"), 0);
     String8 h_decl_specifier = is_core ? Str8Lit("root_global") : Str8Lit("extern");
     String8 type = data_table_tag->first->string;
     String8List strings = MG_GenStringListFromNode(node);
     Str8ListPushF(mg_arena, &layer->h_tables, "%S %S %S[%I64u];\n\n", h_decl_specifier, type, node->string, strings.node_count);
     if(is_core)
     {
      Str8ListPushF(mg_arena, &layer->c_tables, "#if !BUILD_SUPPLEMENTARY_UNIT\n");
     }
     Str8ListPushF(mg_arena, &layer->c_tables, "%S %S[%I64u] =\n{\n", type, node->string, strings.node_count);
     for(String8Node *n = strings.first; n != 0; n = n->next)
     {
      Str8ListPushF(mg_arena, &layer->c_tables, "%S,\n", n->string);
     }
     Str8ListPushF(mg_arena, &layer->c_tables, "};\n\n");
     if(is_core)
     {
      Str8ListPushF(mg_arena, &layer->c_tables, "#endif // !BUILD_SUPPLEMENTARY_UNIT\n");
     }
     data_table_count += 1;
    }
   }
  }
 }
 
 ///////////////////////////////
 //- rjf: generate gens
 //
 U64 gen_count = 0;
 ProfScope("generate gens") // DeferLoop(printf("generating all gens..."), printf("%i generated\n", (int)gen_count))
 {
  for(MG_MDeskFile *file = mdesk_files.first;
      file != 0;
      file = file->next)
  {
   String8 path = file->path;
   String8 layer_key = MG_LayerKeyFromPath(path);
   MG_Layer *layer = MG_LayerFromKey(layer_key);
   for(MD_EachNode(node, file->parse.root->first))
   {
    MD_Node *gen_tag = MD_TagFromString(node, Str8Lit("gen"), 0);
    if(!MD_NodeIsNil(gen_tag))
    {
     B32 c_file = MD_NodeHasTag(node, Str8Lit("c"), 0);
     String8List strings = MG_GenStringListFromNode(node);
     String8List *dst = c_file ? &layer->c_catchall : &layer->h_catchall;
     if(Str8Match(gen_tag->first->string, Str8Lit("enums"), 0))
     {
      dst = &layer->enums;
     }
     if(Str8Match(gen_tag->first->string, Str8Lit("structs"), 0))
     {
      dst = &layer->structs;
     }
     if(Str8Match(gen_tag->first->string, Str8Lit("functions"), 0))
     {
      dst = c_file ? &layer->c_functions : &layer->h_functions;
     }
     if(Str8Match(gen_tag->first->string, Str8Lit("data_tables"), 0))
     {
      dst = c_file ? &layer->c_tables : &layer->h_tables;
     }
     for(String8Node *n = strings.first; n != 0; n = n->next)
     {
      Str8ListPushF(mg_arena, dst, "%S\n", n->string);
     }
     gen_count += 1;
    }
   }
  }
 }
 
 ///////////////////////////////
 //- rjf: generate string embeds
 //
 U64 string_embed_count = 0;
 ProfScope("generate string embeds") // DeferLoop(printf("generating all string embeds..."), printf("%i generated\n", (int)string_embed_count))
 {
  for(MG_MDeskFile *file = mdesk_files.first; file != 0; file = file->next)
  {
   String8 path = file->path;
   String8 layer_key = MG_LayerKeyFromPath(path);
   MG_Layer *layer = MG_LayerFromKey(layer_key);
   for(MD_EachNode(node, file->parse.root->first))
   {
    if(MD_NodeHasTag(node, Str8Lit("embed_string"), 0))
    {
		String8 layer_key = MG_LayerKeyFromPath(path);
        MG_Layer *layer = MG_LayerFromKey(layer_key);
        String8 embed_string = MG_CStringLiteralFromMultilineString(node->first->string);
        // @Changed(rad_names) Generated code is compiled against raddebugger's
        // base now, so it uses its names: str8_lit_comp, not bgz's Str8LitComp
        // (also in the embed and tweak tables below).
        Str8ListPushF(mg_arena, &layer->h_tables, "read_only global String8 %S =\nstr8_lit_comp(\n", node->string);
        Str8ListPush(mg_arena, &layer->h_tables, embed_string);
        Str8ListPushF(mg_arena, &layer->h_tables, ");\n\n");
     	string_embed_count += 1;
    }
   }
  }
 }
 
 ///////////////////////////////
 //- rjf: generate markup embed code
 //
 ProfScope("generate markup embeds")
 {
  //- rjf: generate per-layer embed data
  for(MG_Markup *embed = embeds.first; embed != 0; embed = embed->next)
  {
	  String8 name = embed->root->first->string;
	  String8 path = embed->root->first->next->string;
	  String8 absolute_path = PushStr8F(mg_arena, "%S/%S", project_path, path);
	  String8 data = OS_DataFromFilePath(mg_arena, absolute_path);
	  XXH128_hash_t hash = XXH3_128bits(data.str, (size_t)data.size);
	  String8 data_textified = MG_CArrayLiteralContentsFromData(data);
	  Str8ListPushF(mg_arena, &embed->layer->h_tables, "extern String8 %S;\n", name);
	  Str8ListPushF(mg_arena, &embed->layer->h_tables, "extern U8 %S_data[%I64u];\n", name, data.size);
	  Str8ListPushF(mg_arena, &embed->layer->h_tables, "extern U128 %S_hash;\n", name);
	  Str8ListPushF(mg_arena, &embed->layer->c_tables, "read_only String8 %S = {%S_data, sizeof(%S_data)};\n\n", name, name, name);
	  Str8ListPushF(mg_arena, &embed->layer->c_tables, "read_only U8 %S_data[%I64u] =\n{\n", name, data.size);
	  Str8ListPush (mg_arena, &embed->layer->c_tables, data_textified);
	  Str8ListPushF(mg_arena, &embed->layer->c_tables, "};\n\n");
	  Str8ListPushF(mg_arena, &embed->layer->c_tables, "read_only U128 %S_hash = {0x%I64x, 0x%I64x};\n\n", name, (U64)hash.low64, (U64)hash.high64);
  }

  //- rjf: generate per-layer embed tables
  for(U64 slot_idx = 0; slot_idx < mg_state->layer_slots_count; slot_idx += 1)
  {
   for(MG_LayerNode *n = mg_state->layer_slots[slot_idx].first;
       n != 0;
       n = n->next)
   {
    MG_Layer *layer = &n->v;
    if(layer->embed_count)
    {
     String8 layer_key = layer->key;
     String8 split = Str8Lit("/");
     String8List layer_name_parts = Str8SplitByStrings(mg_arena, layer_key, 1, &split);
     StringJoin join = {0};
     join.sep = Str8Lit("_");
     String8 layer_name = Str8ListJoin(mg_arena, layer_name_parts, &join);
     B32 first = 1;
     for(MG_Markup *embed = embeds.first; embed != 0; embed = embed->next)
     {
      if(embed->layer != layer) {continue;}
      if(first)
      {
       first = 0;
       Str8ListPushF(mg_arena, &layer->c_tables, "global read_only EmbedInfo %S_embed_infos[] =\n", layer_name);
       Str8ListPushF(mg_arena, &layer->c_tables, "{\n");
      }
      Str8ListPushF(mg_arena, &layer->c_tables, "{ str8_lit_comp(\"%S\"), &%S, &%S_hash },\n", embed->root->first->string, embed->root->first->string, embed->root->first->string);
     }
     if(!first)
     {
      Str8ListPushF(mg_arena, &layer->c_tables, "};\n\n");
     }
     Str8ListPushF(mg_arena, &layer->h_tables, "extern EmbedInfoTable %S_embed_table;\n", layer_name, embeds.count);
     Str8ListPushF(mg_arena, &layer->c_tables, "EmbedInfoTable %S_embed_table = {%S_embed_infos, ArrayCount(%S_embed_infos)};\n", layer_name, layer_name, layer_name);
    }
   }
  }
  
  //- rjf: generate global embed table
  if(embeds.count != 0)
  {
   MG_Layer *base_layer = MG_LayerFromKey(Str8Lit("base"));
   Str8ListPushF(mg_arena, &base_layer->h_tables, "extern EmbedInfoTable **embed_tables;\n");
   Str8ListPushF(mg_arena, &base_layer->h_tables, "extern U64 embed_tables_count;\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "EmbedInfoTable *embed_tables_data[] =\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "{\n");
   for(U64 slot_idx = 0; slot_idx < mg_state->layer_slots_count; slot_idx += 1)
   {
    for(MG_LayerNode *n = mg_state->layer_slots[slot_idx].first;
        n != 0;
        n = n->next)
    {
     MG_Layer *layer = &n->v;
     String8List layer_key_parts = PathPartsFromStr8(mg_arena, layer->key);
     StringJoin join = {0};
     join.sep = Str8Lit("_");
     String8 layer_name = Str8ListJoin(mg_arena, layer_key_parts, &join);
     String8 layer_name_upper = UpperFromStr8(mg_arena, layer_name);
     if(layer->embed_count != 0)
     {
      Str8ListPushF(mg_arena, &base_layer->c_tables, "#if defined(GENERATED_%S_META_H)\n", layer_name_upper);
      Str8ListPushF(mg_arena, &base_layer->c_tables, "&%S_embed_table,\n", layer_name);
      Str8ListPushF(mg_arena, &base_layer->c_tables, "#endif\n");
     }
    }
   }
   Str8ListPushF(mg_arena, &base_layer->c_tables, "0,\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "};\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "EmbedInfoTable **embed_tables = embed_tables_data;\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "U64 embed_tables_count = ArrayCount(embed_tables_data)-1;\n");
  }
 }
 
 ///////////////////////////////
 //- rjf: generate markup tweak code
 //
 ProfScope("generate markup tweaks") // DeferLoop(printf("generating all tweaks..."), printf("done\n"))
 {
  //- rjf: generate TweakB32 states
  for(MG_Markup *tweak = tweak_b32s.first; tweak != 0; tweak = tweak->next)
  {
   Str8ListPushF(mg_arena, &tweak->layer->h_tables, "extern B32 TWEAK_%S;\n", tweak->root->first->string);
   Str8ListPushF(mg_arena, &tweak->layer->c_tables, "B32 TWEAK_%S = %S;\n", tweak->root->first->string, tweak->root->first->next->string);
  }
  
  //- rjf: generate TweakF32 states
  for(MG_Markup *tweak = tweak_f32s.first; tweak != 0; tweak = tweak->next)
  {
   Str8ListPushF(mg_arena, &tweak->layer->h_tables, "extern F32 TWEAK_%S;\n", tweak->root->first->string);
   Str8ListPushF(mg_arena, &tweak->layer->c_tables, "F32 TWEAK_%S = %S;\n", tweak->root->first->string, tweak->root->first->next->string);
  }
  
  //- rjf: generate per-layer TweakB32 tables
  for(U64 slot_idx = 0; slot_idx < mg_state->layer_slots_count; slot_idx += 1)
  {
   for(MG_LayerNode *n = mg_state->layer_slots[slot_idx].first;
       n != 0;
       n = n->next)
   {
    MG_Layer *layer = &n->v;
    if(layer->tweak_b32_count)
    {
     String8 layer_key = layer->key;
     String8 split = Str8Lit("/");
     String8List layer_name_parts = Str8SplitByStrings(mg_arena, layer_key, 1, &split);
     StringJoin join = {0};
     join.sep = Str8Lit("_");
     String8 layer_name = Str8ListJoin(mg_arena, layer_name_parts, &join);
     B32 first = 1;
     for(MG_Markup *tweak = tweak_b32s.first; tweak != 0; tweak = tweak->next)
     {
      if(tweak->layer != layer) {continue;}
      if(first)
      {
       first = 0;
       Str8ListPushF(mg_arena, &layer->c_tables, "global read_only TweakB32Info %S_tweak_b32s[] =\n", layer_name);
       Str8ListPushF(mg_arena, &layer->c_tables, "{\n");
      }
      Str8ListPushF(mg_arena, &layer->c_tables, "{ str8_lit_comp(\"%S\"), %S, &TWEAK_%S },\n", tweak->root->first->string, tweak->root->first->next->string, tweak->root->first->string);
     }
     if(!first)
     {
      Str8ListPushF(mg_arena, &layer->c_tables, "};\n\n");
     }
     Str8ListPushF(mg_arena, &layer->h_tables, "extern TweakB32InfoTable %S_tweak_b32_table;\n", layer_name, tweak_b32s.count);
     Str8ListPushF(mg_arena, &layer->c_tables, "TweakB32InfoTable %S_tweak_b32_table = {%S_tweak_b32s, ArrayCount(%S_tweak_b32s)};\n", layer_name, layer_name, layer_name);
    }
   }
  }
  
  //- rjf: generate per-layer TweakF32 tables
  for(U64 slot_idx = 0; slot_idx < mg_state->layer_slots_count; slot_idx += 1)
  {
   for(MG_LayerNode *n = mg_state->layer_slots[slot_idx].first;
       n != 0;
       n = n->next)
   {
    MG_Layer *layer = &n->v;
    if(layer->tweak_f32_count)
    {
     String8 layer_key = layer->key;
     String8 split = Str8Lit("/");
     String8List layer_name_parts = Str8SplitByStrings(mg_arena, layer_key, 1, &split);
     StringJoin join = {0};
     join.sep = Str8Lit("_");
     String8 layer_name = Str8ListJoin(mg_arena, layer_name_parts, &join);
     F32 first = 1;
     for(MG_Markup *tweak = tweak_f32s.first; tweak != 0; tweak = tweak->next)
     {
      if(tweak->layer != layer) {continue;}
      if(first)
      {
       first = 0;
       Str8ListPushF(mg_arena, &layer->c_tables, "global read_only TweakF32Info %S_tweak_f32s[] =\n", layer_name);
       Str8ListPushF(mg_arena, &layer->c_tables, "{\n");
      }
      Str8ListPushF(mg_arena, &layer->c_tables, "{ str8_lit_comp(\"%S\"), %S, {%S, %S}, &TWEAK_%S },\n", tweak->root->first->string, tweak->root->first->next->string, tweak->root->first->next->next->string, tweak->root->first->next->next->next->string, tweak->root->first->string);
     }
     if(!first)
     {
      Str8ListPushF(mg_arena, &layer->c_tables, "};\n\n");
     }
     Str8ListPushF(mg_arena, &layer->h_tables, "extern TweakF32InfoTable %S_tweak_f32_table;\n", layer_name, tweak_f32s.count);
     Str8ListPushF(mg_arena, &layer->c_tables, "TweakF32InfoTable %S_tweak_f32_table = {%S_tweak_f32s, ArrayCount(%S_tweak_f32s)};\n", layer_name, layer_name, layer_name);
    }
   }
  }
  
  //- rjf: generate global tweak b32 table
  if(tweak_b32s.count != 0)
  {
   MG_Layer *base_layer = MG_LayerFromKey(Str8Lit("base"));
   Str8ListPushF(mg_arena, &base_layer->h_tables, "extern TweakB32InfoTable **tweak_b32_tables;\n");
   Str8ListPushF(mg_arena, &base_layer->h_tables, "extern U64 tweak_b32_tables_count;\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "TweakB32InfoTable *tweak_b32_tables_data[] =\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "{\n");
   for(U64 slot_idx = 0; slot_idx < mg_state->layer_slots_count; slot_idx += 1)
   {
    for(MG_LayerNode *n = mg_state->layer_slots[slot_idx].first;
        n != 0;
        n = n->next)
    {
     MG_Layer *layer = &n->v;
     String8List layer_key_parts = PathPartsFromStr8(mg_arena, layer->key);
     StringJoin join = {0};
     join.sep = Str8Lit("_");
     String8 layer_name = Str8ListJoin(mg_arena, layer_key_parts, &join);
     String8 layer_name_upper = UpperFromStr8(mg_arena, layer_name);
     if(layer->tweak_b32_count != 0)
     {
      Str8ListPushF(mg_arena, &base_layer->c_tables, "#if defined(GENERATED_%S_META_H)\n", layer_name_upper);
      Str8ListPushF(mg_arena, &base_layer->c_tables, "&%S_tweak_b32_table,\n", layer_name);
      Str8ListPushF(mg_arena, &base_layer->c_tables, "#endif\n");
     }
    }
   }
   Str8ListPushF(mg_arena, &base_layer->c_tables, "0,\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "};\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "TweakB32InfoTable **tweak_b32_tables = tweak_b32_tables_data;\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "U64 tweak_b32_tables_count = ArrayCount(tweak_b32_tables_data)-1;\n");
  }
  
  //- rjf: generate global tweak f32 table
  if(tweak_f32s.count != 0)
  {
   MG_Layer *base_layer = MG_LayerFromKey(Str8Lit("base"));
   Str8ListPushF(mg_arena, &base_layer->h_tables, "extern TweakF32InfoTable **tweak_f32_tables;\n");
   Str8ListPushF(mg_arena, &base_layer->h_tables, "extern U64 tweak_f32_tables_count;\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "TweakF32InfoTable *tweak_f32_tables_data[] =\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "{\n");
   for(U64 slot_idx = 0; slot_idx < mg_state->layer_slots_count; slot_idx += 1)
   {
    for(MG_LayerNode *n = mg_state->layer_slots[slot_idx].first;
        n != 0;
        n = n->next)
    {
     MG_Layer *layer = &n->v;
     String8List layer_key_parts = PathPartsFromStr8(mg_arena, layer->key);
     StringJoin join = {0};
     join.sep = Str8Lit("_");
     String8 layer_name = Str8ListJoin(mg_arena, layer_key_parts, &join);
     String8 layer_name_upper = UpperFromStr8(mg_arena, layer_name);
     if(layer->tweak_f32_count != 0)
     {
      Str8ListPushF(mg_arena, &base_layer->c_tables, "#if defined(GENERATED_%S_META_H)\n", layer_name_upper);
      Str8ListPushF(mg_arena, &base_layer->c_tables, "&%S_tweak_f32_table,\n", layer_name);
      Str8ListPushF(mg_arena, &base_layer->c_tables, "#endif\n");
     }
    }
   }
   Str8ListPushF(mg_arena, &base_layer->c_tables, "0,\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "};\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "TweakF32InfoTable **tweak_f32_tables = tweak_f32_tables_data;\n");
   Str8ListPushF(mg_arena, &base_layer->c_tables, "U64 tweak_f32_tables_count = ArrayCount(tweak_f32_tables_data)-1;\n");
  }
 }
 
 ///////////////////////////////
 //- rjf: bake all files
 //
 U64 bake_count = 0;
 ProfScope("bake all files") // DeferLoop(printf("baking all files..."), printf("%i baked\n", (int)bake_count))
 {
  for(U64 slot_idx = 0; slot_idx < mg_state->layer_slots_count; slot_idx += 1)
  {
   for(MG_LayerNode *n = mg_state->layer_slots[slot_idx].first;
       n != 0;
       n = n->next)
   {
    MG_Layer *layer = &n->v;
    B32 needs_bake = (layer->enums.node_count != 0 ||
                      layer->structs.node_count != 0 ||
                      layer->h_functions.node_count != 0 ||
                      layer->h_tables.node_count != 0 ||
                      layer->h_catchall.node_count != 0 ||
                      layer->c_functions.node_count != 0 ||
                      layer->c_tables.node_count != 0 ||
                      layer->c_catchall.node_count != 0);
    if(!needs_bake)
    {
     continue;
    }
    String8 layer_generated_folder = PushStr8F(mg_arena, "%S/%S/generated", code_path, layer->key);
    if(OS_MakeDirectory(layer_generated_folder))
    {
     String8List layer_key_parts = PathPartsFromStr8(mg_arena, layer->key);
     StringJoin join = {0};
     join.sep = Str8Lit("_");
     String8 layer_key_filename = Str8ListJoin(mg_arena, layer_key_parts, &join);
     String8 layer_key_filename_upper = UpperFromStr8(mg_arena, layer_key_filename);
     String8 h_path = PushStr8F(mg_arena, "%S/%S.meta.h", layer_generated_folder, layer_key_filename);
     String8 c_path = PushStr8F(mg_arena, "%S/%S.meta.c", layer_generated_folder, layer_key_filename);
     {
      FILE *h = fopen((char *)h_path.str, "w");
      fprintf(h, "// The Digital Grove Codebase\n");
      fprintf(h, "// Copyright (c) 2024 Ryan Fleury. All rights reserved.\n\n");
      fprintf(h, "//- GENERATED CODE\n\n");
      fprintf(h, "#ifndef GENERATED_%.*s_META_H\n", Str8VArg(layer_key_filename_upper));
      fprintf(h, "#define GENERATED_%.*s_META_H\n\n", Str8VArg(layer_key_filename_upper));
      for(String8Node *n = layer->enums.first; n != 0; n = n->next)
      {
       fwrite(n->string.str, n->string.size, 1, h);
      }
      for(String8Node *n = layer->structs.first; n != 0; n = n->next)
      {
       fwrite(n->string.str, n->string.size, 1, h);
      }
      for(String8Node *n = layer->h_catchall.first; n != 0; n = n->next)
      {
       fwrite(n->string.str, n->string.size, 1, h);
      }
      for(String8Node *n = layer->h_functions.first; n != 0; n = n->next)
      {
       fwrite(n->string.str, n->string.size, 1, h);
      }
      for(String8Node *n = layer->h_tables.first; n != 0; n = n->next)
      {
       fwrite(n->string.str, n->string.size, 1, h);
      }
      fprintf(h, "\n#endif // GENERATED_%.*s_META_H\n", Str8VArg(layer_key_filename_upper));
      fflush(h);
      fclose(h);
      bake_count += 1;
     }
     {
      FILE *c = fopen((char *)c_path.str, "w");
      fprintf(c, "// The Digital Grove Codebase\n");
      fprintf(c, "// Copyright (c) 2024 Ryan Fleury. All rights reserved.\n\n");
      fprintf(c, "//- GENERATED CODE\n\n");
      for(String8Node *n = layer->c_catchall.first; n != 0; n = n->next)
      {
       fwrite(n->string.str, n->string.size, 1, c);
      }
      for(String8Node *n = layer->c_functions.first; n != 0; n = n->next)
      {
       fwrite(n->string.str, n->string.size, 1, c);
      }
      for(String8Node *n = layer->c_tables.first; n != 0; n = n->next)
      {
       fwrite(n->string.str, n->string.size, 1, c);
      }
      fflush(c);
      fclose(c);
      bake_count += 1;
     }
    }
   }
  }
 }
}
