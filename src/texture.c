#include "texture.h"

SDL_Texture* texture_load(SDL_Renderer *renderer, char *path)
{
	SDL_Surface *surface = IMG_Load(path);
	if(!surface)
	{
		printf("cant load surface: %s\n",SDL_GetError());
	}
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	if(!texture)
	{
		printf("cant load surface: %s\n",SDL_GetError());
	}
	
	SDL_FreeSurface(surface);

	return texture;
}

