#include "SDLRender.h"

namespace NXFramework
{

namespace SDL
{

SDL_Window *Window;
SDL_Surface *Surface;
SDL_Renderer *Renderer;

// Lifetime
void Initialize(u32 displayWidth, u32 displayHeight)
{
   	SDL_CreateWindowAndRenderer(displayWidth, displayHeight, 0, &SDL::Window, &SDL::Renderer);
	SDL::Surface = SDL_GetWindowSurface(SDL::Window);
	SDL_SetRenderDrawBlendMode(SDL::Renderer, SDL_BLENDMODE_BLEND);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
}
void Shutdown()
{
	SDL_DestroyRenderer(SDL::Renderer);
	SDL_FreeSurface(SDL::Surface);
	SDL_DestroyWindow(SDL::Window);
}

// Frame
void BeginFrame()
{

}
void EndFrame()
{
    SDL_RenderPresent(SDL::Renderer);
}

}

}
