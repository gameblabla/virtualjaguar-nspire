//
// DAC (really, Synchronous Serial Interface) Handler
//
// Originally by David Raingeard
// GCC/SDL port by Niels Wagenaar (Linux/WIN32) and Caz (BeOS)
// Rewritten by James L. Hammons
//

#include "m68k.h"
#include "jaguar.h"
#include "settings.h"
#include "dac.h"

//#define DEBUG_DAC

#define BUFFER_SIZE		0x10000						// Make the DAC buffers 64K x 16 bits

// Jaguar memory locations

#define LTXD			0xF1A148
#define RTXD			0xF1A14C
#define LRXD			0xF1A148
#define RRXD			0xF1A14C
#define SCLK			0xF1A150
#define SMODE			0xF1A154

// Global variables

uint16 lrxd, rrxd;									// I2S ports (into Jaguar)

// Local variables

uint32 LeftFIFOHeadPtr, LeftFIFOTailPtr, RightFIFOHeadPtr, RightFIFOTailPtr;

// We can get away with using native endian here because we can tell SDL to use the native
// endian when looking at the sample buffer, i.e., no need to worry about it.

uint16 * DACBuffer;
uint8 SCLKFrequencyDivider = 19;						// Default is roughly 22 KHz (20774 Hz in NTSC mode)
uint16 serialMode = 0;

// Private function prototypes

int GetCalculatedFrequency(void);

//
// Initialize the SDL sound system
//
void DACInit(void)
{
	memory_malloc_secure((void **)&DACBuffer, BUFFER_SIZE * sizeof(uint16), "DAC buffer");

	DACReset();
}

//
// Reset the sound buffer FIFOs
//
void DACReset(void)
{
	LeftFIFOHeadPtr = LeftFIFOTailPtr = 0, RightFIFOHeadPtr = RightFIFOTailPtr = 1;
}

//
// Close down the SDL sound subsystem
//
void DACDone(void)
{
	memory_free(DACBuffer);
}

//
// Calculate the frequency of SCLK * 32 using the divider
//
int GetCalculatedFrequency(void)
{
	int systemClockFrequency = (vjs.hardwareTypeNTSC ? RISC_CLOCK_RATE_NTSC : RISC_CLOCK_RATE_PAL);

	return systemClockFrequency / (32 * (2 * (SCLKFrequencyDivider + 1)));
}

//
// LTXD/RTXD/SCLK/SMODE ($F1A148/4C/50/54)
//
void DACWriteByte(uint32 offset, uint8 data, uint32 who/*= UNKNOWN*/)
{
	if (offset == SCLK + 3)
		DACWriteWord(offset - 3, (uint16)data);
}

void DACWriteWord(uint32 offset, uint16 data, uint32 who/*= UNKNOWN*/)
{
	if (offset == LTXD + 2)
	{
		while ((LeftFIFOTailPtr + 2) & (BUFFER_SIZE - 1) == LeftFIFOHeadPtr);

		LeftFIFOTailPtr = (LeftFIFOTailPtr + 2) % BUFFER_SIZE;
		DACBuffer[LeftFIFOTailPtr] = data;
	}
	else if (offset == RTXD + 2)
	{
		while ((RightFIFOTailPtr + 2) & (BUFFER_SIZE - 1) == RightFIFOHeadPtr);

		RightFIFOTailPtr = (RightFIFOTailPtr + 2) % BUFFER_SIZE;
		DACBuffer[RightFIFOTailPtr] = data;

	}
	else if (offset == SCLK + 2)					// Sample rate
	{
		if ((uint8)data != SCLKFrequencyDivider)
		{
			SCLKFrequencyDivider = (uint8)data;

			if (data > 7)	// Anything less than 8 is too high!
			{
				DACReset();
			}
		}
	}
	else if (offset == SMODE + 2)
	{
		serialMode = data;
	}
}

//
// LRXD/RRXD/SSTAT ($F1A148/4C/50)
//
uint8 DACReadByte(uint32 offset, uint32 who/*= UNKNOWN*/)
{
	return 0xFF;
}

//static uint16 fakeWord = 0;
uint16 DACReadWord(uint32 offset, uint32 who/*= UNKNOWN*/)
{
	if (offset == LRXD || offset == RRXD)
		return 0x0000;
	else if (offset == LRXD + 2)
		return lrxd;
	else if (offset == RRXD + 2)
		return rrxd;

	return 0xFFFF;	// May need SSTAT as well... (but may be a Jaguar II only feature)		
}
