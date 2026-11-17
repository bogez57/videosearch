// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Generated Code

#include "generated/render.meta.c"


////////////////////////////////
//~ rjf: Helpers

U64 Core_R_BytesPerPixelFromTex2DFmt(R_Tex2DFmt fmt)
{
    U64 num_bits = 0;
    for EachIndex(channel_idx, 4)
    {
        R_ChannelSizeKind size_kind = r_size_kind_from_tex2dfmt_channel(fmt, channel_idx);
        switch(size_kind)
        {
            default:{}break;
            case R_ChannelSizeKind_2: {num_bits += 2;}break;
            case R_ChannelSizeKind_8: {num_bits += 8;}break;
            case R_ChannelSizeKind_10:{num_bits += 10;}break;
            case R_ChannelSizeKind_11:{num_bits += 11;}break;
            case R_ChannelSizeKind_16:{num_bits += 16;}break;
            case R_ChannelSizeKind_24:{num_bits += 24;}break;
            case R_ChannelSizeKind_32:{num_bits += 32;}break;
        }
    }
    U64 num_bits_rounded = num_bits+7;
    num_bits_rounded -= num_bits_rounded%8;
    U64 num_bytes = num_bits_rounded/8;
    return num_bytes;
}

Mat4x4F32 Core_R_SampleChannelMapFromTex2DFmt(R_Tex2DFmt fmt)
{
    UnusedVariable(fmt);
    Mat4x4F32 result =
    {
        {
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1},
        }
    };
    return result;
}

Mat4x4F32 Core_R_SampleChannelMapFromTex2DFormat(R_Tex2DFormat fmt)
{
    Mat4x4F32 result =
    {
        {
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1},
        }
    };
    switch(fmt)
    {
        default:{}break;
        case R_Tex2DFormat_R8:
        {
            MemoryZeroArray(result.v[0]);
            result.v[0][0] = result.v[0][1] = result.v[0][2] = result.v[0][3] = 1.f;
        }break;
    }
    return result;
}

////////////////////////////////
//~ rjf: Basic Type Functions

R_Handle Core_R_HandleZero(void)
{
    R_Handle handle = {0};
    return handle;
}

B32 Core_R_HandleMatch(R_Handle a, R_Handle b)
{
    return MemoryMatchStruct(&a, &b);
}

////////////////////////////////
//~ rjf: Batch Type Functions

R_BatchList Core_R_BatchListMake(U64 instance_size)
{
    R_BatchList list = {0};
    list.bytes_per_inst = instance_size;
    return list;
}

// @Override(Core_R_BatchListPushInst)
void* Core_R_BatchListPushInst(Arena *arena, R_BatchList *list, U64 batch_inst_cap)
{
    void *inst = 0;
    {
        R_BatchNode *n = list->last;
        if(n == 0 || n->v.byte_count+list->bytes_per_inst > n->v.byte_cap)
        {
            n = push_array(arena, R_BatchNode, 1);
            n->v.byte_cap = batch_inst_cap*list->bytes_per_inst;
            n->v.v = push_array_no_zero(arena, U8, n->v.byte_cap);
            SLLQueuePush(list->first, list->last, n);//Changed from SLLQueuePush (essentially the same thing but current core.h has the QueuePush)
            list->batch_count += 1;
        }
        inst = n->v.v + n->v.byte_count;
        n->v.byte_count += list->bytes_per_inst;
        list->byte_count += list->bytes_per_inst;
    }
    return inst;
}

////////////////////////////////
//~ rjf: Pass Type Functions

// @Override(Core_R_PassFromKind)
R_Pass* Core_R_PassFromKind(Arena *arena, R_PassList *list, R_PassKind kind)
{
    R_PassNode *n = list->last;
    if(!r_pass_kind_batch_table[kind])
    {
        n = 0;
    }
    if(n == 0 || n->v.kind != kind)
    {
        n = push_array(arena, R_PassNode, 1);
        SLLQueuePush(list->first, list->last, n);//Changed from SLLQueuePush (essentially the same thing but current core.h has the QueuePush)
        list->count += 1;
        n->v.kind = kind;
        n->v.params = push_array(arena, U8, r_pass_kind_params_size_table[kind]);
    }
    return &n->v;
}