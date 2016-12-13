//
// Blitter core
//
// Originally by David Raingeard
// GCC/SDL port by Niels Wagenaar (Linux/WIN32) and Caz (BeOS)
// Extensive cleanups/fixes by James L. Hammons
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "jaguar.h"
#include "blitter.h"

// External global variables

extern int jaguar_active_memory_dumps;

// Local global variables

uint8_t blitter_working = 0;
bool startConciseBlitLogging = false;
bool logBlit = false;

// Blitter register RAM (most of it is hidden from the user)

static uint8_t blitter_ram[0x100];

// Other crapola

bool specialLog = false;
extern int effect_start;
void BlitterMidsummer(uint32_t cmd);
void BlitterMidsummer2(void);


#define REG(A)	(((uint32)blitter_ram[(A)] << 24) | ((uint32)blitter_ram[(A)+1] << 16) \
				| ((uint32)blitter_ram[(A)+2] << 8) | (uint32)blitter_ram[(A)+3])
#define WREG(A,D)	(blitter_ram[(A)] = ((D)>>24)&0xFF, blitter_ram[(A)+1] = ((D)>>16)&0xFF, \
					blitter_ram[(A)+2] = ((D)>>8)&0xFF, blitter_ram[(A)+3] = (D)&0xFF)

// Blitter registers (offsets from F02200)

#define A1_BASE			((UINT32)0x00)
#define A1_FLAGS		((UINT32)0x04)
#define A1_CLIP			((UINT32)0x08)	// Height and width values for clipping
#define A1_PIXEL		((UINT32)0x0C)	// Integer part of the pixel (Y.i and X.i)
#define A1_STEP			((UINT32)0x10)	// Integer part of the step
#define A1_FSTEP		((UINT32)0x14)	// Fractionnal part of the step
#define A1_FPIXEL		((UINT32)0x18)	// Fractionnal part of the pixel (Y.f and X.f)
#define A1_INC			((UINT32)0x1C)	// Integer part of the increment
#define A1_FINC			((UINT32)0x20)	// Fractional part of the increment
#define A2_BASE			((UINT32)0x24)
#define A2_FLAGS		((UINT32)0x28)
#define A2_MASK			((UINT32)0x2C)	// Modulo values for x and y (M.y  and M.x)
#define A2_PIXEL		((UINT32)0x30)	// Integer part of the pixel (no fractional part for A2)
#define A2_STEP			((UINT32)0x34)	// Integer part of the step (no fractional part for A2)
#define COMMAND			((UINT32)0x38)
#define PIXLINECOUNTER	((UINT32)0x3C)
#define SRCDATA			((UINT32)0x40)
#define DSTDATA			((UINT32)0x48)
#define DSTZ			((UINT32)0x50)
#define SRCZINT			((UINT32)0x58)
#define SRCZFRAC		((UINT32)0x60)
#define PATTERNDATA		((UINT32)0x68)
#define INTENSITYINC	((UINT32)0x70)
#define ZINC			((UINT32)0x74)
#define COLLISIONCTRL	((UINT32)0x78)
#define PHRASEINT0		((UINT32)0x7C)
#define PHRASEINT1		((UINT32)0x80)
#define PHRASEINT2		((UINT32)0x84)
#define PHRASEINT3		((UINT32)0x88)
#define PHRASEZ0		((UINT32)0x8C)
#define PHRASEZ1		((UINT32)0x90)
#define PHRASEZ2		((UINT32)0x94)
#define PHRASEZ3		((UINT32)0x98)

// Blitter command bits

#define SRCEN			(cmd & 0x00000001)
#define SRCENZ			(cmd & 0x00000002)
#define SRCENX			(cmd & 0x00000004)
#define DSTEN			(cmd & 0x00000008)
#define DSTENZ			(cmd & 0x00000010)
#define DSTWRZ			(cmd & 0x00000020)
#define CLIPA1			(cmd & 0x00000040)

#define UPDA1F			(cmd & 0x00000100)
#define UPDA1			(cmd & 0x00000200)
#define UPDA2			(cmd & 0x00000400)

#define DSTA2			(cmd & 0x00000800)

#define Z_OP_INF		(cmd & 0x00040000)
#define Z_OP_EQU		(cmd & 0x00080000)
#define Z_OP_SUP		(cmd & 0x00100000)

#define LFU_NAN			(cmd & 0x00200000)
#define LFU_NA			(cmd & 0x00400000)
#define LFU_AN			(cmd & 0x00800000)
#define LFU_A			(cmd & 0x01000000)

#define CMPDST			(cmd & 0x02000000)
#define BCOMPEN			(cmd & 0x04000000)
#define DCOMPEN			(cmd & 0x08000000)

#define PATDSEL			(cmd & 0x00010000)
#define ADDDSEL			(cmd & 0x00020000)
#define TOPBEN			(cmd & 0x00004000)
#define TOPNEN			(cmd & 0x00008000)
#define BKGWREN			(cmd & 0x10000000)
#define GOURD			(cmd & 0x00001000)
#define GOURZ			(cmd & 0x00002000)
#define SRCSHADE		(cmd & 0x40000000)


#define XADDPHR	 0
#define XADDPIX	 1
#define XADD0	 2
#define XADDINC	 3

#define XSIGNSUB_A1		(REG(A1_FLAGS)&0x080000)
#define XSIGNSUB_A2		(REG(A2_FLAGS)&0x080000)

#define YSIGNSUB_A1		(REG(A1_FLAGS)&0x100000)
#define YSIGNSUB_A2		(REG(A2_FLAGS)&0x100000)

#define YADD1_A1		(REG(A1_FLAGS)&0x040000)
#define YADD1_A2		(REG(A2_FLAGS)&0x040000)

/*******************************************************************************
********************** STUFF CUT BELOW THIS LINE! ******************************
*******************************************************************************/

//Put 'em back, once we fix the problem!!! [KO]
// 1 bpp pixel read
#define PIXEL_SHIFT_1(a)      (((~a##_x) >> 16) & 7)
#define PIXEL_OFFSET_1(a)     (((((uint32_t)a##_y >> 16) * a##_width / 8) + (((uint32_t)a##_x >> 19) & ~7)) * (1 + a##_pitch) + (((uint32_t)a##_x >> 19) & 7))
#define READ_PIXEL_1(a)       ((JaguarReadByte(a##_addr+PIXEL_OFFSET_1(a), BLITTER) >> PIXEL_SHIFT_1(a)) & 0x01)
//#define READ_PIXEL_1(a)       ((JaguarReadByte(a##_addr+PIXEL_OFFSET_1(a)) >> PIXEL_SHIFT_1(a)) & 0x01)

// 2 bpp pixel read
#define PIXEL_SHIFT_2(a)      (((~a##_x) >> 15) & 6)
#define PIXEL_OFFSET_2(a)     (((((uint32_t)a##_y >> 16) * a##_width / 4) + (((uint32_t)a##_x >> 18) & ~7)) * (1 + a##_pitch) + (((uint32_t)a##_x >> 18) & 7))
#define READ_PIXEL_2(a)       ((JaguarReadByte(a##_addr+PIXEL_OFFSET_2(a), BLITTER) >> PIXEL_SHIFT_2(a)) & 0x03)
//#define READ_PIXEL_2(a)       ((JaguarReadByte(a##_addr+PIXEL_OFFSET_2(a)) >> PIXEL_SHIFT_2(a)) & 0x03)

