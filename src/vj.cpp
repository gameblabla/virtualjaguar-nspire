//
// Virtual Jaguar Emulator
//
// Original codebase by David Raingeard (Cal2)
// GCC/SDL port by Niels Wagenaar (Linux/WIN32) and Caz (BeOS)
// Cleanups/fixes/enhancements by James L. Hammons and Adam Green
//

#ifdef __GCCUNIX__
#include <unistd.h>									// Is this necessary anymore?
#endif

#include <time.h>
#include <SDL.h>
#include "jaguar.h"
#include "video.h"
#include "gui.h"
#include "settings.h"								// Pull in "vjs" struct

// Uncomment this for speed control (?)
//#define SPEED_CONTROL

// Private function prototypes

// External variables

extern uint8 * jaguar_mainRam;
extern uint8 * jaguar_mainRom;
extern uint8 * jaguar_bootRom;
extern uint8 * jaguar_CDBootROM;

// Global variables (export capable)
//should these even be here anymore?

bool finished = false;
bool BIOSLoaded = false;
bool CDBIOSLoaded = false;

//
// The main emulator loop (what else?)
//

//temp, so we can grab this from elsewhere.
uint32 totalFrames;

int vj(char* name)
{
//	int32 nNormalFrac = 0; 
	int32 nFrameskip = 0;							// Default: Show every frame

	printf("Virtual Jaguar GCC/SDL Portable Jaguar Emulator v1.0.7\n");
	printf("Based upon Virtual Jaguar core v1.0.0 by David Raingeard.\n");
	printf("Written by Niels Wagenaar (Linux/WIN32), Carwin Jones (BeOS),\n");
	printf("James L. Hammons (WIN32) and Adam Green (MacOS)\n");
	printf("Contact: http://sdlemu.ngemu.com/ | sdlemu@ngemu.com\n");

	bool haveCart = false;							// Assume there is no cartridge...!
	LoadVJSettings();								// Get config file settings...

	// Set up SDL library
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		return -1;
	}
	
	InitMemory();
	InitVersion();
	jaguar_init();

	// Get the BIOS ROM
	//	if (vjs.useJaguarBIOS)
	// What would be nice here would be a way to check if the BIOS was loaded so that we
	// could disable the pushbutton on the Misc Options menu... !!! FIX !!! [DONE here, but needs to be fixed in GUI as well!]
	BIOSLoaded = (JaguarLoadROM(jaguar_bootRom, vjs.jagBootPath) == 0x20000 ? true : false);
	CDBIOSLoaded = (JaguarLoadROM(jaguar_CDBootROM, vjs.CDBootPath) == 0x40000 ? true : false);

	SET32(jaguar_mainRam, 0, 0x00200000);			// Set top of stack...

	InitVideo();
	InitGUI();

	GUIMain(name);

	//This is no longer accurate...!
	//	int elapsedTime = clock() - startTime;
	//	int fps = (1000 * totalFrames) / elapsedTime;

	jaguar_done();
	VersionDone();
	MemoryDone();
	VideoDone();
	log_done();	

	// Free SDL components last...!
	//SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_CDROM);
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	SDL_Quit();

    return 0;
}
