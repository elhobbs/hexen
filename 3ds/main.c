#include <stdbool.h>
#ifdef _3DS
#include <3ds.h>
#endif
#include <string.h>
#include <stdarg.h>
#include "h2def.h"
#include "ST_START.H"

#ifdef _3DS
u32 __stacksize__ = 1024 * 1024;
#endif

int DisplayTicker = 0;
//byte *pcscreen, *destscreen, *destview;
extern  doomcom_t *doomcom;
extern byte *subscreen;
extern boolean	automapactive;

int UpdateState;
extern int screenblocks;
extern boolean automapontop;

void keyboard_draw();

void keyboard_init();
void _3ds_shutdown() {
#ifdef _3DS
	//gpuExit();
	gfxSet3D(false);
	printf("shutting down...maybe...\n");
	gfxExit();
#endif
}

int main(int argc, char**argv)
{
#ifdef _3DS
	gfxInitDefault();
	consoleInit(GFX_BOTTOM, 0);
	consoleSetWindow(0, 0, 0, 40, 19);
	osSetSpeedupEnable(true);
	keyboard_init();
#endif

	printf("gfx init complete\n");

	atexit(_3ds_shutdown);

	gfxSet3D(true); // uncomment if using stereoscopic 3D

	myargc = argc;
	myargv = argv;
	H2_Main();

	exit(0);
	return 0;
}

int ms_to_next_tick = 0;

int I_GetTime(void)
{
#ifdef _3DS
	int t = osGetTime();
	int i = t*(TICRATE / 5) / 200;
	ms_to_next_tick = (i + 1) * 200 / (TICRATE / 5) - t;
	if (ms_to_next_tick > 1000 / TICRATE || ms_to_next_tick<1) ms_to_next_tick = 1;
	return i;
#else
	static int i = 0;
	return i++;
#endif
}

void I_WaitVBL(int vbls)
{
#ifdef _3DS
	gspWaitForVBlank();
#endif
}

void I_InitGraphics(void)
{
	//pcscreen = destscreen = (byte *)malloc(400*240*4);
	I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
}

byte pal3ds[256 * 3];

void I_SetPalette(byte *palette)
{
	int i;
	for (i = 0; i < 768; i++)
	{
		pal3ds[i] = gammatable[usegamma][*palette++];
	}
}

void I_NetCmd(void)
{
	if (!netgame)
		I_Error("I_NetCmd when not in netgame");
	//DPMIInt (doomcom->intnum);
}

void I_InitNetwork(void)
{
	int             i;

	i = M_CheckParm("-net");
	if (!i)
	{
		//
		// single player game
		//
		doomcom = malloc(sizeof(*doomcom));
		memset(doomcom, 0, sizeof(*doomcom));
		netgame = false;
		doomcom->id = DOOMCOM_ID;
		doomcom->numplayers = doomcom->numnodes = 1;
		doomcom->deathmatch = false;
		doomcom->consoleplayer = 0;
		doomcom->ticdup = 1;
		doomcom->extratics = 0;
		return;
	}

	netgame = true;
	doomcom = (doomcom_t *)atoi(myargv[i + 1]);
	//DEBUG
	doomcom->skill = startskill;
	doomcom->episode = startepisode;
	doomcom->map = startmap;
	doomcom->deathmatch = deathmatch;

}

/*
===============
=
= I_Init
=
= hook interrupts and set graphics mode
=
===============
*/

void I_Init(void)
{
	extern void I_StartupTimer(void);

	//novideo = M_CheckParm("novideo");
	//ST_Message("  I_StartupMouse ");
	//I_StartupMouse();
	ST_Message("S_Init... ");
	S_Init();
	S_Start();
}


/*
===============
=
= I_Shutdown
=
= return to default system state
=
===============
*/