// 4 bpp pixel read
#define PIXEL_SHIFT_4(a)      (((~a##_x) >> 14) & 4)
#define PIXEL_OFFSET_4(a)     (((((uint32_t)a##_y >> 16) * (a##_width/2)) + (((uint32_t)a##_x >> 17) & ~7)) * (1 + a##_pitch) + (((uint32_t)a##_x >> 17) & 7))
#define READ_PIXEL_4(a)       ((JaguarReadByte(a##_addr+PIXEL_OFFSET_4(a), BLITTER) >> PIXEL_SHIFT_4(a)) & 0x0f)
//#define READ_PIXEL_4(a)       ((JaguarReadByte(a##_addr+PIXEL_OFFSET_4(a)) >> PIXEL_SHIFT_4(a)) & 0x0f)

// 8 bpp pixel read
#define PIXEL_OFFSET_8(a)     (((((uint32_t)a##_y >> 16) * a##_width) + (((uint32_t)a##_x >> 16) & ~7)) * (1 + a##_pitch) + (((uint32_t)a##_x >> 16) & 7))
#define READ_PIXEL_8(a)       (JaguarReadByte(a##_addr+PIXEL_OFFSET_8(a), BLITTER))
//#define READ_PIXEL_8(a)       (JaguarReadByte(a##_addr+PIXEL_OFFSET_8(a)))

// 16 bpp pixel read
#define PIXEL_OFFSET_16(a)    (((((uint32_t)a##_y >> 16) * a##_width) + (((uint32_t)a##_x >> 16) & ~3)) * (1 + a##_pitch) + (((uint32_t)a##_x >> 16) & 3))
#define READ_PIXEL_16(a)       (JaguarReadWord(a##_addr+(PIXEL_OFFSET_16(a)<<1), BLITTER))
//#define READ_PIXEL_16(a)       (JaguarReadWord(a##_addr+(PIXEL_OFFSET_16(a)<<1)))

// 32 bpp pixel read
#define PIXEL_OFFSET_32(a)    (((((uint32_t)a##_y >> 16) * a##_width) + (((uint32_t)a##_x >> 16) & ~1)) * (1 + a##_pitch) + (((uint32_t)a##_x >> 16) & 1))
#define READ_PIXEL_32(a)      (JaguarReadLong(a##_addr+(PIXEL_OFFSET_32(a)<<2), BLITTER))
//#define READ_PIXEL_32(a)      (JaguarReadLong(a##_addr+(PIXEL_OFFSET_32(a)<<2)))

// pixel read
#define READ_PIXEL(a,f) (\
	 (((f>>3)&0x07) == 0) ? (READ_PIXEL_1(a)) : \
	 (((f>>3)&0x07) == 1) ? (READ_PIXEL_2(a)) : \
	 (((f>>3)&0x07) == 2) ? (READ_PIXEL_4(a)) : \
	 (((f>>3)&0x07) == 3) ? (READ_PIXEL_8(a)) : \
	 (((f>>3)&0x07) == 4) ? (READ_PIXEL_16(a)) : \
	 (((f>>3)&0x07) == 5) ? (READ_PIXEL_32(a)) : 0)

// 16 bpp z data read
#define ZDATA_OFFSET_16(a)     (PIXEL_OFFSET_16(a) + a##_zoffs * 4)
#define READ_ZDATA_16(a)       (JaguarReadWord(a##_addr+(ZDATA_OFFSET_16(a)<<1), BLITTER))
//#define READ_ZDATA_16(a)       (JaguarReadWord(a##_addr+(ZDATA_OFFSET_16(a)<<1)))

// z data read
#define READ_ZDATA(a,f) (READ_ZDATA_16(a))

// 16 bpp z data write
#define WRITE_ZDATA_16(a,d)     {  JaguarWriteWord(a##_addr+(ZDATA_OFFSET_16(a)<<1), d, BLITTER); }
//#define WRITE_ZDATA_16(a,d)     {  JaguarWriteWord(a##_addr+(ZDATA_OFFSET_16(a)<<1), d); }

// z data write
#define WRITE_ZDATA(a,f,d) WRITE_ZDATA_16(a,d);

// 1 bpp r data read
#define READ_RDATA_1(r,a,p)  ((p) ?  ((REG(r+(((uint32_t)a##_x >> 19) & 0x04))) >> (((uint32_t)a##_x >> 16) & 0x1F)) & 0x0001 : (REG(r) & 0x0001))

// 2 bpp r data read
#define READ_RDATA_2(r,a,p)  ((p) ?  ((REG(r+(((uint32_t)a##_x >> 18) & 0x04))) >> (((uint32_t)a##_x >> 15) & 0x3E)) & 0x0003 : (REG(r) & 0x0003))

// 4 bpp r data read
#define READ_RDATA_4(r,a,p)  ((p) ?  ((REG(r+(((uint32_t)a##_x >> 17) & 0x04))) >> (((uint32_t)a##_x >> 14) & 0x28)) & 0x000F : (REG(r) & 0x000F))

// 8 bpp r data read
#define READ_RDATA_8(r,a,p)  ((p) ?  ((REG(r+(((uint32_t)a##_x >> 16) & 0x04))) >> (((uint32_t)a##_x >> 13) & 0x18)) & 0x00FF : (REG(r) & 0x00FF))

// 16 bpp r data read
#define READ_RDATA_16(r,a,p)  ((p) ? ((REG(r+(((uint32_t)a##_x >> 15) & 0x04))) >> (((uint32_t)a##_x >> 12) & 0x10)) & 0xFFFF : (REG(r) & 0xFFFF))

// 32 bpp r data read
#define READ_RDATA_32(r,a,p)  ((p) ? REG(r+(((uint32_t)a##_x >> 14) & 0x04)) : REG(r))

// register data read
#define READ_RDATA(r,a,f,p) (\
	 (((f>>3)&0x07) == 0) ? (READ_RDATA_1(r,a,p)) : \
	 (((f>>3)&0x07) == 1) ? (READ_RDATA_2(r,a,p)) : \
	 (((f>>3)&0x07) == 2) ? (READ_RDATA_4(r,a,p)) : \
	 (((f>>3)&0x07) == 3) ? (READ_RDATA_8(r,a,p)) : \
	 (((f>>3)&0x07) == 4) ? (READ_RDATA_16(r,a,p)) : \
	 (((f>>3)&0x07) == 5) ? (READ_RDATA_32(r,a,p)) : 0)

// 1 bpp pixel write
#define WRITE_PIXEL_1(a,d)       { JaguarWriteByte(a##_addr+PIXEL_OFFSET_1(a), (JaguarReadByte(a##_addr+PIXEL_OFFSET_1(a), BLITTER)&(~(0x01 << PIXEL_SHIFT_1(a))))|(d<<PIXEL_SHIFT_1(a)), BLITTER); }
//#define WRITE_PIXEL_1(a,d)       { JaguarWriteByte(a##_addr+PIXEL_OFFSET_1(a), (JaguarReadByte(a##_addr+PIXEL_OFFSET_1(a))&(~(0x01 << PIXEL_SHIFT_1(a))))|(d<<PIXEL_SHIFT_1(a))); }

