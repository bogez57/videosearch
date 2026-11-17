// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BASE_GEO_H
#define BASE_GEO_H

////////////////////////////////////////////////////////////////
//~ rjf: Geometry Types

typedef struct VertexIndexArrayPair VertexIndexArrayPair;
struct VertexIndexArrayPair
{
 Vec3F32Array vertices;
 S32Array indices;
};

////////////////////////////////////////////////////////////////
//~ rjf: Geometry Generators

root_function VertexIndexArrayPair IcoSphereMake(Arena *arena, U64 num_subdivide_passes);

#endif //BASE_GEO_H