void mus_exit();
void mix_exit();
void S_ShutDown(void)
{
	mus_exit();
	mix_exit();
	printf("sound shutdown complete\n");
#if 0
	extern int tsm_ID;
	if (tsm_ID != -1)
	{
		I_StopSong(RegisteredSong);
		I_UnRegisterSong(RegisteredSong);
		I_ShutdownSound();
	}
	if (i_CDMusic)
	{
		I_CDMusStop();
	}
#endif
}

boolean S_GetSoundPlayingInfo(mobj_t *mobj, int sound_id)
{
#if 0
	int i;

	for (i = 0; i < snd_Channels; i++)
	{
		if (Channel[i].sound_id == sound_id && Channel[i].mo == mobj)
		{
			if (I_SoundIsPlaying(Channel[i].handle))
			{
				return true;
			}
		}
	}
#endif
	return false;
}

void I_Shutdown(void)
{
	S_ShutDown();
}

/*
================
=
= I_Error
=
================
*/

void I_Error(char *error, ...)
{
	va_list argptr;

	D_QuitNetGame();
	I_Shutdown();
	va_start(argptr, error);
	vprintf(error, argptr);
	va_end(argptr);
	printf("\n");
	while (1);
	exit(1);
}

//--------------------------------------------------------------------------
//
// I_Quit
//
// Shuts down net game, saves defaults, prints the exit text message,
// goes to text mode, and exits.
//
//--------------------------------------------------------------------------

void I_Quit(void)
{
	D_QuitNetGame();
	M_SaveDefaults();
	I_Shutdown();

	printf("\nHexen: Beyond Heretic\n");
	exit(1);
}

/*
===============
=
= I_ZoneBase
=
===============
*/

byte *I_ZoneBase(int *size)
{
	int heap = 24*1024*1024;
	byte *ptr = malloc(heap);;

	*size = heap;
	return ptr;
}

/*
=============
=
= I_AllocLow
=
=============
*/