// 2 bpp pixel write
#define WRITE_PIXEL_2(a,d)       { JaguarWriteByte(a##_addr+PIXEL_OFFSET_2(a), (JaguarReadByte(a##_addr+PIXEL_OFFSET_2(a), BLITTER)&(~(0x03 << PIXEL_SHIFT_2(a))))|(d<<PIXEL_SHIFT_2(a)), BLITTER); }
//#define WRITE_PIXEL_2(a,d)       { JaguarWriteByte(a##_addr+PIXEL_OFFSET_2(a), (JaguarReadByte(a##_addr+PIXEL_OFFSET_2(a))&(~(0x03 << PIXEL_SHIFT_2(a))))|(d<<PIXEL_SHIFT_2(a))); }

// 4 bpp pixel write
#define WRITE_PIXEL_4(a,d)       { JaguarWriteByte(a##_addr+PIXEL_OFFSET_4(a), (JaguarReadByte(a##_addr+PIXEL_OFFSET_4(a), BLITTER)&(~(0x0f << PIXEL_SHIFT_4(a))))|(d<<PIXEL_SHIFT_4(a)), BLITTER); }
//#define WRITE_PIXEL_4(a,d)       { JaguarWriteByte(a##_addr+PIXEL_OFFSET_4(a), (JaguarReadByte(a##_addr+PIXEL_OFFSET_4(a))&(~(0x0f << PIXEL_SHIFT_4(a))))|(d<<PIXEL_SHIFT_4(a))); }

// 8 bpp pixel write
#define WRITE_PIXEL_8(a,d)       { JaguarWriteByte(a##_addr+PIXEL_OFFSET_8(a), d, BLITTER); }
//#define WRITE_PIXEL_8(a,d)       { JaguarWriteByte(a##_addr+PIXEL_OFFSET_8(a), d); }

// 16 bpp pixel write
#define WRITE_PIXEL_16(a,d)     {  JaguarWriteWord(a##_addr+(PIXEL_OFFSET_16(a)<<1),d); }
//#define WRITE_PIXEL_16(a,d)     {  JaguarWriteWord(a##_addr+(PIXEL_OFFSET_16(a)<<1), d, BLITTER); if (specialLog) WriteLog("Pixel write address: %08X\n", a##_addr+(PIXEL_OFFSET_16(a)<<1)); }
//#define WRITE_PIXEL_16(a,d)     {  JaguarWriteWord(a##_addr+(PIXEL_OFFSET_16(a)<<1), d); if (specialLog) WriteLog("Pixel write address: %08X\n", a##_addr+(PIXEL_OFFSET_16(a)<<1)); }

// 32 bpp pixel write
#define WRITE_PIXEL_32(a,d)		{ JaguarWriteLong(a##_addr+(PIXEL_OFFSET_32(a)<<2), d, BLITTER); }
//#define WRITE_PIXEL_32(a,d)		{ JaguarWriteLong(a##_addr+(PIXEL_OFFSET_32(a)<<2), d); }

// pixel write
#define WRITE_PIXEL(a,f,d) {\
	switch ((f>>3)&0x07) { \
	case 0: WRITE_PIXEL_1(a,d);  break;  \
	case 1: WRITE_PIXEL_2(a,d);  break;  \
	case 2: WRITE_PIXEL_4(a,d);  break;  \
	case 3: WRITE_PIXEL_8(a,d);  break;  \
	case 4: WRITE_PIXEL_16(a,d); break;  \
	case 5: WRITE_PIXEL_32(a,d); break;  \
	}}

// Width in Pixels of a Scanline
// This is a pretranslation of the value found in the A1 & A2 flags: It's really a floating point value
// of the form EEEEMM where MM is the mantissa with an implied "1." in front of it and the EEEE value is
// the exponent. Valid values for the exponent range from 0 to 11 (decimal). It's easiest to think of it
// as a floating point bit pattern being followed by a number of zeroes. So, e.g., 001101 translates to
// 1.01 (the "1." being implied) x (2 ^ 3) or 1010 -> 10 in base 10 (i.e., 1.01 with the decimal place
// being shifted to the right 3 places).
/*static uint32_t blitter_scanline_width[48] =
{
     0,    0,    0,    0,					// Note: This would really translate to 1, 1, 1, 1
     2,    0,    0,    0,
     4,    0,    6,    0,
     8,   10,   12,   14,
    16,   20,   24,   28,
    32,   40,   48,   56,
    64,   80,   96,  112,
   128,  160,  192,  224,
   256,  320,  384,  448,
   512,  640,  768,  896,
  1024, 1280, 1536, 1792,
  2048, 2560, 3072, 3584
};//*/

//static uint8_t * tom_ram_8;
//static uint8_t * paletteRam;
static uint8_t src;
static uint8_t dst;
static uint8_t misc;
static uint8_t a1ctl;
static uint8_t mode;
static uint8_t ity;
static uint8_t zop;
static uint8_t op;
static uint8_t ctrl;
static uint32_t a1_addr;
static uint32_t a2_addr;
static int32_t a1_zoffs;
static int32_t a2_zoffs;
static uint32_t xadd_a1_control;
static uint32_t xadd_a2_control;
static int32_t a1_pitch;
static int32_t a2_pitch;
static uint32_t n_pixels;
static uint32_t n_lines;
static int32_t a1_x;
static int32_t a1_y;
static int32_t a1_width;
static int32_t a2_x;
static int32_t a2_y;
static int32_t a2_width;
static int32_t a2_mask_x;
static int32_t a2_mask_y;
static int32_t a1_xadd;
static int32_t a1_yadd;
static int32_t a2_xadd;
static int32_t a2_yadd;
static uint8_t a1_phrase_mode;
static uint8_t a2_phrase_mode;
static int32_t a1_step_x = 0;
static int32_t a1_step_y = 0;
static int32_t a2_step_x = 0;
static int32_t a2_step_y = 0;
static uint32_t outer_loop;
static uint32_t inner_loop;
static uint32_t a2_psize;
static uint32_t a1_psize;
static uint32_t gouraud_add;
//static uint32_t gouraud_data;
//static uint16_t gint[4];
//static uint16_t gfrac[4];
//static uint8_t  gcolour[4];
static int gd_i[4];
static int gd_c[4];
static int gd_ia, gd_ca;
static int colour_index = 0;
static int32_t zadd;
static uint32_t z_i[4];

static int32_t a1_clip_x, a1_clip_y;

// In the spirit of "get it right first, *then* optimize" I've taken the liberty
// of removing all the unnecessary code caching. If it turns out to be a good way
// to optimize the blitter, then we may revisit it in the future...

