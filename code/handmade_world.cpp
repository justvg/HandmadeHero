#if !defined(HANDMADE_WORLD_CPP)
#define HANDMADE_WORLD_CPP

// TODO(george): Think about what ther real safe margin is
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILES_PER_CHUNK 8

#define TILE_CHUNK_UNINITIALIZED INT32_MAX

inline world_position
NullPosition()
{
    world_position Result = {};

    Result.ChunkX = TILE_CHUNK_UNINITIALIZED;

    return(Result);
}

inline bool32
IsValid(world_position P)
{
    bool32 Result = (P.ChunkX != TILE_CHUNK_UNINITIALIZED);
    return(Result);
}

inline bool32
IsValid(world_position *P)
{
    bool32 Result = (P->ChunkX != TILE_CHUNK_UNINITIALIZED);
    return(Result);
}

inline bool32
IsCanonical(real32 ChunkDim, real32 TileRel)
{
    // TODO(george): Fix floating point math so this can be exact?
    real32 Epsilon = 0.01f;
    bool32 Result = (TileRel >= -(0.5f*ChunkDim + Epsilon)) && (TileRel <= (0.5f*ChunkDim + Epsilon));

    return(Result);
}

inline bool32
IsCanonical(world *World, v3 Offset)
{
    bool32 Result = (IsCanonical(World->ChunkDimInMeters.x, Offset.x) && 
                     IsCanonical(World->ChunkDimInMeters.y, Offset.y) &&
                     IsCanonical(World->ChunkDimInMeters.z, Offset.z));

    return(Result);
}

inline void
ClearWorldEntityBlock(world_entity_block *Block)
{
    Block->EntityCount = 0;
    Block->EntityDataSize = 0;
    Block->Next = 0;
}

inline world_chunk * 
GetWorldChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
             memory_arena *Arena = 0)
{
    Assert(ChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ < TILE_CHUNK_SAFE_MARGIN);

    // TODO(george): Better hash function!
    uint32 HashValue = 19*ChunkX + 7*ChunkY + 3*ChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1); 
    Assert(HashSlot < ArrayCount(World->ChunkHash));

    world_chunk *Result = 0;
    for(world_chunk *Chunk = World->ChunkHash[HashSlot];
        Chunk;
        Chunk = Chunk->NextInHash)
    {
        if((ChunkX == Chunk->ChunkX) &&
           (ChunkY == Chunk->ChunkY) && 
           (ChunkZ == Chunk->ChunkZ))
        {
            Result = Chunk;
            break;
        }
    }


    if(!Result && Arena)
    {
        Result = PushStruct(Arena, world_chunk, NoClear());
        ClearWorldEntityBlock(&Result->FirstBlock);
        Result->ChunkX = ChunkX;
        Result->ChunkY = ChunkY;
        Result->ChunkZ = ChunkZ;

        Result->NextInHash = World->ChunkHash[HashSlot];
        World->ChunkHash[HashSlot] = Result;
    }
    
    return(Result);
}

internal world *
CreateWorld(v3 ChunkDimInMeters, memory_arena *ParentArena)
{
    world *World = PushStruct(ParentArena, world);

    World->ChunkDimInMeters = ChunkDimInMeters;
    World->FirstFree = 0;
    SubArena(&World->Arena, ParentArena, GetArenaSizeRemaining(ParentArena), NoClear());

    return(World);
}

inline void
RecanonicalizeCoord(real32 ChunkDim, int32 *Tile, real32 *TileRel)
{
    // TODO(george): Need t something that doesn't use the div/mul method
    // for recanonicalizing because this can end up rounding back on to the tile
    // you just] came from.

    // NOTE(george): Wrapping IS NOT ALLOWED, so all coordinates are assumed to be
    // within the safe margin!
    // TODO(george): Assert that we are nowhere near the edges of the world.
    
    int32 Offset = RoundReal32ToInt32(*TileRel / ChunkDim); 
    *Tile += Offset;
    *TileRel -= Offset*ChunkDim;

    Assert(IsCanonical(ChunkDim, *TileRel));
}

inline world_position
MapIntoChunkSpace(world *World, world_position BasePos, v3 Offset)
{
    world_position Result = BasePos;
    
    Result.Offset_ += Offset;
    RecanonicalizeCoord(World->ChunkDimInMeters.x, &Result.ChunkX, &Result.Offset_.x);
    RecanonicalizeCoord(World->ChunkDimInMeters.y, &Result.ChunkY, &Result.Offset_.y);
    RecanonicalizeCoord(World->ChunkDimInMeters.z, &Result.ChunkZ, &Result.Offset_.z);
    
    return(Result);
}

inline bool32
AreInTheSameChunk(world *World, world_position *A, world_position *B)
{
    Assert(IsCanonical(World, A->Offset_));
    Assert(IsCanonical(World, B->Offset_));
    bool32 Result = ((A->ChunkX == B->ChunkX) &&
                     (A->ChunkY == B->ChunkY) &&
                     (A->ChunkZ == B->ChunkZ));

    return(Result);
}

