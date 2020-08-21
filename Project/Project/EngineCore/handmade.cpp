#include "handmade.h"
#include<math.h>

internal void
RenderWeirdGradeint(game_offscreen_buffer* Buffer, int XOffset, int YOffset) 
{
    uint8* Row = (uint8*)Buffer->Memory;

    for (int Y = 0; Y < Buffer->Height; Y++) 
    {
        uint32* Pixel = (uint32*)Row;

        for (int X = 0; X < Buffer->Width; X++) 
        {
            uint8 Blue = X + XOffset;
            uint8 Green = Y + YOffset;
            // 0xXXRRGGBB
            *Pixel++ = (Green << 8) | Blue;
        }

        Row += Buffer->Pitch;
    }
}

internal void
GameOutputSound(game_sound_output_buffer* SoundBuffer, int ToneHz)
{
    
    local_persist float tSine;
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplePerSecond / ToneHz;

    int16* SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
    {
        real32 SineValue = sinf(tSine);
        int16 Value = (int16)(SineValue * ToneVolume);
        *SampleOut++ = Value;
        *SampleOut++ = Value;

        tSine += 2.0f * Pi32 * (1.0f / (float)WavePeriod);
    }
}

internal void
GameUpdateAndRender(game_memory* Memory, game_input* Input, game_offscreen_buffer* Buffer,
    game_sound_output_buffer* SoundBuffer)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize)

    game_state* GameState = (game_state *)Memory->PermanentStorage;

    if (!Memory->IsInitialized)
    {
       
        GameState->ToneHz = 256;    

        Memory->IsInitialized = true;
    }

    GameOutputSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradeint(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}

internal game_state* GameStartUp()
{
    game_state* GameState = new game_state;
    if (GameState)
    {
        GameState->ToneHz = 256;
    }
    return GameState;
}

internal void GameShutdow(game_state* GameState)
{
    delete GameState;
}