//
// Generic blit handler
//
void blitter_generic(uint32_t cmd)
{
	uint32_t srcdata, srczdata, dstdata, dstzdata, writedata, inhibit;
	uint32_t bppSrc = (DSTA2 ? 1 << ((REG(A1_FLAGS) >> 3) & 0x07) : 1 << ((REG(A2_FLAGS) >> 3) & 0x07));
	while (outer_loop--)
	{
		uint32_t a1_start = a1_x, a2_start = a2_x, bitPos = 0;

		//Kludge for Hover Strike...
		//I wonder if this kludge is in conjunction with the SRCENX down below...
		// This isn't so much a kludge but the way things work in BCOMPEN mode...!
		if (BCOMPEN && SRCENX)
		{
			if (n_pixels < bppSrc)
				bitPos = bppSrc - n_pixels;
		}

		inner_loop = n_pixels;
		while (inner_loop--)
		{
			srcdata = srczdata = dstdata = dstzdata = writedata = inhibit = 0;

			if (!DSTA2)							// Data movement: A1 <- A2
			{
				if (SRCEN || SRCENX)	// Not sure if this is correct... (seems to be...!)
				{
					srcdata = READ_PIXEL(a2, REG(A2_FLAGS));

					if (SRCENZ)
						srczdata = READ_ZDATA(a2, REG(A2_FLAGS));
					else if (cmd & 0x0001C020)	// PATDSEL | TOPBEN | TOPNEN | DSTWRZ
						srczdata = READ_RDATA(SRCZINT, a2, REG(A2_FLAGS), a2_phrase_mode);
				}
				else	// Use SRCDATA register...
				{
					srcdata = READ_RDATA(SRCDATA, a2, REG(A2_FLAGS), a2_phrase_mode);

					if (cmd & 0x0001C020)		// PATDSEL | TOPBEN | TOPNEN | DSTWRZ
						srczdata = READ_RDATA(SRCZINT, a2, REG(A2_FLAGS), a2_phrase_mode);
				}

				// load dst data and Z
				if (DSTEN)
				{
					dstdata = READ_PIXEL(a1, REG(A1_FLAGS));

					if (DSTENZ)
						dstzdata = READ_ZDATA(a1, REG(A1_FLAGS));
					else
						dstzdata = READ_RDATA(DSTZ, a1, REG(A1_FLAGS), a1_phrase_mode);
				}
				else
				{
					dstdata = READ_RDATA(DSTDATA, a1, REG(A1_FLAGS), a1_phrase_mode);

					if (DSTENZ)
						dstzdata = READ_RDATA(DSTZ, a1, REG(A1_FLAGS), a1_phrase_mode);
				}

				if (GOURZ)
					srczdata = z_i[colour_index] >> 16;

				// apply z comparator
				if (Z_OP_INF && srczdata <  dstzdata)	inhibit = 1;
				if (Z_OP_EQU && srczdata == dstzdata)	inhibit = 1;
				if (Z_OP_SUP && srczdata >  dstzdata)	inhibit = 1;

				if (DCOMPEN | BCOMPEN)
				{
//Temp, for testing Hover Strike
//Doesn't seem to do it... Why?
//What needs to happen here is twofold. First, the address generator in the outer loop has
//to honor the BPP when calculating the start address (which it kinda does already). Second,
//it has to step bit by bit when using BCOMPEN. How to do this???
	if (BCOMPEN)
	{
		uint32_t pixShift = (~bitPos) & (bppSrc - 1);
		srcdata = (srcdata >> pixShift) & 0x01;

		bitPos++;
	}
/*
Interesting (Hover Strike--large letter):

Blit! (0018FA70 <- 008DDC40) count: 2 x 13, A1/2_FLAGS: 00014218/00013C18 [cmd: 1401060C]
 CMD -> src: SRCENX dst: DSTEN  misc:  a1ctl: UPDA1 UPDA2 mode:  ity: PATDSEL z-op:  op: LFU_CLEAR ctrl: BCOMPEN BKGWREN
  A1 step values: -2 (X), 1 (Y)
  A2 step values: -1 (X), 1 (Y) [mask (unused): 00000000 - FFFFFFFF/FFFFFFFF]
  A1 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 320 (21), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
  A2 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 192 (1E), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
        A1 x/y: 100/12, A2 x/y: 106/0 Pattern: 000000F300000000

Blit! (0018FA70 <- 008DDC40) count: 8 x 13, A1/2_FLAGS: 00014218/00013C18 [cmd: 1401060C]
 CMD -> src: SRCENX dst: DSTEN  misc:  a1ctl: UPDA1 UPDA2 mode:  ity: PATDSEL z-op:  op: LFU_CLEAR ctrl: BCOMPEN BKGWREN
  A1 step values: -8 (X), 1 (Y)
  A2 step values: -1 (X), 1 (Y) [mask (unused): 00000000 - FFFFFFFF/FFFFFFFF]
  A1 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 320 (21), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
  A2 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 192 (1E), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
        A1 x/y: 102/12, A2 x/y: 107/0 Pattern: 000000F300000000

Blit! (0018FA70 <- 008DDC40) count: 1 x 13, A1/2_FLAGS: 00014218/00013C18 [cmd: 1401060C]
 CMD -> src: SRCENX dst: DSTEN  misc:  a1ctl: UPDA1 UPDA2 mode:  ity: PATDSEL z-op:  op: LFU_CLEAR ctrl: BCOMPEN BKGWREN
  A1 step values: -1 (X), 1 (Y)
  A2 step values: -1 (X), 1 (Y) [mask (unused): 00000000 - FFFFFFFF/FFFFFFFF]
  A1 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 320 (21), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
  A2 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 192 (1E), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
        A1 x/y: 118/12, A2 x/y: 70/0 Pattern: 000000F300000000

Blit! (0018FA70 <- 008DDC40) count: 8 x 13, A1/2_FLAGS: 00014218/00013C18 [cmd: 1401060C]
 CMD -> src: SRCENX dst: DSTEN  misc:  a1ctl: UPDA1 UPDA2 mode:  ity: PATDSEL z-op:  op: LFU_CLEAR ctrl: BCOMPEN BKGWREN
  A1 step values: -8 (X), 1 (Y)
  A2 step values: -1 (X), 1 (Y) [mask (unused): 00000000 - FFFFFFFF/FFFFFFFF]
  A1 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 320 (21), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
  A2 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 192 (1E), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
        A1 x/y: 119/12, A2 x/y: 71/0 Pattern: 000000F300000000

Blit! (0018FA70 <- 008DDC40) count: 1 x 13, A1/2_FLAGS: 00014218/00013C18 [cmd: 1401060C]
 CMD -> src: SRCENX dst: DSTEN  misc:  a1ctl: UPDA1 UPDA2 mode:  ity: PATDSEL z-op:  op: LFU_CLEAR ctrl: BCOMPEN BKGWREN
  A1 step values: -1 (X), 1 (Y)
  A2 step values: -1 (X), 1 (Y) [mask (unused): 00000000 - FFFFFFFF/FFFFFFFF]
  A1 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 320 (21), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
  A2 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 192 (1E), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
        A1 x/y: 127/12, A2 x/y: 66/0 Pattern: 000000F300000000

Blit! (0018FA70 <- 008DDC40) count: 8 x 13, A1/2_FLAGS: 00014218/00013C18 [cmd: 1401060C]
 CMD -> src: SRCENX dst: DSTEN  misc:  a1ctl: UPDA1 UPDA2 mode:  ity: PATDSEL z-op:  op: LFU_CLEAR ctrl: BCOMPEN BKGWREN
  A1 step values: -8 (X), 1 (Y)
  A2 step values: -1 (X), 1 (Y) [mask (unused): 00000000 - FFFFFFFF/FFFFFFFF]
  A1 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 320 (21), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
  A2 -> pitch: 1 phrases, depth: 8bpp, z-off: 0, width: 192 (1E), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
        A1 x/y: 128/12, A2 x/y: 67/0 Pattern: 000000F300000000
*/


					if (!CMPDST)
					{
//WriteLog("Blitter: BCOMPEN set on command %08X inhibit prev:%u, now:", cmd, inhibit);
						// compare source pixel with pattern pixel
/*
Blit! (000B8250 <- 0012C3A0) count: 16 x 1, A1/2_FLAGS: 00014420/00012000 [cmd: 05810001]
 CMD -> src: SRCEN  dst:  misc:  a1ctl:  mode:  ity: PATDSEL z-op:  op: LFU_REPLACE ctrl: BCOMPEN
  A1 -> pitch: 1 phrases, depth: 16bpp, z-off: 0, width: 384 (22), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
  A2 -> pitch: 1 phrases, depth: 1bpp, z-off: 0, width: 16 (10), addctl: XADDPIX YADD0 XSIGNADD YSIGNADD
        x/y: 0/20
...
*/
// AvP is still wrong, could be cuz it's doing A1 -> A2...

// Src is the 1bpp bitmap... DST is the PATTERN!!!
// This seems to solve at least ONE of the problems with MC3D...
// Why should this be inverted???
// Bcuz it is. This is supposed to be used only for a bit -> pixel expansion...
/*						if (srcdata == READ_RDATA(PATTERNDATA, a2, REG(A2_FLAGS), a2_phrase_mode))
//						if (srcdata != READ_RDATA(PATTERNDATA, a2, REG(A2_FLAGS), a2_phrase_mode))
							inhibit = 1;//*/
/*						uint32_t A2bpp = 1 << ((REG(A2_FLAGS) >> 3) & 0x07);
						if (A2bpp == 1 || A2bpp == 16 || A2bpp == 8)
							inhibit = (srcdata == 0 ? 1: 0);
//							inhibit = !srcdata;
						else
							WriteLog("Blitter: Bad BPP (%u) selected for BCOMPEN mode!\n", A2bpp);//*/
// What it boils down to is this:

						if (srcdata == 0)
							inhibit = 1;//*/
					}
					else
					{
						// compare destination pixel with pattern pixel
						if (dstdata == READ_RDATA(PATTERNDATA, a1, REG(A1_FLAGS), a1_phrase_mode))
//						if (dstdata != READ_RDATA(PATTERNDATA, a1, REG(A1_FLAGS), a1_phrase_mode))
							inhibit = 1;
					}
				}

				if (CLIPA1)
				{
					inhibit |= (((a1_x >> 16) < a1_clip_x && (a1_x >> 16) >= 0
						&& (a1_y >> 16) < a1_clip_y && (a1_y >> 16) >= 0) ? 0 : 1);
				}

				// compute the write data and store
				if (!inhibit)
				{
					if (PATDSEL)
					{
						// use pattern data for write data
						writedata = READ_RDATA(PATTERNDATA, a1, REG(A1_FLAGS), a1_phrase_mode);
					}
					else if (ADDDSEL)
					{
						writedata = (srcdata & 0xFF) + (dstdata & 0xFF);

						if (!TOPBEN)
						{
//This is correct now, but slow...
							int16_t s = (srcdata & 0xFF) | (srcdata & 0x80 ? 0xFF00 : 0x0000),
								d = dstdata & 0xFF;
							int16_t sum = s + d;

							if (sum < 0)
								writedata = 0x00;
							else if (sum > 0xFF)
								writedata = 0xFF;
							else
								writedata = (uint32_t)sum;
						}

//This doesn't seem right... Looks like it would muck up the low byte... !!! FIX !!!
						writedata |= (srcdata & 0xF00) + (dstdata & 0xF00);

						if (!TOPNEN && writedata > 0xFFF)
						{
							writedata &= 0xFFF;
						}

						writedata |= (srcdata & 0xF000) + (dstdata & 0xF000);
					}
					else
					{
						if (LFU_NAN) writedata |= ~srcdata & ~dstdata;
						if (LFU_NA)  writedata |= ~srcdata & dstdata;
						if (LFU_AN)  writedata |= srcdata  & ~dstdata;
						if (LFU_A) 	 writedata |= srcdata  & dstdata;
					}
					
					if (GOURD)
						writedata = ((gd_c[colour_index]) << 8) | (gd_i[colour_index] >> 16);

					if (SRCSHADE)
					{
						int intensity = srcdata & 0xFF;
						int ia = gd_ia >> 16;
						if (ia & 0x80)
							ia = 0xFFFFFF00 | ia;
						intensity += ia;
						if (intensity < 0)
							intensity = 0;
						if (intensity > 0xFF)
							intensity = 0xFF;
						writedata = (srcdata & 0xFF00) | intensity;
					}
				}
				else
				{
					writedata = dstdata;
					srczdata = dstzdata;
				}

				if (BKGWREN || !inhibit)
				{
					// write to the destination
					WRITE_PIXEL(a1, REG(A1_FLAGS), writedata);
					if (DSTWRZ)
						WRITE_ZDATA(a1, REG(A1_FLAGS), srczdata);
				}
			}
			else	// if (DSTA2) 							// Data movement: A1 -> A2
			{
				// load src data and Z
				if (SRCEN)
				{
					srcdata = READ_PIXEL(a1, REG(A1_FLAGS));
					if (SRCENZ)
						srczdata = READ_ZDATA(a1, REG(A1_FLAGS));
					else if (cmd & 0x0001C020)	// PATDSEL | TOPBEN | TOPNEN | DSTWRZ
						srczdata = READ_RDATA(SRCZINT, a1, REG(A1_FLAGS), a1_phrase_mode);
				}
				else
				{
					srcdata = READ_RDATA(SRCDATA, a1, REG(A1_FLAGS), a1_phrase_mode);
					if (cmd & 0x001C020)	// PATDSEL | TOPBEN | TOPNEN | DSTWRZ
						srczdata = READ_RDATA(SRCZINT, a1, REG(A1_FLAGS), a1_phrase_mode);
				}

				// load dst data and Z
				if (DSTEN)
				{
					dstdata = READ_PIXEL(a2, REG(A2_FLAGS));
					if (DSTENZ)
						dstzdata = READ_ZDATA(a2, REG(A2_FLAGS));
					else
						dstzdata = READ_RDATA(DSTZ, a2, REG(A2_FLAGS), a2_phrase_mode);
				}
				else
				{
					dstdata = READ_RDATA(DSTDATA, a2, REG(A2_FLAGS), a2_phrase_mode);
					if (DSTENZ)
						dstzdata = READ_RDATA(DSTZ, a2, REG(A2_FLAGS), a2_phrase_mode);
				}

				if (GOURZ)
					srczdata = z_i[colour_index] >> 16;

				// apply z comparator
				if (Z_OP_INF && srczdata < dstzdata)	inhibit = 1;
				if (Z_OP_EQU && srczdata == dstzdata)	inhibit = 1;
				if (Z_OP_SUP && srczdata > dstzdata)	inhibit = 1;

				// apply data comparator
//NOTE: The bit comparator (BCOMPEN) is NOT the same at the data comparator!
				if (DCOMPEN | BCOMPEN)
				{
					if (!CMPDST)
					{
						// compare source pixel with pattern pixel
// AvP: Numbers are correct, but sprites are not!
//This doesn't seem to be a problem... But could still be wrong...
/*						if (srcdata == READ_RDATA(PATTERNDATA, a1, REG(A1_FLAGS), a1_phrase_mode))
//						if (srcdata != READ_RDATA(PATTERNDATA, a1, REG(A1_FLAGS), a1_phrase_mode))
							inhibit = 1;//*/
// This is probably not 100% correct... It works in the 1bpp case
// (in A1 <- A2 mode, that is...)
// AvP: This is causing blocks to be written instead of bit patterns...
// Works now...
// NOTE: We really should separate out the BCOMPEN & DCOMPEN stuff!
/*						uint32_t A1bpp = 1 << ((REG(A1_FLAGS) >> 3) & 0x07);
						if (A1bpp == 1 || A1bpp == 16 || A1bpp == 8)
							inhibit = (srcdata == 0 ? 1: 0);
						else
							WriteLog("Blitter: Bad BPP (%u) selected for BCOMPEN mode!\n", A1bpp);//*/
// What it boils down to is this:
						if (srcdata == 0)
							inhibit = 1;//*/
					}
					else
					{
						// compare destination pixel with pattern pixel
						if (dstdata == READ_RDATA(PATTERNDATA, a2, REG(A2_FLAGS), a2_phrase_mode))
//						if (dstdata != READ_RDATA(PATTERNDATA, a2, REG(A2_FLAGS), a2_phrase_mode))
							inhibit = 1;
					}

// This is DEFINITELY WRONG
//					if (a1_phrase_mode || a2_phrase_mode)
//						inhibit = !inhibit;
				}

				if (CLIPA1)
				{
					inhibit |= (((a1_x >> 16) < a1_clip_x && (a1_x >> 16) >= 0
						&& (a1_y >> 16) < a1_clip_y && (a1_y >> 16) >= 0) ? 0 : 1);
				}

				// compute the write data and store
				if (!inhibit)
				{
					if (PATDSEL)
					{
						// use pattern data for write data
						writedata = READ_RDATA(PATTERNDATA, a2, REG(A2_FLAGS), a2_phrase_mode);
					}
					else if (ADDDSEL)
					{
						// intensity addition
						writedata = (srcdata & 0xFF) + (dstdata & 0xFF);
						if (!(TOPBEN) && writedata > 0xFF)
							writedata = 0xFF;
						writedata |= (srcdata & 0xF00) + (dstdata & 0xF00);
						if (!(TOPNEN) && writedata > 0xFFF)
							writedata = 0xFFF;
						writedata |= (srcdata & 0xF000) + (dstdata & 0xF000);
					}
					else
					{
						if (LFU_NAN)
							writedata |= ~srcdata & ~dstdata;
						if (LFU_NA)
							writedata |= ~srcdata & dstdata;
						if (LFU_AN)
							writedata |= srcdata & ~dstdata;
						if (LFU_A)
							writedata |= srcdata & dstdata;
					}

					if (GOURD)
						writedata = ((gd_c[colour_index]) << 8) | (gd_i[colour_index] >> 16);

					if (SRCSHADE)
					{
						int intensity = srcdata & 0xFF;
						int ia = gd_ia >> 16;
						if (ia & 0x80)
							ia = 0xFFFFFF00 | ia;
						intensity += ia;
						if (intensity < 0)
							intensity = 0;
						if (intensity > 0xFF)
							intensity = 0xFF;
						writedata = (srcdata & 0xFF00) | intensity;
					}
				}
				else
				{
					writedata = dstdata;
					srczdata = dstzdata;
				}

				if (BKGWREN || !inhibit)
				{
					// write to the destination
					WRITE_PIXEL(a2, REG(A2_FLAGS), writedata);

					if (DSTWRZ)
						WRITE_ZDATA(a2, REG(A2_FLAGS), srczdata);
				}
			}

			if (!BCOMPEN)
			{
				a1_x += a1_xadd, a1_y += a1_yadd;
				a2_x = (a2_x + a2_xadd) & a2_mask_x, a2_y = (a2_y + a2_yadd) & a2_mask_y;
			}
			else
			{
				a1_y += a1_yadd, a2_y = (a2_y + a2_yadd) & a2_mask_y;
				if (!DSTA2)
				{
					a1_x += a1_xadd;
					if (bitPos % bppSrc == 0)
						a2_x = (a2_x + a2_xadd) & a2_mask_x;
				}
				else
				{
					a2_x = (a2_x + a2_xadd) & a2_mask_x;
					if (bitPos % bppSrc == 0)
						a1_x += a1_xadd;
				}
			}//*/

			if (GOURZ)
				z_i[colour_index] += zadd;

			if (GOURD || SRCSHADE)
			{
				gd_i[colour_index] += gd_ia;
				if ((int32_t)gd_i[colour_index] < 0)
					gd_i[colour_index] = 0;
				if (gd_i[colour_index] > 0x00FFFFFF)
					gd_i[colour_index] = 0x00FFFFFF;//*/

								gd_c[colour_index] += gd_ca;
				if ((int32_t)gd_c[colour_index] < 0)
					gd_c[colour_index] = 0;
				if (gd_c[colour_index] > 0x000000FF)
					gd_c[colour_index] = 0x000000FF;
			}

			if (GOURD || SRCSHADE || GOURZ)
			{
				if (a1_phrase_mode)
					colour_index = (colour_index + 1) & 0x03;
			}
		}

//NOTE: The way to fix the CD BIOS is to uncomment below and comment the stuff after
//      the phrase mode mucking around. But it fucks up everything else...
//#define SCREWY_CD_DEPENDENT
#ifdef SCREWY_CD_DEPENDENT
		a1_x += a1_step_x;
		a1_y += a1_step_y;
		a2_x += a2_step_x;
		a2_y += a2_step_y;//*/
#endif

		if (a1_phrase_mode)			// v2
		{
			// Bump the pointer to the next phrase boundary
			// Even though it works, this is crappy... Clean it up!
			uint32_t size = 64 / a1_psize;

			// Crappy kludge... ('aligning' source to destination)
			if (a2_phrase_mode && DSTA2)
			{
				uint32_t extra = (a2_start >> 16) % size;
				a1_x += extra << 16;
			}

			uint32_t pixelSize = (size - 1) << 16;
			a1_x = (a1_x + pixelSize) & ~pixelSize;
		}
		if (a2_phrase_mode)			// v1
		{
			// Bump the pointer to the next phrase boundary
			// Even though it works, this is crappy... Clean it up!
			uint32_t size = 64 / a2_psize;

			// Crappy kludge... ('aligning' source to destination)
			// Prolly should do this for A1 channel as well... [DONE]
			if (a1_phrase_mode && !DSTA2)
			{
				uint32_t extra = (a1_start >> 16) % size;
				a2_x += extra << 16;
			}

			uint32_t pixelSize = (size - 1) << 16;
			a2_x = (a2_x + pixelSize) & ~pixelSize;
		}

		//Not entirely: This still mucks things up... !!! FIX !!!
		//Should this go before or after the phrase mode mucking around?
#ifndef SCREWY_CD_DEPENDENT
		a1_x += a1_step_x;
		a1_y += a1_step_y;
		a2_x += a2_step_x;
		a2_y += a2_step_y;//*/
#endif
	}

	// write values back to registers
	WREG(A1_PIXEL,  (a1_y & 0xFFFF0000) | ((a1_x >> 16) & 0xFFFF));
	WREG(A1_FPIXEL, (a1_y << 16) | (a1_x & 0xFFFF));
	WREG(A2_PIXEL,  (a2_y & 0xFFFF0000) | ((a2_x >> 16) & 0xFFFF));
//specialLog = false;
}

