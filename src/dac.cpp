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

	DACReset();							// Start playback!
	//WriteLog("DAC: Successfully initialized.\n");
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
	//WriteLog("DAC: Done.\n");
}
//
// Calculate the frequency of SCLK * 32 using the divider
//
int GetCalculatedFrequency(void)
{
	int systemClockFrequency = (vjs.hardwareTypeNTSC ? RISC_CLOCK_RATE_NTSC : RISC_CLOCK_RATE_PAL);

	// We divide by 32 here in order to find the frequency of 32 SCLKs in a row (transferring
	// 16 bits of left data + 16 bits of right data = 32 bits, 1 SCLK = 1 bit transferred).
	return systemClockFrequency / (32 * (2 * (SCLKFrequencyDivider + 1)));
}

//
// LTXD/RTXD/SCLK/SMODE ($F1A148/4C/50/54)
//
void DACWriteByte(uint32 offset, uint8 data, uint32 who/*= UNKNOWN*/)
{
	//WriteLog("DAC: %s writing BYTE %02X at %08X\n", whoName[who], data, offset);
	if (offset == SCLK + 3)
		DACWriteWord(offset - 3, (uint16)data);
}

void DACWriteWord(uint32 offset, uint16 data, uint32 who/*= UNKNOWN*/)
{
	if (offset == LTXD + 2)
	{
		// Spin until buffer has been drained (for too fast processors!)...
//Small problem--if Head == 0 and Tail == buffer end, then this will fail... !!! FIX !!!
//[DONE]
		// Also, we're taking advantage of the fact that the buffer is a multiple of two
		// in this check...
		while ((LeftFIFOTailPtr + 2) & (BUFFER_SIZE - 1) == LeftFIFOHeadPtr);
					// Is it necessary to do this? Mebbe.
		// We use a circular buffer 'cause it's easy. Note that the callback function
		// takes care of dumping audio to the soundcard...! Also note that we're writing
		// the samples in the buffer in an interleaved L/R format.
		LeftFIFOTailPtr = (LeftFIFOTailPtr + 2) % BUFFER_SIZE;
		DACBuffer[LeftFIFOTailPtr] = data;
	}
	else if (offset == RTXD + 2)
	{
		// Spin until buffer has been drained (for too fast processors!)...
//uint32 spin = 0;
		while ((RightFIFOTailPtr + 2) & (BUFFER_SIZE - 1) == RightFIFOHeadPtr);
/*		{
spin++;
if (spin == 0x10000000)
{
	//WriteLog("\nStuck in right DAC spinlock! Tail=%u, Head=%u\nAborting!\n", RightFIFOTailPtr, RightFIFOHeadPtr);
	log_done();
	exit(0);
}
		}*/

//This is wrong		if (RightFIFOTailPtr + 2 != RightFIFOHeadPtr)
//		{
		RightFIFOTailPtr = (RightFIFOTailPtr + 2) % BUFFER_SIZE;
		DACBuffer[RightFIFOTailPtr] = data;
//		}
/*#ifdef DEBUG_DAC
		else
			//WriteLog("DAC: Ran into FIFO's right tail pointer!\n");
#endif*/
	}
	else if (offset == SCLK + 2)					// Sample rate
	{
		//WriteLog("DAC: Writing %u to SCLK...\n", data);
		if ((uint8)data != SCLKFrequencyDivider)
		{
			SCLKFrequencyDivider = (uint8)data;
//Of course a better way would be to query the hardware to find the upper limit...
			if (data > 7)	// Anything less than 8 is too high!
			{
				DACReset();
			}
		}
	}
	else if (offset == SMODE + 2)
	{
		serialMode = data;
		/*WriteLog("DAC: %s writing to SMODE. Bits: %s%s%s%s%s%s [68K PC=%08X]\n", whoName[who],
			(data & 0x01 ? "INTERNAL " : ""), (data & 0x02 ? "MODE " : ""),
			(data & 0x04 ? "WSEN " : ""), (data & 0x08 ? "RISING " : ""),
			(data & 0x10 ? "FALLING " : ""), (data & 0x20 ? "EVERYWORD" : ""),
			m68k_get_reg(NULL, M68K_REG_PC));*/
	}
}

//
// LRXD/RRXD/SSTAT ($F1A148/4C/50)
//
uint8 DACReadByte(uint32 offset, uint32 who/*= UNKNOWN*/)
{
//	//WriteLog("DAC: %s reading byte from %08X\n", whoName[who], offset);
	return 0xFF;
}

//static uint16 fakeWord = 0;
uint16 DACReadWord(uint32 offset, uint32 who/*= UNKNOWN*/)
{
//	//WriteLog("DAC: %s reading word from %08X\n", whoName[who], offset);
//	return 0xFFFF;
//	//WriteLog("DAC: %s reading WORD %04X from %08X\n", whoName[who], fakeWord, offset);
//	return fakeWord++;
//NOTE: This only works if a bunch of things are set in BUTCH which we currently don't
//      check for. !!! FIX !!!
// Partially fixed: We check for I2SCNTRL in the JERRY I2S routine...
//	return GetWordFromButchSSI(offset, who);
	if (offset == LRXD || offset == RRXD)
		return 0x0000;
	else if (offset == LRXD + 2)
		return lrxd;
	else if (offset == RRXD + 2)
		return rrxd;

	return 0xFFFF;	// May need SSTAT as well... (but may be a Jaguar II only feature)		
}