byte *I_AllocLow(int length)
{
	byte *mem = (byte *)malloc(length);

	memset(mem, 0, length);
	return mem;
}
/*
#define KBDQUESIZE 32
//byte keyboardque[KBDQUESIZE];
int kbdtail, kbdhead;

#define KEY_LSHIFT      0xfe

#define KEY_INS         (0x80+0x52)
#define KEY_DEL         (0x80+0x53)
#define KEY_PGUP        (0x80+0x49)
#define KEY_PGDN        (0x80+0x51)
#define KEY_HOME        (0x80+0x47)
#define KEY_END         (0x80+0x4f)

#define SC_RSHIFT       0x36
#define SC_LSHIFT       0x2a

#define SC_UPARROW              0x48
#define SC_DOWNARROW    0x50
#define SC_LEFTARROW            0x4b
#define SC_RIGHTARROW   0x4d

byte        scantokey[128] =
{
	//  0           1       2       3       4       5       6       7
	//  8           9       A       B       C       D       E       F
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    KEY_BACKSPACE, 9, // 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    13 ,    KEY_RCTRL,'a',  's',      // 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	39 ,    '`',    KEY_LSHIFT,92,  'z',    'x',    'c',    'v',      // 2
	'b',    'n',    'm',    ',',    '.',    '/',    KEY_RSHIFT,'*',
	KEY_RALT,' ',   0  ,    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,   // 3
	KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,0  ,    0  , KEY_HOME,
	KEY_UPARROW,KEY_PGUP,'-',KEY_LEFTARROW,'5',KEY_RIGHTARROW,'+',KEY_END, //4
	KEY_DOWNARROW,KEY_PGDN,KEY_INS,KEY_DEL,0,0,             0,              KEY_F11,
	KEY_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7
};
*/
void   I_StartTic(void)
{
	touchPosition touch;
	circlePosition nubPos = { 0, 0 };
	circlePosition cstickPos = { 0, 0 };
	int dx, dy;
	irrstCstickRead(&nubPos);
	if (abs(nubPos.dx) > 20 || abs(nubPos.dy) > 20) {
		event_t ev;
		dx = 0;
		dy = 0;
		if (abs(nubPos.dx) > 20) {
			dx = (nubPos.dx) * (nubSensitivity + 5) / 10;
		}
		if (abs(nubPos.dy) > 20) {
			dy = -(nubPos.dy) * (nubSensitivity + 5) / 10;
		}

		ev.type = ev_nub;
		ev.data1 = 0;
		ev.data2 = dx;
		ev.data3 = dy;
		H2_PostEvent(&ev);
	}

	circleRead(&cstickPos);
	if (abs(cstickPos.dx) > 20 || abs(cstickPos.dy) > 20) {
		event_t ev;
		dx = 0;
		dy = 0;
		if (abs(cstickPos.dx) > 20) {
			dx = (cstickPos.dx >> 2) * (cstickSensitivity + 5) / 10;
		}
		if (abs(cstickPos.dy) > 20) {
			dy = (cstickPos.dy >> 2) * (cstickSensitivity + 5) / 10;
		}

		ev.type = ev_cstick;
		ev.data1 = 0;
		ev.data2 = dx;
		ev.data3 = dy;
		H2_PostEvent(&ev);
	}
#if 0
	int             k;
	event_t ev;


	//I_ReadMouse();

	//
	// keyboard events
	//
	while (kbdtail < kbdhead)
	{
		k = keyboardque[kbdtail&(KBDQUESIZE - 1)];
		kbdtail++;
		// extended keyboard shift key bullshit
		if ((k & 0x7f) == SC_LSHIFT || (k & 0x7f) == SC_RSHIFT)
		{
			if (keyboardque[(kbdtail - 2)&(KBDQUESIZE - 1)] == 0xe0)
				continue;
			k &= 0x80;
			k |= SC_RSHIFT;
		}

		if (k == 0xe0)
			continue;               // special / pause keys
		if (keyboardque[(kbdtail - 2)&(KBDQUESIZE - 1)] == 0xe1)
			continue;                               // pause key bullshit

		if (k == 0xc5 && keyboardque[(kbdtail - 2)&(KBDQUESIZE - 1)] == 0x9d)
		{
			ev.type = ev_keydown;
			ev.data1 = KEY_PAUSE;
			H2_PostEvent(&ev);
			continue;
		}
		if (k & 0x80)
			ev.type = ev_keyup;
		else
			ev.type = ev_keydown;
		k &= 0x7f;
		switch (k)
		{
		case SC_UPARROW:
			ev.data1 = KEY_UPARROW;
			break;
		case SC_DOWNARROW:
			ev.data1 = KEY_DOWNARROW;
			break;
		case SC_LEFTARROW:
			ev.data1 = KEY_LEFTARROW;
			break;
		case SC_RIGHTARROW:
			ev.data1 = KEY_RIGHTARROW;
			break;
		default:
			ev.data1 = scantokey[k];
			break;
		}
		H2_PostEvent(&ev);
	}
#endif

}

void copy_screen(int side) {
#ifdef _3DS
	u16 screen_width, screen_height;
	u8* bufAdr = gfxGetFramebuffer(GFX_TOP, side, &screen_height, &screen_width);
	byte *src = screen;
	int w, h;

	for (w = 0; w<SCREENWIDTH; w++)
	{
		for (h = 0; h<SCREENHEIGHT; h++)
		{
			u32 v = (w * screen_height + h) * 3;
			u32 v1 = ((SCREENHEIGHT - h - 1) * SCREENWIDTH + w);
			bufAdr[v] = pal3ds[src[v1] * 3 + 2];
			bufAdr[v + 1] = pal3ds[src[v1] * 3 + 1];
			bufAdr[v + 2] = pal3ds[src[v1] * 3 + 0];
		}
	}
#endif
}

