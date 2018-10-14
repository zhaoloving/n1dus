#include "SDLHelper.h"

namespace NXFramework
{

namespace SDL
{

void ClearScreen(SDL_Renderer *renderer, SDL_Color colour)
{
	SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
	SDL_RenderClear(renderer);
}

void DrawRect(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color colour)
{
	SDL_Rect rect;
	rect.x = x; rect.y = y; rect.w = w; rect.h = h;
	SDL_SetRenderDrawColor(SDL::Renderer, colour.r, colour.g, colour.b, colour.a);
	SDL_RenderFillRect(SDL::Renderer, &rect);
}

void DrawCircle(SDL_Renderer *renderer, int x, int y, int r, SDL_Color colour)
{
	filledCircleRGBA(renderer, x, y, r, colour.r, colour.g, colour.b, colour.a);
	return;
}

void DrawText(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color colour, const char *text, int wrap)
{
	SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(font, text, colour, wrap);
	SDL_SetSurfaceAlphaMod(surface, colour.a);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);

	SDL_Rect position;
	position.x = x; position.y = y;
	SDL_QueryTexture(texture, NULL, NULL, &position.w, &position.h);
	SDL_RenderCopy(renderer, texture, NULL, &position);
	SDL_DestroyTexture(texture);
}

void DrawTextf(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color colour, const char* text, ...)
{
	char buffer[256];
	va_list args;
	va_start(args, text);
	vsnprintf(buffer, 256, text, args);
	DrawText(renderer, font, x, y, colour, buffer);
	va_end(args);
}

void LoadImage(SDL_Renderer *renderer, SDL_Texture **texture, char *path)
{
	SDL_Surface *loaded_surface = NULL;
	loaded_surface = IMG_Load(path);

	if (loaded_surface)
	{
		Uint32 colorkey = SDL_MapRGB(loaded_surface->format, 0, 0, 0);
		SDL_SetColorKey(loaded_surface, SDL_TRUE, colorkey);
		*texture = SDL_CreateTextureFromSurface(renderer, loaded_surface);
        if(*texture == NULL)
            LOG("SDL_CreateTextureFromSurface failed %s...\n",path);
	}
	else
    {
        LOG("IMG_Load failed %s...\n",path);
    }
	SDL_FreeSurface(loaded_surface);
}

void DrawImage(SDL_Renderer *renderer, SDL_Texture *texture, int x, int y)
{
	SDL_Rect position;
	position.x = x; position.y = y;
	SDL_QueryTexture(texture, NULL, NULL, &position.w, &position.h);
	SDL_RenderCopy(renderer, texture, NULL, &position);
}

void DrawImageScale(SDL_Renderer *renderer, SDL_Texture *texture, int x, int y, int w, int h)
{
	SDL_Rect position;
	position.x = x; position.y = y; position.w = w; position.h = h;
	SDL_RenderCopy(renderer, texture, NULL, &position);
}

}

}
