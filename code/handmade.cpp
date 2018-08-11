#include "handmade.h"

#include "handmade_tile.cpp"

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{ 
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
    {
#if 0
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

#if 0
		GameState->tSine += 2.0f*Pi32*1.0f / (real32)WavePeriod;

        if (GameState->tSine > 2.0f*Pi32)
        {
            GameState->tSine -= 2.0f*Pi32;
        }
#endif
    }
}

#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;
    uint32 FileSize;     
    uint16 Reserved1;    
    uint16 Reserved2;    
    uint32 BitmapOffset; 
	uint32 Size;          
	int32 Width;         
	int32 Height;        
	uint16 Planes;        
	uint16 BitsPerPixel; 
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorsImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char *Filename)
{
    loaded_bitmap Result = {};

    // NOTE(george): Byte order in mem. is BB GG RR AA bottom up
    // In little endian -> 0xAARRGGBB
    // Casey had AA BB GG RR
    // In little endian -> 0xRRGGBBAA
    debug_read_file_result ReadResult = ReadEntireFile(Thread, Filename);
    if(ReadResult.ContentsSize != 0)
    {
        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        Result.Pixels = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;

        // NOTE(george): If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EITHER DIRECTION and
        // the height will be negative for top-down.
        // (Also, there can be compression, etc., etc... DON'T think this
        // is complete BMP loading code because it isn't!)

#if 0
        uint32 *SourceDest = Pixels;
        for(int32 Y = 0; Y < Header->Height; Y++)
        {
            for(int32 X = 0; X < Header->Width; X++)
            {
                *SourceDest = (*SourceDest >> 8) | (*SourceDest << 24);
                SourceDest++;
            }
        }
#endif
    }

    return(Result);
}

internal void
DrawRectangle(game_offscreen_buffer *Buffer, 
              real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY,
              real32 R, real32 G, real32 B)
{
    int32 MinX = RoundReal32ToInt32(RealMinX);
    int32 MinY = RoundReal32ToInt32(RealMinY);
    int32 MaxX = RoundReal32ToInt32(RealMaxX);
    int32 MaxY = RoundReal32ToInt32(RealMaxY);

    if (MinX < 0)
    {
        MinX = 0;
    }

    if (MinY < 0)
    {
        MinY = 0;
    }

    if (MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }

    if (MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }

    uint32 Color = (RoundReal32ToUInt32(R * 255.0f) << 16) | 
                   (RoundReal32ToUInt32(G * 255.0f) << 8) |
                   RoundReal32ToUInt32(B * 255.0f);

    uint8 *Row = (uint8 *)Buffer->Memory + MinX*Buffer->BytesPerPixel + MinY*Buffer->Pitch;

    for (int Y = MinY; Y < MaxY; Y++)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = MinX; X < MaxX; X++)
        {
            *Pixel++ = Color;
        }

        Row += Buffer->Pitch;
    }
}