void copy_subscreen(int side) {
#ifdef _3DS
	u16 screen_width, screen_height;
	u16* bufAdr = (u16*)gfxGetFramebuffer(GFX_BOTTOM, side, &screen_height, &screen_width);
	byte *src = subscreen;
	int w, h;
	int rows_to_copy;
	int row_start;

	if (!automapactive || automapontop) {
		rows_to_copy = 66;
		row_start = 0;
	}
	else {
		rows_to_copy = 240;
		row_start = 0;
	}

	for (w = 0; w<320; w++)
	{
		for (h = 0; h<rows_to_copy; h++)
		{
			u32 v = (w * screen_height + h);
			u32 v1 = ((240 - h - 1) * SCREENWIDTH + w);
			switch (src[v1]) {
			case 0:
				bufAdr[v] = 0;
				break;
			default:
				bufAdr[v] = RGB8_to_565(pal3ds[src[v1] * 3 + 0], pal3ds[src[v1] * 3 + 1], pal3ds[src[v1] * 3 + 2]);
				break;
			}
		}
	}
#endif
}

/*
==============
=
= I_Update
=
==============
*/

int screen_side = 3; // 1 = left, 2 = right, 3 == both

void I_Update(void)
{
	int i;
	byte *dest;
	int tics;
	static int lasttic;

	//
	// blit screen to video
	//
#if 0
	if (DisplayTicker)
	{
		if (screenblocks > 9 || UpdateState&(I_FULLSCRN | I_MESSAGES))
		{
			dest = (byte *)screen;
		}
		else
		{
			dest = (byte *)pcscreen;
		}
		tics = ticcount - lasttic;
		lasttic = ticcount;
		if (tics > 20)
		{
			tics = 20;
		}
		for (i = 0; i < tics; i++)
		{
			*dest = 0xff;
			dest += 2;
		}
		for (i = tics; i < 20; i++)
		{
			*dest = 0x00;
			dest += 2;
		}
	}
#endif


	//memset(pcscreen, 255, SCREENHEIGHT*SCREENWIDTH);

	if (UpdateState == I_NOUPDATE)
	{
		return;
	}
#if 0
	if (UpdateState&I_FULLSCRN)
	{
		memcpy(pcscreen, screen, SCREENWIDTH*SCREENHEIGHT);
		memcpy(pcscreen, subscreen, SCREENWIDTH*SCREENHEIGHT);
		UpdateState = I_NOUPDATE; // clear out all draw types
	}
	if (UpdateState&I_FULLVIEW)
	{
		if (UpdateState&I_MESSAGES && screenblocks > 7)
		{
			for (i = 0; i <
				(viewwindowy + viewheight)*SCREENWIDTH; i += SCREENWIDTH)
			{
				memcpy(pcscreen + i, screen + i, SCREENWIDTH);
			}
			UpdateState &= ~(I_FULLVIEW | I_MESSAGES);
		}
		else
		{
			for (i = viewwindowy*SCREENWIDTH + viewwindowx; i <
				(viewwindowy + viewheight)*SCREENWIDTH; i += SCREENWIDTH)
			{
				memcpy(pcscreen + i, screen + i, viewwidth);
			}
			UpdateState &= ~I_FULLVIEW;
		}
	}
	if (UpdateState&I_STATBAR)
	{
		memcpy(pcscreen + SCREENWIDTH*(SCREENHEIGHT - SBARHEIGHT),
			screen + SCREENWIDTH*(SCREENHEIGHT - SBARHEIGHT),
			SCREENWIDTH*SBARHEIGHT);
		UpdateState &= ~I_STATBAR;
	}
	if (UpdateState&I_MESSAGES)
	{
		memcpy(pcscreen, screen, SCREENWIDTH * 28);
		UpdateState &= ~I_MESSAGES;
	}
#endif

	//  memcpy(pcscreen, screen, SCREENHEIGHT*SCREENWIDTH);

#ifdef _3DS
	if ((screen_side & 2) != 0) {
		copy_screen(GFX_RIGHT);
	}
	if ((screen_side & 1) != 0) {
		copy_screen(GFX_LEFT);
		keyboard_draw();
		copy_subscreen(GFX_LEFT);
		gfxFlushBuffers();
		gfxSwapBuffers();
	}
#endif
}