void blitter_blit(uint32_t cmd)
{
	uint32_t pitchValue[4] = { 0, 1, 3, 2 };
	colour_index = 0;
	src = cmd & 0x07;
	dst = (cmd >> 3) & 0x07;
	misc = (cmd >> 6) & 0x03;
	a1ctl = (cmd >> 8) & 0x7;
	mode = (cmd >> 11) & 0x07;
	ity = (cmd >> 14) & 0x0F;
	zop = (cmd >> 18) & 0x07;
	op = (cmd >> 21) & 0x0F;
	ctrl = (cmd >> 25) & 0x3F;

	// Addresses in A1/2_BASE are *phrase* aligned, i.e., bottom three bits are ignored!
	// NOTE: This fixes Rayman's bad collision detection AND keeps T2K working!
	a1_addr = REG(A1_BASE) & 0xFFFFFFF8;
	a2_addr = REG(A2_BASE) & 0xFFFFFFF8;

	a1_zoffs = (REG(A1_FLAGS) >> 6) & 7;
	a2_zoffs = (REG(A2_FLAGS) >> 6) & 7;

	xadd_a1_control = (REG(A1_FLAGS) >> 16) & 0x03;
	xadd_a2_control = (REG(A2_FLAGS) >> 16) & 0x03;

	a1_pitch = pitchValue[(REG(A1_FLAGS) & 0x03)];
	a2_pitch = pitchValue[(REG(A2_FLAGS) & 0x03)];

	n_pixels = REG(PIXLINECOUNTER) & 0xFFFF;
	n_lines = (REG(PIXLINECOUNTER) >> 16) & 0xFFFF;

	a1_x = (REG(A1_PIXEL) << 16) | (REG(A1_FPIXEL) & 0xFFFF);
	a1_y = (REG(A1_PIXEL) & 0xFFFF0000) | (REG(A1_FPIXEL) >> 16);

	uint32_t m = (REG(A1_FLAGS) >> 9) & 0x03, e = (REG(A1_FLAGS) >> 11) & 0x0F;
	a1_width = ((0x04 | m) << e) >> 2;//*/

	a2_x = (REG(A2_PIXEL) & 0x0000FFFF) << 16;
	a2_y = (REG(A2_PIXEL) & 0xFFFF0000);

	m = (REG(A2_FLAGS) >> 9) & 0x03, e = (REG(A2_FLAGS) >> 11) & 0x0F;
	a2_width = ((0x04 | m) << e) >> 2;//*/
	a2_mask_x = ((REG(A2_MASK) & 0x0000FFFF) << 16) | 0xFFFF;
	a2_mask_y = (REG(A2_MASK) & 0xFFFF0000) | 0xFFFF;

	// Check for "use mask" flag
	if (!(REG(A2_FLAGS) & 0x8000))
	{
		a2_mask_x = 0xFFFFFFFF; // must be 16.16
		a2_mask_y = 0xFFFFFFFF; // must be 16.16
	}

	a1_phrase_mode = 0;

	// According to the official documentation, a hardware bug ties A2's yadd bit to A1's...
	a2_yadd = a1_yadd = (YADD1_A1 ? 1 << 16 : 0);

	if (YSIGNSUB_A1)
		a1_yadd = -a1_yadd;

	// determine a1_xadd
	switch (xadd_a1_control)
	{
	case XADDPHR:
// This is a documented Jaguar bug relating to phrase mode and truncation... Look into it!
		// add phrase offset to X and truncate
		a1_xadd = 1 << 16;
		a1_phrase_mode = 1;
		break;
	case XADDPIX:
		// add pixelsize (1) to X
		a1_xadd = 1 << 16;
		break;
	case XADD0:
		// add zero (for those nice vertical lines)
		a1_xadd = 0;
		break;
	case XADDINC:
		// add the contents of the increment register
		a1_xadd = (REG(A1_INC) << 16)		 | (REG(A1_FINC) & 0x0000FFFF);
		a1_yadd = (REG(A1_INC) & 0xFFFF0000) | (REG(A1_FINC) >> 16);
		break;
	}

	if (XSIGNSUB_A1)
		a1_xadd = -a1_xadd;

	if (YSIGNSUB_A2)
		a2_yadd = -a2_yadd;

	a2_phrase_mode = 0;

	// determine a2_xadd
	switch (xadd_a2_control)
	{
	case XADDPHR:
		// add phrase offset to X and truncate
		a2_xadd = 1 << 16;
		a2_phrase_mode = 1;
		break;
	case XADDPIX:
		// add pixelsize (1) to X
		a2_xadd = 1 << 16;
		break;
	case XADD0:
		// add zero (for those nice vertical lines)
		a2_xadd = 0;
		break;
	}

	if (XSIGNSUB_A2)
		a2_xadd = -a2_xadd;

	// Modify outer loop steps based on blitter command

	a1_step_x = 0;
	a1_step_y = 0;
	a2_step_x = 0;
	a2_step_y = 0;

	if (UPDA1F)
		a1_step_x = (REG(A1_FSTEP) & 0xFFFF),
		a1_step_y = (REG(A1_FSTEP) >> 16);

	if (UPDA1)
		a1_step_x |= ((REG(A1_STEP) & 0x0000FFFF) << 16),
		a1_step_y |= ((REG(A1_STEP) & 0xFFFF0000));

	if (UPDA2)
		a2_step_x = (REG(A2_STEP) & 0x0000FFFF) << 16,
		a2_step_y = (REG(A2_STEP) & 0xFFFF0000);

	outer_loop = n_lines;

	// Clipping...

	if (CLIPA1)
		a1_clip_x = REG(A1_CLIP) & 0x7FFF,
		a1_clip_y = (REG(A1_CLIP) >> 16) & 0x7FFF;

// This phrase sizing is incorrect as well... !!! FIX !!! [NOTHING TO FIX]
// Err, this is pixel size... (and it's OK)
	a2_psize = 1 << ((REG(A2_FLAGS) >> 3) & 0x07);
	a1_psize = 1 << ((REG(A1_FLAGS) >> 3) & 0x07);

	// Z-buffering
	if (GOURZ)
	{
		zadd = REG(ZINC);

		for(int v=0; v<4; v++)
			z_i[v] = REG(PHRASEZ0 + v*4);
	}

	// Gouraud shading
	if (GOURD || GOURZ || SRCSHADE)
	{
		gd_c[0] = blitter_ram[PATTERNDATA + 6];
		gd_i[0]	= ((uint32_t)blitter_ram[PATTERNDATA + 7] << 16)
			| ((uint32_t)blitter_ram[SRCDATA + 6] << 8) | blitter_ram[SRCDATA + 7];

		gd_c[1] = blitter_ram[PATTERNDATA + 4];
		gd_i[1]	= ((uint32_t)blitter_ram[PATTERNDATA + 5] << 16)
			| ((uint32_t)blitter_ram[SRCDATA + 4] << 8) | blitter_ram[SRCDATA + 5];

		gd_c[2] = blitter_ram[PATTERNDATA + 2];
		gd_i[2]	= ((uint32_t)blitter_ram[PATTERNDATA + 3] << 16)
			| ((uint32_t)blitter_ram[SRCDATA + 2] << 8) | blitter_ram[SRCDATA + 3];

		gd_c[3] = blitter_ram[PATTERNDATA + 0];
		gd_i[3]	= ((uint32_t)blitter_ram[PATTERNDATA + 1] << 16)
			| ((uint32_t)blitter_ram[SRCDATA + 0] << 8) | blitter_ram[SRCDATA + 1];

		gouraud_add = REG(INTENSITYINC);

		gd_ia = gouraud_add & 0x00FFFFFF;
		if (gd_ia & 0x00800000)
			gd_ia = 0xFF000000 | gd_ia;

		gd_ca = (gouraud_add >> 24) & 0xFF;
		if (gd_ca & 0x00000080)
			gd_ca = 0xFFFFFF00 | gd_ca;
	}

	blitter_working = 1;
	blitter_generic(cmd);

	blitter_working = 0;
}