internal void
DrawBitmap(game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap, real32 RealX, real32 RealY)
{
    int32 MinX = RoundReal32ToInt32(RealX);
    int32 MinY = RoundReal32ToInt32(RealY);
    int32 MaxX = RoundReal32ToInt32(RealX + (real32)Bitmap->Width);
    int32 MaxY = RoundReal32ToInt32(RealY + (real32)Bitmap->Height);

    if (MinX < 0)
    {
        MinX = 0;
    }

    if (MinY < 0)
    {
        MinY = 0;
    }

    if (MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }

    if (MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }

    // TODO(george): SourceRow needs to be changed based on clipping.
    uint32 *SourceRow = Bitmap->Pixels + Bitmap->Width*(Bitmap->Height - 1);
    uint8 *DestRow = (uint8 *)Buffer->Memory + MinX*Buffer->BytesPerPixel + MinY*Buffer->Pitch;
    for(int32 Y = MinY; Y < MaxY; Y++)
    {
        uint32 *Source = SourceRow;
        uint32 *Dest = (uint32 *)DestRow;
        for(int32 X = MinX; X < MaxX; X++)
        {
            *Dest++ = *Source++;
        }
        SourceRow -= Bitmap->Width;
        DestRow += Buffer->Pitch;
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == 
            (ArrayCount(Input->Controllers[0].Buttons) - 1));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    real32 PlayerHeight = 1.4f;
    real32 PlayerWidth = 0.75f*PlayerHeight;
    
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->Backdrop = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
        GameState->Hero = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/hero.bmp");

        GameState->PlayerP.AbsTileX = 1;
        GameState->PlayerP.AbsTileY = 3;
        GameState->PlayerP.AbsTileZ = 0;
        GameState->PlayerP.OffsetX = 0.5f;
        GameState->PlayerP.OffsetY = 0.5f;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *)Memory->PermanentStorage + sizeof(game_state));

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;
        World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

        tile_map *TileMap = World->TileMap;

        TileMap->ChunkShift = 4;
        TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
        TileMap->ChunkDim = (1 << TileMap->ChunkShift);

        TileMap->TileChunkCountX = 128;
        TileMap->TileChunkCountY = 128;
        TileMap->TileChunkCountZ = 2;

        TileMap->TileChunks = PushArray(&GameState->WorldArena,
                                        TileMap->TileChunkCountX*TileMap->TileChunkCountY*TileMap->TileChunkCountZ, 
                                        tile_chunk);

        TileMap->TileSideInMeters = 1.4f;

        uint32 RandomNumberIndex = 0;

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;
        uint32 ScreenX = 0;
        uint32 ScreenY = 0;
        uint32 AbsTileZ = 0;

        // TODO(george): Replace all this with real world generation!s
        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;
        for (uint32 ScreenIndex = 0; ScreenIndex < 100; ScreenIndex++)
        {
            // TODO(george): Random number generator!
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
            uint32 RandomChoice;
            if(DoorUp || DoorDown)
            {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            }
            else
            {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
            }

            bool32 CreatedZDoor = false;
            if(RandomChoice == 2)
            {
                CreatedZDoor = true;
                if(AbsTileZ == 0)
                {
                    DoorUp = true;
                }
                else
                {
                    DoorDown = true;
                }
            }
            else if(RandomChoice == 1)
            {
                DoorRight = true;                
            }
            else
            {
                DoorTop = true;                
            }

            for(uint32 TileY = 0; TileY < TilesPerHeight; TileY++)
            {
                for(uint32 TileX = 0; TileX < TilesPerWidth; TileX++)
                {
                    uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;

                    uint32 TileValue = 1;
                    if((TileX == 0) && (!DoorLeft || (TileY != TilesPerHeight/2)))
                    {
                        TileValue = 2;
                    }

                    if((TileX == TilesPerWidth - 1) && (!DoorRight || (TileY != TilesPerHeight/2)))
                    {
                        TileValue = 2;
                    }

                    if((TileY == 0) && (!DoorBottom || (TileX != TilesPerWidth/2)))
                    {
                        TileValue = 2;
                    }

                    if((TileY == TilesPerHeight - 1) && (!DoorTop || (TileX != TilesPerWidth/2)))
                    {
                        TileValue = 2;
                    }

                    if((TileX == 10) && (TileY == 6))
                    {
                        if(DoorUp)
                        {
                            TileValue = 3;
                        }
                        else if(DoorDown)
                        {
                            TileValue = 4;
                        }
                    }

                    SetTileValue(&GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, AbsTileZ, TileValue);
                }
            }

            DoorLeft = DoorRight;
            DoorBottom = DoorTop;

            if(CreatedZDoor)
            {
                if(DoorUp)
                {
                    DoorDown = true;
                    DoorUp = false;
                }
                else if(DoorDown)
                {
                    DoorUp = true;
                    DoorDown = false;
                }
            }
            else
            {
                DoorUp = false;
                DoorDown = false;
            }

            DoorRight = false;
            DoorTop = false;

            if(RandomChoice == 2)
            {
                if(AbsTileZ == 0)
                {
                    AbsTileZ = 1;
                }
                else
                {
                    AbsTileZ = 0;
                }
            }
            else if(RandomChoice == 1)
            {
                ScreenX += 1;
            }
            else
            {
                ScreenY += 1;
            }
        }
    

        Memory->IsInitialized = true;
    }

    world *World = GameState->World;
    tile_map *TileMap = World->TileMap;

    int32 TileSideInPixels = 60;
    real32 MetersToPixels = (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;

    real32 LowerLeftX = -(real32)TileSideInPixels/2.0f;
    real32 LowerLeftY = (real32)Buffer->Height;  

    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++)
    {
        game_controller_input *Controller = &Input->Controllers[ControllerIndex];
        if(Controller->IsAnalog)
        {
            // NOTE(george): Use analog movement tuning
        }
        else
        {
            // NOTE(george): Use digital movement tuning
            real32 dPlayerX = 0.0f; 
            real32 dPlayerY = 0.0f; 

            if(Controller->MoveUp.EndedDown)
            {
                dPlayerY = 1.0f;
            }
            if(Controller->MoveDown.EndedDown)
            {
                dPlayerY = -1.0f;
            }
            if(Controller->MoveLeft.EndedDown)
            {
                dPlayerX = -1.0f;
            }
            if(Controller->MoveRight.EndedDown)
            {
                dPlayerX = 1.0f;
            }
            real32 PlayerSpeed = 3.0f;
            if(Controller->ActionUp.EndedDown)
            {
                PlayerSpeed = 10.0f;
            }

            dPlayerX *= PlayerSpeed;
            dPlayerY *= PlayerSpeed;

            // TODO(george): Diagonal will be faster! Fix once we have vectors!
            tile_map_position NewPlayerP = GameState->PlayerP;
            NewPlayerP.OffsetX += Input->dtForFrame * dPlayerX;
            NewPlayerP.OffsetY += Input->dtForFrame * dPlayerY;
            NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);
            // TODO(george): Delta function that auto-recanonicalizes

            // We should use NewPlayerX and Y!
            tile_map_position PlayerLeft = NewPlayerP;
            PlayerLeft.OffsetX -= 0.5f*PlayerWidth;
            PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);
            
            tile_map_position PlayerRight = NewPlayerP;
            PlayerRight.OffsetX += 0.5f*PlayerWidth;            
            PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);
            
            if(IsTileMapPointEmpty(TileMap, NewPlayerP) &&
               IsTileMapPointEmpty(TileMap, PlayerLeft) &&
               IsTileMapPointEmpty(TileMap, PlayerRight))
            {
                if(!AreOnTheSameTile(&GameState->PlayerP, &NewPlayerP))
                {
                    uint32 NewTileValue = GetTileValue(TileMap, NewPlayerP);

                    if(NewTileValue == 3)
                    {
                        NewPlayerP.AbsTileZ++;
                    }
                    else if(NewTileValue == 4)
                    {
                        NewPlayerP.AbsTileZ--;
                    }
                }

                GameState->PlayerP = NewPlayerP;
            }
        }
    }

    DrawBitmap(Buffer, &GameState->Backdrop, 0, 0);

    real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
    real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

    for (int32 RelRow = -10; RelRow < 10; RelRow++)
    {
        for (int32 RelColumn = -20; RelColumn < 20; RelColumn++)
        {
            uint32 Column = GameState->PlayerP.AbsTileX + RelColumn;
            uint32 Row = GameState->PlayerP.AbsTileY + RelRow;

            uint32 TileID = GetTileValue(TileMap, Column, Row, GameState->PlayerP.AbsTileZ); 
            if(TileID > 1)
            {
                real32 Gray = 0.5f;
                if(TileID == 2)
                {
                    Gray = 1.0f;
                }

                if(TileID > 2)
                {
                    Gray = 0.25f;
                }
                
                if((Row == GameState->PlayerP.AbsTileY) && (Column == GameState->PlayerP.AbsTileX))
                {
                    Gray = 0.0f;
                }

                real32 CenX = ScreenCenterX - MetersToPixels*GameState->PlayerP.OffsetX + (real32)RelColumn * TileSideInPixels;
                real32 CenY = ScreenCenterY + MetersToPixels*GameState->PlayerP.OffsetY - (real32)RelRow * TileSideInPixels;
                real32 MinX = CenX - 0.5f*TileSideInPixels;
                real32 MinY = CenY - 0.5f*TileSideInPixels;
                real32 MaxX = CenX + 0.5f*TileSideInPixels;
                real32 MaxY = CenY + 0.5f*TileSideInPixels;

                DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
            }
        }
    }

    real32 PlayerR = 1.0f;
    real32 PlayerG = 1.0f;
    real32 PlayerB = 0.0f;
    real32 PlayerLeft = ScreenCenterX - (0.5f*MetersToPixels*PlayerWidth);
    real32 PlayerTop = ScreenCenterY - MetersToPixels*PlayerHeight;
   /* DrawRectangle(Buffer, 
                  PlayerLeft, PlayerTop, 
                  PlayerLeft + MetersToPixels*PlayerWidth, 
                  PlayerTop + MetersToPixels*PlayerHeight, 
                  PlayerR, PlayerG, PlayerB);*/
    DrawBitmap(Buffer, &GameState->Hero, PlayerLeft, PlayerTop);                  
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}

/*
internal void 
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32 *Pixel = (uint32 *) Row;
        
        for (int X = 0; X < Buffer->Width; X++)
        {
            uint8 Blue = (uint8)(X + BlueOffset);
            uint8 Green = (uint8)(Y + GreenOffset);
            
            *Pixel++ = ((Green << 8) | Blue);   
        }
        
        Row += Buffer->Pitch;
    }
}
*/