#ifdef _3DS
touchPosition	g_lastTouch = { 0, 0 };
touchPosition	g_currentTouch = { 0, 0 };

//0=DS Bit,1=game key, 2=menu key
int keys3ds[32][3] = {
	{ KEY_A,		KEY_RCTRL,		KEY_ENTER }, //bit 00
	{ KEY_B,		' ',			KEY_ESCAPE }, //bit 01
	{ KEY_SELECT,	KEY_ENTER,		0 }, //bit 02
	{ KEY_START,	KEY_ESCAPE,	0 }, //bit 03
	{ KEY_DRIGHT,	KEY_RIGHTARROW,0 }, //bit 04
	{ KEY_DLEFT,	KEY_LEFTARROW, 0 }, //bit 05
	{ KEY_DUP,		KEY_UPARROW,	0 }, //bit 06
	{ KEY_DDOWN,	KEY_DOWNARROW, 0 }, //bit 07
	{ KEY_R,		'.',			0 }, //bit 08
	{ KEY_L,		',',			0 }, //bit 09
	{ KEY_X,		'x',			0 }, //bit 10
	{ KEY_Y,		'y',			'y' }, //bit 11
	{ 0, 0, 0 }, //bit 12
	{ 0, 0, 0 }, //bit 13
	{ KEY_ZL,		'[',			0 }, //bit 14
	{ KEY_ZR,		']',			0 }, //bit 15
	{ 0, 0, 0 }, //bit 16
	{ 0, 0, 0 }, //bit 17
	{ 0, 0, 0 }, //bit 18
	{ 0, 0, 0 }, //bit 19
	{ 0, 0, 0 }, //bit 20
	{ 0, 0, 0 }, //bit 21
	{ 0, 0, 0 }, //bit 22
	{ 0, 0, 0 }, //bit 23
/*
	{ KEY_CSTICK_RIGHT,	KEYD_CSTICK_RIGHT,	0 }, //bit 24
	{ KEY_CSTICK_LEFT,	KEYD_CSTICK_LEFT,	0 }, //bit 25
	{ KEY_CSTICK_UP,	KEYD_CSTICK_UP,		0 }, //bit 26
	{ KEY_CSTICK_DOWN,	KEYD_CSTICK_DOWN,	0 }, //bit 27
	{ KEY_CPAD_RIGHT,	KEYD_CPAD_RIGHT,	0 }, //bit 28
	{ KEY_CPAD_LEFT,	KEYD_CPAD_LEFT,		0 }, //bit 29
	{ KEY_CPAD_UP,		KEYD_CPAD_UP,		0 }, //bit 30
	{ KEY_CPAD_DOWN,	KEYD_CPAD_DOWN,		0 }, //bit 31*/
};

void keyboard_input();

void DS_Controls(void) {
	int dx, dy;

	scanKeys();	// Do DS input housekeeping
	u32 keys = keysDown();
	u32 held = keysHeld();
	u32 up = keysUp();
	int i;
	boolean altmode = 0;// menuactive && !setup_select;


	for (i = 0; i<16; i++) {
		//send key down
		if (keys3ds[i][0] & keys) {
			event_t ev;
			ev.type = ev_keydown;
			ev.data1 = keys3ds[i][(altmode && keys3ds[i][2]) ? 2 : 1];
			H2_PostEvent(&ev);
		}
		//send key up
		if (keys3ds[i][0] & up) {
			event_t ev;
			ev.type = ev_keyup;
			ev.data1 = keys3ds[i][(altmode && keys3ds[i][2]) ? 2 : 1];
			H2_PostEvent(&ev);
		}
	}

	if (keysDown() & KEY_TOUCH)
	{
		touchRead(&g_lastTouch);
		g_lastTouch.px <<= 7;
		g_lastTouch.py <<= 7;
	}

	if (keysHeld() & KEY_TOUCH) // this is only for x axis
	{
		event_t event;

		touchRead(&g_currentTouch);// = touchReadXY();
								   // let's use some fixed point magic to improve touch smoothing accuracy
		g_currentTouch.px <<= 7;
		g_currentTouch.py <<= 7;

		dx = (g_currentTouch.px - g_lastTouch.px) >> 6;
		dy = (g_currentTouch.py - g_lastTouch.py) >> 6;

		event.type = ev_mouse;
		//event.data1 = I_SDLtoDoomMouseState(Event->motion.state);
		event.data1 = 0;
		event.data2 = (dx << 3) * (mouseSensitivity + 5) / 10;
		event.data3 = (dy >> 1) * (mouseSensitivity + 5) / 10;
		H2_PostEvent(&event);

		g_lastTouch.px = (g_lastTouch.px + g_currentTouch.px) / 2;
		g_lastTouch.py = (g_lastTouch.py + g_currentTouch.py) / 2;
	}
}
#else
void DS_Controls() {

}
void keyboard_input() {

}
#endif


