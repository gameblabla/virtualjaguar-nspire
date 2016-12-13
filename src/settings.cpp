//
// SETTINGS.CPP: Virtual Jaguar configuration loading/saving support
//
// by James L. Hammons
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include "SDL.h"
#include "sdlemu_config.h"
#include "log.h"
#include "settings.h"


// Global variables

VJSettings vjs;

// Private function prototypes

void CheckForTrailingSlash(char * path);

//
// Load Virtual Jaguar's settings
//
void LoadVJSettings(void)
{
	vjs.useJoystick = false;
	vjs.joyport = 0;
	vjs.hardwareTypeNTSC = true;
	vjs.useJaguarBIOS = false;
	vjs.DSPEnabled = true;
	vjs.usePipelinedDSP = false;
	vjs.fullscreen = false;
	vjs.useOpenGL = 0;
	vjs.glFilter = 0;

	// Keybindings in order of U, D, L, R, C, B, A, Op, Pa, 0-9, #, *
	vjs.p1KeyBindings[0] = SDLK_UP;
	vjs.p1KeyBindings[1] = SDLK_DOWN;
	vjs.p1KeyBindings[2] = SDLK_LEFT;
	vjs.p1KeyBindings[3] = SDLK_RIGHT;
	vjs.p1KeyBindings[4] = SDLK_LCTRL;
	vjs.p1KeyBindings[5] = SDLK_LSHIFT;
	vjs.p1KeyBindings[6] = SDLK_BACKSPACE;
	vjs.p1KeyBindings[7] = SDLK_TAB;
	vjs.p1KeyBindings[8] = SDLK_RETURN;
	vjs.p1KeyBindings[9] =  SDLK_0;
	vjs.p1KeyBindings[10] = SDLK_1;
	vjs.p1KeyBindings[11] = SDLK_2;
	vjs.p1KeyBindings[12] = SDLK_3;
	vjs.p1KeyBindings[13] = SDLK_4;
	vjs.p1KeyBindings[14] = SDLK_5;
	vjs.p1KeyBindings[15] = SDLK_6;
	vjs.p1KeyBindings[16] = SDLK_7;
	vjs.p1KeyBindings[17] = SDLK_8;
	vjs.p1KeyBindings[18] = SDLK_9;
	vjs.p1KeyBindings[19] = SDLK_KP_DIVIDE;
	vjs.p1KeyBindings[20] = SDLK_KP_MULTIPLY;

	strcpy(vjs.jagBootPath, "./BIOS/jagboot.rom");
	strcpy(vjs.CDBootPath, "./BIOS/jagcd.rom");
	strcpy(vjs.EEPROMPath, "./EEPROMs");
	strcpy(vjs.ROMPath, "./ROMs");
	CheckForTrailingSlash(vjs.EEPROMPath);
	CheckForTrailingSlash(vjs.ROMPath);

	vjs.hardwareTypeAlpine = false;	// No external setting for this yet...
}

//
// Save Virtual Jaguar's settings
//
void SaveVJSettings(void)
{
}

//
// Check path for a trailing slash, and append if not present
//
void CheckForTrailingSlash(char * path)
{
	if (strlen(path) > 0)
		if (path[strlen(path) - 1] != '/')
			strcat(path, "/");	// NOTE: Possible buffer overflow
}
