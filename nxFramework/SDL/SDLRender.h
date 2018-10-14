#pragma once
#include <common.h>

namespace NXFramework
{

namespace SDL
{
    extern SDL_Window*      Window;
    extern SDL_Surface*     Surface;
    extern SDL_Renderer*    Renderer;

    // Lifetime
    void Initialize(u32 displayWidth, u32 displayHeight);
    void Shutdown();

    // Frame
    void BeginFrame();
    void EndFrame();
}


}