inline v3
Substract(world *World, world_position *A, world_position *B)
{

    v3 dTile = {(real32)A->ChunkX - (real32)B->ChunkX,
                (real32)A->ChunkY - (real32)B->ChunkY,
                (real32)A->ChunkZ - (real32)B->ChunkZ};

    v3 Result = Hadamard(World->ChunkDimInMeters, dTile) + (A->Offset_ - B->Offset_);

    return(Result);
}

inline world_position
CenteredChunkPoint(uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ)
{
    world_position Result = {};

    Result.ChunkX = ChunkX;
    Result.ChunkY = ChunkY;
    Result.ChunkZ = ChunkZ;    

    return(Result);
}

inline world_position
CenteredChunkPoint(world_chunk *Chunk)
{
    world_position Result = CenteredChunkPoint(Chunk->ChunkX, Chunk->ChunkY, Chunk->ChunkZ);

    return(Result);
}

internal void
ChangeEntityLocationRaw(memory_arena *Arena, world *World, uint32 LowEntityIndex, 
                     world_position *OldP, world_position *NewP)
{
    // TODO(george): If this moves an entity into the camera bounds, should it automatically
    // go into the high set immediately?
    // If it moves _out_ of the camera bounds, should it be removed from the high set 
    // immediately?

    Assert(!OldP || IsValid(OldP));
    Assert(!NewP || IsValid(NewP));

    if(OldP && NewP && AreInTheSameChunk(World, OldP, NewP))
    {
        // NOTE(george): Leave entity where it is
    }
    else
    {
        if(OldP)
        {
            // NOTE(george): Pull the entity out of its current entity block
            world_chunk *Chunk = GetWorldChunk(World, OldP->ChunkX, OldP->ChunkY, OldP->ChunkZ);
            Assert(Chunk);
            if(Chunk)
            {
                bool32 NotFound = true;
                world_entity_block *FirstBlock = &Chunk->FirstBlock;
                for(world_entity_block *Block = FirstBlock; Block && NotFound; Block = Block->Next)
                {
                    for(uint32 Index = 0; (Index < Block->EntityCount) && NotFound; Index++)
                    {
                        if(Block->LowEntityIndex[Index] == LowEntityIndex)
                        {
                            Assert(FirstBlock->EntityCount > 0);
                            Block->LowEntityIndex[Index] = FirstBlock->LowEntityIndex[--FirstBlock->EntityCount];
                            if(FirstBlock->EntityCount == 0)
                            {
                                if(FirstBlock->Next)
                                {
                                    world_entity_block *NextBlock = FirstBlock->Next;
                                    *FirstBlock = *NextBlock;
                                    
                                    NextBlock->Next = World->FirstFree;
                                    World->FirstFree = NextBlock;
                                }
                            }

                            NotFound = false;
                        }
                    }
                }
            }
        }
        
        if(NewP)
        {
            // NOTE(george): Doesn't work if we insert second+ world_entity_block in the chain??

            // NOTE(george): Insert the entity into its new entity block
            world_chunk *Chunk = GetWorldChunk(World, NewP->ChunkX, NewP->ChunkY, NewP->ChunkZ, Arena);
            Assert(Chunk);

            world_entity_block *Block = &Chunk->FirstBlock;
            if(Block->EntityCount == ArrayCount(Block->LowEntityIndex))
            {
                // NOTE(george): We're out of room, get a new block!
                world_entity_block *OldBlock = World->FirstFree;    
                if(OldBlock)
                {
                    World->FirstFree = OldBlock->Next;
                }
                else
                {
                    OldBlock = PushStruct(Arena, world_entity_block);
                }
                
                *OldBlock = *Block;
                Block->Next = OldBlock;
                Block->EntityCount = 0;
            }

            Assert(Block->EntityCount < ArrayCount(Block->LowEntityIndex));
            Block->LowEntityIndex[Block->EntityCount++] = LowEntityIndex;
        }
    }
}

internal void
ChangeEntityLocation(memory_arena *Arena, world *World,
                        uint32 LowEntityIndex, low_entity *LowEntity,  
                        world_position NewPInit)
{
    world_position *OldP = 0;
    world_position *NewP = 0;

    if(!IsSet(&LowEntity->Sim, EntityFlag_Nonspatial) && IsValid(LowEntity->P))
    {
        OldP = &LowEntity->P;
    }

    if(IsValid(NewPInit))
    {
        NewP = &NewPInit;
    }

    ChangeEntityLocationRaw(Arena, World, LowEntityIndex, OldP, NewP);
    if(NewP)
    {
        LowEntity->P = *NewP;
        ClearFlags(&LowEntity->Sim, EntityFlag_Nonspatial);        
    }
    else
    {
        LowEntity->P = NullPosition();
        AddFlags(&LowEntity->Sim, EntityFlag_Nonspatial);        
    }
}

#endif