/*
===============
=
= I_StartFrame
=
===============
*/

void I_StartFrame(void)
{
	//printf("+");
	DS_Controls();
	keyboard_input();
#if 0
	I_JoystickEvents();
	if (useexterndriver)
	{
		DPMIInt(i_Vector);
	}
#endif
}

//==========================================================================
//
// S_InitScript
//
//==========================================================================
static boolean UseSndScript;
static char ArchivePath[128];
#define DEFAULT_ARCHIVEPATH     "o:\\sound\\archive\\"
extern sfxinfo_t S_sfx[];
extern musicinfo_t S_music[];

void S_InitScript(void)
{
	int p;
	int i;

	strcpy(ArchivePath, DEFAULT_ARCHIVEPATH);
	//if (!(p = M_CheckParm("-devsnd")))
	//{
		UseSndScript = false;
		SC_OpenLump("sndinfo");
	//}
	//else
	//{
	//	UseSndScript = true;
	//	SC_OpenFile(myargv[p + 1]);
	//}
	while (SC_GetString())
	{
		if (*sc_String == '$')
		{
			if (!strcasecmp(sc_String, "$ARCHIVEPATH"))
			{
				SC_MustGetString();
				strcpy(ArchivePath, sc_String);
			}
			else if (!strcasecmp(sc_String, "$MAP"))
			{
				SC_MustGetNumber();
				SC_MustGetString();
				if (sc_Number)
				{
					P_PutMapSongLump(sc_Number, sc_String);
				}
			}
			continue;
		}
		else
		{
			for (i = 0; i < NUMSFX; i++)
			{
				if (!strcmp(S_sfx[i].tagName, sc_String))
				{
					SC_MustGetString();
					if (*sc_String != '?')
					{
						strcpy(S_sfx[i].lumpname, sc_String);
					}
					else
					{
						strcpy(S_sfx[i].lumpname, "default");
					}
					break;
				}
			}
			if (i == NUMSFX)
			{
				SC_MustGetString();
			}
		}
	}
	SC_Close();

	for (i = 0; i < NUMSFX; i++)
	{
		if (!strcmp(S_sfx[i].lumpname, ""))
		{
			strcpy(S_sfx[i].lumpname, "default");
		}
	}
}

#ifdef _WIN32
fixed_t D_abs(fixed_t x)
{
	fixed_t _t = (x), _s;
	_s = _t >> (8 * sizeof _t - 1);
	return (_t^_s) - _s;
}
/*
* Fixed Point Multiplication
*/

/* CPhipps - made __inline__ to inline, as specified in the gcc docs
* Also made const */

fixed_t FixedMul(fixed_t a, fixed_t b)
{
	return (fixed_t)((__int64)a*b >> FRACBITS);
}

/*
* Fixed Point Division
*/

/* CPhipps - made __inline__ to inline, as specified in the gcc docs
* Also made const */

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
	return (D_abs(a) >> 14) >= D_abs(b) ? ((a^b) >> 31) ^ MAXINT :
		(fixed_t)(((__int64)a << FRACBITS) / b);
}

#endif