void blitter_init(void)
{
	blitter_reset();
}

void blitter_reset(void)
{
	memset(blitter_ram, 0x00, 0xA0);
}

void blitter_done(void)
{
	WriteLog("BLIT: Done.\n");
}

uint8 BlitterReadByte(uint32 offset, uint32 who/*=UNKNOWN*/)
{
	offset &= 0xFF;

	// status register
//This isn't cycle accurate--how to fix? !!! FIX !!!
	if (offset == (0x38 + 3))
		return 0x01;	// always idle

//Attempted fix for AvP:
	if (offset >= 0x04 && offset <= 0x07)
//		return (offset > 0x05 ? blitter_ram[PIXLINECOUNTER + offset - 0x04] : 0x00);
//		return 0x00;	// WO register! What does it expect to see here???
//This is it. I wonder if it just ignores the lower three bits?
		return blitter_ram[A1_PIXEL + offset - 0x04];

	return blitter_ram[offset];
}

//Crappy!
uint16 BlitterReadWord(uint32 offset, uint32 who/*=UNKNOWN*/)
{
	return ((uint16)BlitterReadByte(offset, who) << 8) | (uint16)BlitterReadByte(offset+1, who);
}

//Crappy!
uint32 BlitterReadLong(uint32 offset, uint32 who/*=UNKNOWN*/)
{
	return (BlitterReadWord(offset, who) << 16) | BlitterReadWord(offset+2, who);
}

