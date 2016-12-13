//
// VIDEO.CPP: SDL/local hardware specific video routines
//
// by James L. Hammons
//

#include "tom.h"
#include "settings.h"
#include "video.h"
#include <SDL.h>
// External global variables

//shouldn't these exist here??? Prolly.
//And now, they do! :-)
SDL_Surface * surface, * mainSurface;
Uint32 mainSurfaceFlags;
int16 * backbuffer;
// One of the reasons why OpenGL is slower then normal SDL rendering, is because
// the data is being pumped into the buffer every frame with a overflow as result.
// So, we going tot render every 1 frame instead of every 0 frame.
int frame_ticker  = 0;

//
// Create SDL/OpenGL surfaces
//
bool InitVideo(void)
{
	mainSurfaceFlags = SDL_SWSURFACE;

	mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH,
	(vjs.hardwareTypeNTSC ? VIRTUAL_SCREEN_HEIGHT_NTSC : VIRTUAL_SCREEN_HEIGHT_PAL),
	16, mainSurfaceFlags);


	if (mainSurface == NULL)
	{
		WriteLog("VJ: SDL is unable to set the video mode: %s\n", SDL_GetError());
		return false;
	}

	// Create the primary SDL display (16 BPP, 5/5/5 RGB format)
	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, VIRTUAL_SCREEN_WIDTH,
		(vjs.hardwareTypeNTSC ? VIRTUAL_SCREEN_HEIGHT_NTSC : VIRTUAL_SCREEN_HEIGHT_PAL),
		16, 0x7C00, 0x03E0, 0x001F, 0);

	if (surface == NULL)
	{
		WriteLog("VJ: Could not create primary SDL surface: %s\n", SDL_GetError());
		return false;
	}

	// Set up the backbuffer
//To be safe, this should be 1280 * 625 * 2...
	backbuffer = (int16 *)malloc(320 * 240 * sizeof(int16));
// backbuffer = (int16 *)malloc(1280 * 625 * sizeof(int16));
//	memset(backbuffer, 0x44, VIRTUAL_SCREEN_WIDTH * VIRTUAL_SCREEN_HEIGHT_NTSC * sizeof(int16));
	memset(backbuffer, 0x44, VIRTUAL_SCREEN_WIDTH *
		(vjs.hardwareTypeNTSC ? VIRTUAL_SCREEN_HEIGHT_NTSC : VIRTUAL_SCREEN_HEIGHT_PAL)
		* sizeof(int16));

	return true;
}

//
// Free various SDL components
//
void VideoDone(void)
{
	if (surface) SDL_FreeSurface(surface);
	if (backbuffer) free(backbuffer);
}

//
// Render the backbuffer to the primary screen surface
//
void RenderBackbuffer(void)
{
	SDL_LockSurface(surface);
	memcpy(surface->pixels, backbuffer, tom_getVideoModeWidth() * tom_getVideoModeHeight() * 2);
	SDL_UnlockSurface(surface);

	SDL_Rect rect = { 0, 0, surface->w, surface->h };
	SDL_BlitSurface(surface, &rect, mainSurface, &rect);
	SDL_Flip(mainSurface);
}

//
// Resize the main SDL screen & backbuffer
//
void ResizeScreen(uint32 width, uint32 height)
{
	if (surface) SDL_FreeSurface(surface);
	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16, 0x7C00, 0x03E0, 0x001F, 0);

	if (surface == NULL)
	{
		WriteLog("Video: Could not create primary SDL surface: %s", SDL_GetError());
		exit(1);
	}

	mainSurface = SDL_SetVideoMode(width, height, 16, mainSurfaceFlags);
	if (mainSurface == NULL)
	{
		WriteLog("Video: SDL is unable to set the video mode: %s\n", SDL_GetError());
		exit(1);
	}
}

//
// Return the screen's pitch
//
uint32 GetSDLScreenPitch(void)
{
	return surface->pitch;
}

//
// Fullscreen <-> window switching
//
void ToggleFullscreen(void)
{
}
