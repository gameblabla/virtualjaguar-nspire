//
// Joystick handler
//
// by cal2
// GCC/SDL port by Niels Wagenaar (Linux/WIN32) and Caz (BeOS)
// Cleanups/fixes by James L. Hammons
//

#include <time.h>
#include <SDL.h>
#include "jaguar.h"
#include "video.h"
#include "settings.h"

#define BUTTON_U		0
#define BUTTON_D		1
#define BUTTON_L		2
#define BUTTON_R		3
#define BUTTON_s		4
#define BUTTON_7		5
#define BUTTON_4		6
#define BUTTON_1		7
#define BUTTON_0		8
#define BUTTON_8		9
#define BUTTON_5		10
#define BUTTON_2		11
#define BUTTON_d		12
#define BUTTON_9		13
#define BUTTON_6		14
#define BUTTON_3		15

#define BUTTON_A		16
#define BUTTON_B		17
#define BUTTON_C		18
#define BUTTON_OPTION	19
#define BUTTON_PAUSE	20

// Global vars

static uint8 joystick_ram[4];
static uint8 joypad_0_buttons[21];
extern bool finished;
extern bool showGUI;
bool GUIKeyHeld = false;
bool interactiveMode = false;
bool iLeft, iRight, iToggle = false;
bool keyHeld1 = false, keyHeld2 = false, keyHeld3 = false;
int objectPtr = 0;
bool startMemLog = false;
extern bool doDSPDis;


void joystick_init(void)
{
	joystick_reset();
}

void joystick_exec(void)
{
//	extern bool useJoystick;
	uint8 * keystate = SDL_GetKeyState(NULL);
  	
	memset(joypad_0_buttons, 0, 21);
	iLeft = iRight = false;

	// Keybindings in order of U, D, L, R, C, B, A, Op, Pa, 0-9, #, *
//	vjs.p1KeyBindings[0] = sdlemu_getval_int("p1k_up", SDLK_UP);

	if (keystate[vjs.p1KeyBindings[0]])
		joypad_0_buttons[BUTTON_U] = 0x01;
	if (keystate[vjs.p1KeyBindings[1]])
		joypad_0_buttons[BUTTON_D] = 0x01;
	if (keystate[vjs.p1KeyBindings[2]])
		joypad_0_buttons[BUTTON_L] = 0x01;
	if (keystate[vjs.p1KeyBindings[3]])
		joypad_0_buttons[BUTTON_R] = 0x01;
	// The buttons are labelled C,B,A on the controller (going from left to right)
	if (keystate[vjs.p1KeyBindings[4]])
		joypad_0_buttons[BUTTON_C] = 0x01;
	if (keystate[vjs.p1KeyBindings[5]])
		joypad_0_buttons[BUTTON_B] = 0x01;
	if (keystate[vjs.p1KeyBindings[6]])
		joypad_0_buttons[BUTTON_A] = 0x01;
//I may yet move these to O and P...
	if (keystate[vjs.p1KeyBindings[7]])
		joypad_0_buttons[BUTTON_OPTION] = 0x01;
	if (keystate[vjs.p1KeyBindings[8]])
		joypad_0_buttons[BUTTON_PAUSE] = 0x01;

	if (keystate[vjs.p1KeyBindings[9]])
		joypad_0_buttons[BUTTON_0] = 0x01;
	if (keystate[vjs.p1KeyBindings[10]])
		joypad_0_buttons[BUTTON_1] = 0x01;
	if (keystate[vjs.p1KeyBindings[11]])
		joypad_0_buttons[BUTTON_2] = 0x01;
	if (keystate[vjs.p1KeyBindings[12]])
		joypad_0_buttons[BUTTON_3] = 0x01;
	if (keystate[vjs.p1KeyBindings[13]])
		joypad_0_buttons[BUTTON_4] = 0x01;
	if (keystate[vjs.p1KeyBindings[14]])
		joypad_0_buttons[BUTTON_5] = 0x01;
	if (keystate[vjs.p1KeyBindings[15]])
		joypad_0_buttons[BUTTON_6] = 0x01;
	if (keystate[vjs.p1KeyBindings[16]])
		joypad_0_buttons[BUTTON_7] = 0x01;
	if (keystate[vjs.p1KeyBindings[17]])
		joypad_0_buttons[BUTTON_8] = 0x01;
	if (keystate[vjs.p1KeyBindings[18]])
		joypad_0_buttons[BUTTON_9] = 0x01;
	if (keystate[vjs.p1KeyBindings[19]])
		joypad_0_buttons[BUTTON_s] = 0x01;
	if (keystate[vjs.p1KeyBindings[20]])
		joypad_0_buttons[BUTTON_d] = 0x01;

    if (keystate[SDLK_ESCAPE])
    {
		jaguar_done();
		VersionDone();
		MemoryDone();
		VideoDone();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_Quit();
		exit(0);
    }

	// Needed to ensure that the events queue is empty [nwagenaar]
    SDL_PumpEvents();
}

void joystick_reset(void)
{
	memset(joystick_ram, 0x00, 4);
	memset(joypad_0_buttons, 0, 21);
}

void joystick_done(void)
{
}

uint8 joystick_byte_read(uint32 offset)
{
//	extern bool hardwareTypeNTSC;
	offset &= 0x03;

	if (offset == 0)
	{
		uint8 data = 0x00;
		int pad0Index = joystick_ram[1] & 0x0F;
		
// This is bad--we're assuming that a bit is set in the last case. Might not be so!
		if (!(pad0Index & 0x01)) 
			pad0Index = 0;
		else if (!(pad0Index & 0x02)) 
			pad0Index = 1;
		else if (!(pad0Index & 0x04)) 
			pad0Index = 2;
		else 
			pad0Index = 3;

		if (joypad_0_buttons[(pad0Index << 2) + 0])	data |= 0x01;
		if (joypad_0_buttons[(pad0Index << 2) + 1]) data |= 0x02;
		if (joypad_0_buttons[(pad0Index << 2) + 2]) data |= 0x04;
		if (joypad_0_buttons[(pad0Index << 2) + 3]) data |= 0x08;

		return ~data;
	}
	else if (offset == 3)
	{
		uint8 data = 0x2F | (vjs.hardwareTypeNTSC ? 0x10 : 0x00);
		int pad0Index = joystick_ram[1] & 0x0F;
		
		if (!(pad0Index & 0x01))
		{
			if (joypad_0_buttons[BUTTON_PAUSE])
				data ^= 0x01;
			if (joypad_0_buttons[BUTTON_A])
				data ^= 0x02;
		}
		else if (!(pad0Index & 0x02))
		{
			if (joypad_0_buttons[BUTTON_B])
				data ^= 0x02;
		}
		else if (!(pad0Index & 0x04))
		{
			if (joypad_0_buttons[BUTTON_C])
				data ^= 0x02;
		}
		else
		{
			if (joypad_0_buttons[BUTTON_OPTION])
				data ^= 0x02;
		}		
		return data;
	}

	return joystick_ram[offset];
}

uint16 joystick_word_read(uint32 offset)
{
	return ((uint16)joystick_byte_read((offset+0)&0x03) << 8) | joystick_byte_read((offset+1)&0x03);
}

void joystick_byte_write(uint32 offset, uint8 data)
{
	joystick_ram[offset&0x03] = data;
}

void joystick_word_write(uint32 offset, uint16 data)
{
	offset &= 0x03;
	joystick_ram[offset+0] = (data >> 8) & 0xFF;
	joystick_ram[offset+1] = data & 0xFF;
}