void BlitterWriteByte(uint32 offset, uint8 data, uint32 who/*=UNKNOWN*/)
{
/*if (offset & 0xFF == 0x7B)
	WriteLog("--> Wrote to B_STOP: value -> %02X\n", data);*/
	offset &= 0xFF;

//	if ((offset >= 0x7C) && (offset <= 0x9B))
	// This handles writes to INTENSITY0-3 by also writing them to their proper places in
	// PATTERNDATA & SOURCEDATA (should do the same for the Z registers! !!! FIX !!! [DONE])
	if ((offset >= 0x7C) && (offset <= 0x9B))//8B))
	{
		switch (offset)
		{
		// INTENSITY registers 0-3
		case 0x7C: break;
		case 0x7D: blitter_ram[PATTERNDATA + 1] = data; break;
		case 0x7E: blitter_ram[SRCDATA + 0] = data; break;
		case 0x7F: blitter_ram[SRCDATA + 1] = data; break;

		case 0x80: break;
		case 0x81: blitter_ram[PATTERNDATA + 3] = data; break;
		case 0x82: blitter_ram[SRCDATA + 2] = data; break;
		case 0x83: blitter_ram[SRCDATA + 3] = data; break;
		
		case 0x84: break;
		case 0x85: blitter_ram[PATTERNDATA + 5] = data; break;
		case 0x86: blitter_ram[SRCDATA + 4] = data; break;
		case 0x87: blitter_ram[SRCDATA + 5] = data; break;
		
		case 0x88: break;
		case 0x89: blitter_ram[PATTERNDATA + 7] = data; break;
		case 0x8A: blitter_ram[SRCDATA + 6] = data; break;
		case 0x8B: blitter_ram[SRCDATA + 7] = data; break;

		// Z registers 0-3
		case 0x8C: blitter_ram[SRCZINT + 0] = data; break;
		case 0x8D: blitter_ram[SRCZINT + 1] = data; break;
		case 0x8E: blitter_ram[SRCZFRAC + 0] = data; break;
		case 0x8F: blitter_ram[SRCZFRAC + 1] = data; break;

		case 0x90: blitter_ram[SRCZINT + 2] = data; break;
		case 0x91: blitter_ram[SRCZINT + 3] = data; break;
		case 0x92: blitter_ram[SRCZFRAC + 2] = data; break;
		case 0x93: blitter_ram[SRCZFRAC + 3] = data; break;
		
		case 0x94: blitter_ram[SRCZINT + 4] = data; break;
		case 0x95: blitter_ram[SRCZINT + 5] = data; break;
		case 0x96: blitter_ram[SRCZFRAC + 4] = data; break;
		case 0x97: blitter_ram[SRCZFRAC + 5] = data; break;
		
		case 0x98: blitter_ram[SRCZINT + 6] = data; break;
		case 0x99: blitter_ram[SRCZINT + 7] = data; break;
		case 0x9A: blitter_ram[SRCZFRAC + 6] = data; break;
		case 0x9B: blitter_ram[SRCZFRAC + 7] = data; break;
		}
	}

	blitter_ram[offset] = data;
}

void BlitterWriteWord(uint32 offset, uint16 data, uint32 who/*=UNKNOWN*/)
{
	BlitterWriteByte(offset+0, (data>>8) & 0xFF, who);
	BlitterWriteByte(offset+1, data & 0xFF, who);

	if ((offset & 0xFF) == 0x3A)
	{
		blitter_blit(GET32(blitter_ram, 0x38));
	}
}
//F02278,9,A,B

void BlitterWriteLong(uint32 offset, uint32 data, uint32 who/*=UNKNOWN*/)
{
	BlitterWriteWord(offset, data >> 16, who);
	BlitterWriteWord(offset+2, data & 0xFFFF, who);
}

void LogBlit(void)
{
}