#if !defined(HANDMADE_TILE)
#define HANDMADE_TILE

// TODO(george): Think about what ther real safe margin is
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

inline tile_chunk * 
GetTileChunk(tile_map *TileMap, int32 TileChunkX, int32 TileChunkY, int32 TileChunkZ,
             memory_arena *Arena = 0)
{
    Assert(TileChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(TileChunkZ < TILE_CHUNK_SAFE_MARGIN);

    // TODO(george): Better hash function!
    uint32 HashValue = 19*TileChunkX + 7*TileChunkY + 3*TileChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(TileMap->TileChunkHash) - 1); 
    Assert(HashSlot < ArrayCount(TileMap->TileChunkHash));

    tile_chunk *TileChunk = TileMap->TileChunkHash + HashSlot;
    do
    {
        if((TileChunkX == TileChunk->TileChunkX) &&
           (TileChunkY == TileChunk->TileChunkY) && 
           (TileChunkZ == TileChunk->TileChunkZ))
        {
            break;
        }

        if(Arena && (TileChunk->TileChunkX != TILE_CHUNK_UNINITIALIZED) && (!TileChunk->NextInHash))
        {
            TileChunk->NextInHash = PushStruct(Arena, tile_chunk);
            TileChunk = TileChunk->NextInHash;
            TileChunk->TileChunkX = TILE_CHUNK_UNINITIALIZED;
        }

        if(Arena && (TileChunkX == TILE_CHUNK_UNINITIALIZED))
        {
            uint32 TileCount = TileMap->ChunkDim*TileMap->ChunkDim;

            TileChunk->TileChunkX = TileChunkX;
            TileChunk->TileChunkY = TileChunkY;
            TileChunk->TileChunkZ = TileChunkZ;

            TileChunk->Tiles = PushArray(Arena, TileCount, uint32);

            // TODO(george): Do we want to always initialize? 
            for(uint32 TileIndex = 0; TileIndex < TileCount; TileIndex++)
            {
                TileChunk->Tiles[TileIndex] = 1;
            }

            TileChunk->NextInHash = 0;

            break;
        }
        
        TileChunk = TileChunk->NextInHash;
    } while(TileChunk);
    
    return(TileChunk);
}

inline bool32
GetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, int32 TileX, int32 TileY)
{
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);

    uint32 TileChunkValue = TileChunk->Tiles[TileY*TileMap->ChunkDim + TileX];
    return(TileChunkValue);
}

inline void
SetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, int32 TileX, int32 TileY, uint32 TileValue)
{
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);

    TileChunk->Tiles[TileY*TileMap->ChunkDim + TileX] = TileValue;
}

inline void
SetTileValue(tile_map *TileMap, tile_chunk *TileChunk, uint32 TestTileX, uint32 TestTileY, uint32 TileValue)
{
    if(TileChunk && TileChunk->Tiles)
    {
        SetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY, TileValue);
    }
}

inline tile_chunk_position 
GetChunkPositionFor(tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    tile_chunk_position Result;

    Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
    Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
    Result.TileChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & TileMap->ChunkMask;
    Result.RelTileY = AbsTileY & TileMap->ChunkMask;

    return(Result);
}

inline bool32
GetTileValue(tile_map *TileMap, tile_chunk *TileChunk, uint32 TestTileX, uint32 TestTileY)
{
    uint32 TileChunkValue = 0;

    if(TileChunk && TileChunk->Tiles)
    {
        TileChunkValue = GetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY);
    }

    return(TileChunkValue);
}

inline uint32
GetTileValue(tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
    uint32 TileChunkValue = GetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);

    return(TileChunkValue);
}

inline uint32
GetTileValue(tile_map *TileMap, tile_map_position Pos)
{
    uint32 TileChunkValue = GetTileValue(TileMap, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ);

    return(TileChunkValue);
}

internal bool32
IsTileValueEmpty(uint32 TileValue)
{
    bool32 Empty = ((TileValue == 1) || 
                     (TileValue == 3) ||
                     (TileValue == 4));

    return(Empty);
}

internal bool32
IsTileMapPointEmpty(tile_map *TileMap, tile_map_position Pos)
{
    uint32 TileChunkValue = GetTileValue(TileMap, Pos);
    bool32 Empty = IsTileValueEmpty(TileChunkValue);

    return(Empty);
}

internal void
SetTileValue(memory_arena *Arena, tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ, uint32 TileValue)
{
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ, Arena);
    SetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}

internal void
InitializeTileMap(tile_map *TileMap, real32 TileSideInMeters)
{
    TileMap->ChunkShift = 4;
    TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
    TileMap->ChunkDim = (1 << TileMap->ChunkShift);
    TileMap->TileSideInMeters = TileSideInMeters;

    for(uint32 TileChunkIndex = 0; TileChunkIndex < ArrayCount(TileMap->TileChunkHash); TileChunkIndex++)
    {
        TileMap->TileChunkHash[TileChunkIndex].TileChunkX = INT32_MAX;
    }
}

//
// TODO(george): Do these really belong in more of a "positioning" or "geometry" file?
// 

inline void
RecanonicalizeCoord(tile_map *TileMap, int32 *Tile, real32 *TileOffset)
{
    // TODO(george): Need to do something that doesn't use the div/mul method
    // for recanonicalizing because this can end up rounding back on to the tile
    // you just came from.

    // NOTE(george): TileMap is assumed to be toroidal topology, if you step off one end you
    // come back on the other! 
    int32 Offset = RoundReal32ToInt32(*TileOffset / TileMap->TileSideInMeters); 
    *Tile += Offset;
    *TileOffset -= Offset*TileMap->TileSideInMeters;

    // TODO(george): Fix floating point math so this can be exact?
    Assert(*TileOffset > -0.5f*TileMap->TileSideInMeters);
    Assert(*TileOffset < 0.5f*TileMap->TileSideInMeters);
}

internal tile_map_position
MapIntoTileSpace(tile_map *TileMap, tile_map_position BasePos, v2 Offset)
{
    tile_map_position Result = BasePos;
    
    Result.Offset_ += Offset;
    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.Offset_.X);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.Offset_.Y);
    
    return(Result);
}

inline bool32
AreOnTheSameTile(tile_map_position *A, tile_map_position *B)
{
    bool32 Result = ((A->AbsTileX == B->AbsTileX) &&
                     (A->AbsTileY == B->AbsTileY) &&
                     (A->AbsTileZ == B->AbsTileZ));

    return(Result);
}

inline tile_map_difference 
Substract(tile_map *TileMap, tile_map_position *A, tile_map_position *B)
{
    tile_map_difference Result;

    v2 dTileXY = {(real32)A->AbsTileX - (real32)B->AbsTileX,
                  (real32)A->AbsTileY - (real32)B->AbsTileY};
    real32 dTileZ = (real32)A->AbsTileZ - (real32)B->AbsTileZ;

    Result.dXY = TileMap->TileSideInMeters*dTileXY + (A->Offset_ - B->Offset_);

    // TODO(george): Think about what we want to do about Z
    Result.dZ = TileMap->TileSideInMeters*dTileZ;

    return(Result);
}

inline tile_map_position
CenteredTilePoint(uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    tile_map_position Result = {};

    Result.AbsTileX = AbsTileX;
    Result.AbsTileY = AbsTileY;
    Result.AbsTileZ = AbsTileZ;    

    return(Result);
}

#endif