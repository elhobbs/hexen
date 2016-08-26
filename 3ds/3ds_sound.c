#ifdef _3DS
#include <3ds.h>
#endif
#include "H2DEF.H"
#include "SOUNDST.H"
#include "musplay.h"

extern sfxinfo_t S_sfx[];
extern musicinfo_t S_music[];

// music currently being played
static int mus_playing = 0;


int snd_MaxVolume = 0;      // maximum volume for sound
int snd_MusicVolume = 0;    // maximum volume for music

#define	WAV_BUFFERS				128
#define	WAV_MASK				0x7F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

static int snd_channels = 2;
static int snd_samplebits = 16;
static int snd_speed = 11025;
static int snd_samples;
static int snd_samplepos;
static byte* snd_buffer;



static boolean	snd_init = false;
static boolean	snd_firsttime = true;

static int	sample16;
static int	snd_sent, snd_completed;

static int	gSndBufSize = 0;
#ifdef _3DS
static ndspWaveBuf gWavebuf[WAV_BUFFERS];
#endif
static float gMix[12];

static int snd_scaletable[32][256];
#ifdef _WIN32
#define linearAlloc malloc
#endif

static void dsp_init() {
	int i;

	snd_sent = 0;
	snd_completed = 0;

	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	void *lpData = linearAlloc(gSndBufSize);
	if (!lpData)
	{
		printf("Sound: Out of memory.\n");
		return;
	}
	memset(lpData, 0, gSndBufSize);
#ifdef _3DS
	ndspInit();
	ndspChnSetInterp(0, NDSP_INTERP_NONE);
	ndspChnSetRate(0, (float)snd_speed);
	ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
	memset(gWavebuf, 0, sizeof(ndspWaveBuf)*WAV_BUFFERS);
	memset(gMix, 0, sizeof(gMix));
	DSP_FlushDataCache(lpData, gSndBufSize);
	
	for (i = 0; i<WAV_BUFFERS; i++)
	{
		gWavebuf[i].nsamples = WAV_BUFFER_SIZE / (snd_channels * snd_samplebits / 8);
		gWavebuf[i].looping = false;
		gWavebuf[i].status = NDSP_WBUF_FREE;
		gWavebuf[i].data_vaddr = lpData + i*WAV_BUFFER_SIZE;
	}
#endif
	snd_samples = gSndBufSize / (snd_samplebits / 8);
	snd_samplepos = 0;
	snd_buffer = (byte *)lpData;
	sample16 = (snd_samplebits / 8) - 1;

	snd_init = true;
}

static void dsp_shutdown() {
#ifdef _3DS
	ndspChnWaveBufClear(0);
	svcSleepThread(20000);
	ndspExit();
#endif
	snd_init = false;
}

void mix_exit() {
	dsp_shutdown();
}

static int dsp_dmapos(void)
{
	int		s;

	if (!snd_init)
	{
		return 0;
	}
	s = snd_sent * WAV_BUFFER_SIZE;

	s >>= sample16;

	s &= (snd_samples - 1);

	return s;
}
static void dsp_submit(void)
{
	if (!snd_init)
		return;

	//
	// find which sound blocks have completed
	//
	while (1)
	{
		if (snd_completed == snd_sent)
		{
			//printf("Sound overrun\n");
			break;
		}
#ifdef _3DS
		if (gWavebuf[snd_completed & WAV_MASK].status != NDSP_WBUF_DONE) {
			break;
		}
#endif
		snd_completed++;	// this buffer has been played
	}

	//
	// submit two new sound blocks
	//
	while (((snd_sent - snd_completed) >> sample16) < 4)
	{
#ifdef _3DS
		//h = lpWaveHdr + (snd_sent&WAV_MASK);
		ndspChnWaveBufAdd(0, gWavebuf + (snd_sent&WAV_MASK));
#endif
		snd_sent++;
		/*
		* Now the data block can be sent to the output device. The
		* waveOutWrite function returns immediately and waveform
		* data is sent to the output device in the background.
		*/

	}
	//printf("%d %d\n", snd_sent, snd_completed);
}

void S_Start(void)
{
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
#define S_CLOSE_DIST (160 * FRACUNIT)
#define S_CLIPPING_DIST (1200 * FRACUNIT)
#define S_STEREO_SWING (96 * FRACUNIT)
#define S_ATTENUATOR ((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)


#define PRIORITY_MAX_ADJUST 10
#define DIST_ADJUST (MAX_SND_DIST/PRIORITY_MAX_ADJUST)

//#define MAX_CHANNELS	23

#define NORM_PITCH 128
#define NORM_PRIORITY 64
#define NORM_SEP 128

static channel_t channel[MAX_CHANNELS];

#define	PAINTBUFFER_SIZE	512
typedef struct
{
	int left;
	int right;
} portable_samplepair_t;
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];

static int soundtime;
static int paintedtime;




void S_StartSound(mobj_t *origin, int sound_id)
{
	S_StartSoundAtVolume(origin, sound_id, 127);
}
int S_GetSoundID(char *name)
{
	int i;

	for (i = 0; i < NUMSFX; i++)
	{
		if (!strcmp(S_sfx[i].tagName, name))
		{
			return i;
		}
	}
	return 0;
}
static void I_StopChannel(int cnum)
{
	int i;
	channel_t *c = &channel[cnum];

	if (!snd_init)
		return;

	if (c && c->sfxinfo)
	{
		c->sfxinfo = 0;
	}
}

//
// Changes volume and stereo-separation variables
//  from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//

static int I_AdjustSoundParams(
	mobj_t *listener, mobj_t *source,
	int volume,
	int *vol, int *sep, int *dist)
{
	extern int snd_MaxVolume;
	fixed_t        approx_dist;
	fixed_t        adx;
	fixed_t        ady;
	angle_t        angle;

	// calculate the distance to sound origin
	//  and clip it if necessary
	adx = abs(listener->x - source->x);
	ady = abs(listener->y - source->y);

	// From _GG1_ p.428. Appox. eucledian distance fast.
	approx_dist = adx + ady - ((adx < ady ? adx : ady) >> 1);

	if (approx_dist > S_CLIPPING_DIST)
	{
		return 0;
	}

	// angle of source to listener
	angle = R_PointToAngle2(listener->x,
		listener->y,
		source->x,
		source->y);

	if (angle > listener->angle)
	{
		angle = angle - listener->angle;
	}
	else
	{
		angle = angle + (0xffffffff - listener->angle);
	}

	angle >>= ANGLETOFINESHIFT;

	// stereo separation
	*sep = 128 - (FixedMul(S_STEREO_SWING, finesine[angle]) >> FRACBITS);
	*dist = approx_dist >> FRACBITS;

	// volume calculation
	if (approx_dist < S_CLOSE_DIST)
	{
		*vol = volume;
	}
	else
	{
		// distance effect
		*vol = (volume
			* ((S_CLIPPING_DIST - approx_dist) >> FRACBITS))
			/ S_ATTENUATOR;
	}

	return (*vol > 0);
}
static int I_GetChannel(mobj_t *origin, sfxinfo_t *sfxinfo)
{
	int cnum;
	channel_t *c;

	if (!snd_init) {
		return -1;
	}

	for (cnum = 0; cnum < MAX_CHANNELS && channel[cnum].sfxinfo; cnum++);

	if (cnum == MAX_CHANNELS) {
		// Look for lower priority
		for (cnum = 0; cnum < MAX_CHANNELS; cnum++) {
			if (channel[cnum].sfxinfo->priority >= sfxinfo->priority) {
				break;
			}
		}
		if (cnum == MAX_CHANNELS) {
			return -1;
		}
		else {
			I_StopChannel(cnum);
		}
	}

	c = &channel[cnum];
	c->sfxinfo = sfxinfo;
	c->mo = origin;

	return cnum;
}
static int I_GetSfxLumpNum(sfxinfo_t *sound)
{
	//printf("I_GetSfxLumpNum: %8s\n", sound->lumpname);
	return W_GetNumForName(sound->lumpname);

}
static void I_StartSound(mobj_t *origin, sfxinfo_t *sfx, int cnum, int volume, int vol, int sep, int pitch, int priority)
{
	byte *data;
	int lump;
	int cached;
	int sfx_len;
	channel_t *ch;
	size_t len;

	if ((cnum < 0) || (cnum >= MAX_CHANNELS)) {
#ifdef RANGECHECK
		I_Error("I_StartSound: handle out of range");
#else
		return;
#endif
	}

	lump = sfx->lumpnum;

	len = W_LumpLength(lump);

	if (len <= 8) return;

	len -= 8;

	data = sfx->snd_ptr;
	sfx_len = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];
	if (data[0] != 0x03 || data[1] != 0x00 || sfx_len > len || sfx_len <= 48) {
		return;
	}
	ch = &channel[cnum];
	ch->pitch = pitch;
	if (origin == 0) {
		ch->left = vol;
		ch->right = vol;
	}
	else {
		ch->left = ((255 - sep) * vol) / 127;
		ch->right = ((sep)* vol) / 127;
	}
	ch->end = paintedtime + len;
	ch->pos = 0;
	ch->sfxinfo = sfx;
	ch->mo = origin;
	ch->volume = volume;
	//printf("%8s %08x %3d %3d - %3d %3d\n",sfx->lumpname, origin, sep, vol, ch->left, ch->right);
}
void S_StartSoundAtVolume(mobj_t *origin, int sound_id, int volume)
{
	int vol, sep, pitch, priority, cnum, is_pickup, dist;
	sfxinfo_t *sfx;

	if (!snd_init) {
		return;
	}

	if (sound_id < 1 || sound_id > NUMSFX)
		I_Error("S_StartSoundAtVolume: Bad sound_id: %d", sound_id);

	if (volume == 0)
	{
		return;
	}
	sfx = &S_sfx[sound_id];

	sep = NORM_SEP;
	vol = volume;
	if (!origin || origin == players[displayplayer].mo) {
		sep = NORM_SEP;
		//volume *= 8;
	}
	else {
		if (!I_AdjustSoundParams(players[displayplayer].mo, origin, volume, &vol, &sep, &dist)) {
			return;
		}
		else {
			if (origin->x == players[displayplayer].mo->x &&
				origin->y == players[displayplayer].mo->y)
				sep = NORM_SEP;
		}
	}
	if (sfx->changePitch)
	{
		pitch = (127 + (M_Random() & 7) - (M_Random() & 7));
	}
	else
	{
		pitch = 127;
	}
	priority = sfx->priority;
	priority *= (PRIORITY_MAX_ADJUST - (dist / DIST_ADJUST));

	// kill old sound
	for (cnum = 0; cnum < MAX_CHANNELS; cnum++) {
		if (channel[cnum].sfxinfo && channel[cnum].mo == origin)
		{
			I_StopChannel(cnum);
			break;
		}
	}

	// try to find a channel
	cnum = I_GetChannel(origin, sfx);
	if (cnum < 0)
		return;
	if (sfx->lumpnum == 0)
	{
		//printf("sound_id: %d\n", sound_id);
		sfx->lumpnum = I_GetSfxLumpNum(sfx);
	}
	sfx->snd_ptr = W_CacheLumpNum(sfx->lumpnum,PU_CACHE);
	
	I_StartSound(origin, sfx, cnum, volume, vol, sep, pitch, priority);
}
void S_StopSound(mobj_t *origin)
{

}
void S_StopAllSound(void)
{

}
void S_PauseSound(void)
{

}
void S_ResumeSound(void)
{

}
static void I_StopSong() {
	if (mus_playing > 0)
	{
		//if (mus_paused)
		//	I_ResumeSong(mus_playing->handle);

		mus_stop_music();
		W_CacheLumpNum(mus_playing, PU_CACHE);

		mus_playing = 0;
	}
}
void S_StartSong(int song, boolean loop)
{
	//printf("S_StartSong: %d\n", song);
	char *songLump = P_GetMapSongLump(song);
	S_StartSongName(songLump, loop);
}
void S_StartSongName(char *songLump, boolean loop)
{
	if (songLump == 0) {
		return;
	}
	//printf("S_StartSongName: %s\n", songLump);
	int lumpnum = W_GetNumForName(songLump);
	if (lumpnum <= 0 || lumpnum == mus_playing) {
		return;
	}
	I_StopSong();
	u8 *data = (u8*)W_CacheLumpNum(lumpnum, PU_MUSIC);
	if (data) {
		mus_play_music(data);
		mus_playing = lumpnum;
	}
	else {
		printf("failed to find music data\n");
		mus_playing = 0;
	}
	S_SetMusicVolume();
}

void S_InitScaletable(void)
{
	int		i, j;

	for (i = 0; i<32; i++)
		for (j = 0; j<256; j++)
			snd_scaletable[i][j] = ((signed char)j) * i * 8;
}

void S_Init(void)
{
	if (snd_firsttime) {
		dsp_init();
		S_InitScaletable();
		S_SetSfxVolume();
		mus_init();
		snd_firsttime = false;
	}
}
void S_GetChannelInfo(SoundInfo_t *s)
{

}
void S_SetMusicVolume(void)
{
	mus_update_volume();
}

void S_SetSfxVolume(void)
{
	float vol = (float)snd_MaxVolume / 15.0f;
	gMix[0] = vol;
	gMix[1] = vol;
	ndspChnSetMix(0, gMix);
}

static void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	fullsamples = snd_samples / snd_channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = dsp_dmapos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped

		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSound();
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos / snd_channels;
}
static void I_PaintChannelFrom8(channel_t *ch, sfxinfo_t *sfx, int count)
{
	int 	data;
	int		*lscale, *rscale;
	unsigned char *buf;
	int		i;

	if (ch->left > 255)
		ch->left = 255;
	if (ch->right > 255)
		ch->right = 255;

	lscale = snd_scaletable[ch->left >> 3];
	rscale = snd_scaletable[ch->right >> 3];
	buf = ((signed char *)sfx->snd_ptr) + 8 + ch->pos;

	for (i = 0; i<count; i++)
	{
		data = (int)(buf[i] ^ 0x80);
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}

	ch->pos += count;
}
static int 	*snd_p, snd_linear_count, snd_vol;
static short	*snd_out;
static void I_WriteLinearBlastStereo16(void)
{
	int		i;
	int		val;

	for (i = 0; i<snd_linear_count; i += 2)
	{
		val = (snd_p[i] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i + 1] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i + 1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i + 1] = (short)0x8000;
		else
			snd_out[i + 1] = val;
	}
}
static void I_TransferStereo16(int endtime)
{
	int		lpos;
	int		lpaintedtime;

	snd_vol = 256;// volume.value * 256;

	snd_p = (int *)paintbuffer;
	lpaintedtime = paintedtime;

	while (lpaintedtime < endtime)
	{
		// handle recirculating buffer issues
		lpos = lpaintedtime & ((snd_samples >> 1) - 1);

		snd_out = (short *)snd_buffer + (lpos << 1);

		snd_linear_count = (snd_samples >> 1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		I_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count >> 1);
	}

}

static void I_UpdateChannels() {
	int 	i, sep, vol, dist;
	channel_t *ch;

	// paint in the channels.
	ch = channel;
	for (i = 0; i < MAX_CHANNELS; i++, ch++)
	{
		if (!ch->sfxinfo || 
			!ch->mo ||
			ch->mo == players[displayplayer].mo) {
			continue;
		}
		if (paintedtime >= ch->end)
		{
			ch->mo = 0;
			ch->sfxinfo = 0;
			//printf("sound-a ended %d %d\n", ltime, ch->end);
			continue;
		}
		sep = NORM_SEP;
		if (!I_AdjustSoundParams(players[displayplayer].mo, ch->mo, ch->volume, &vol, &sep, &dist)) {
			ch->left = 0;
			ch->right = 0;
		}
		else {
			if (ch->mo->x == players[displayplayer].mo->x &&
				ch->mo->y == players[displayplayer].mo->y)
				sep = NORM_SEP;
			ch->left = ((255 - sep) * ch->volume) / 127;
			ch->right = ((sep)* ch->volume) / 127;
		}
	}
}
static void I_PaintChannels(int endtime)
{
	int 	i;
	int 	end;
	channel_t *ch;
	sfxinfo_t *sfx;
	int		ltime, count, mixed = 0;

	while (paintedtime < endtime)
	{
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));

		// paint in the channels.
		ch = channel;
		for (i = 0; i<MAX_CHANNELS; i++, ch++)
		{
			if (!ch->sfxinfo) {
				continue;
			}
			if (paintedtime >= ch->end)
			{
				ch->mo = 0;
				ch->sfxinfo = 0;
				//printf("sound-a ended %d %d\n", ltime, ch->end);
				continue;
			}
			if (ch->left <= 0 && ch->right <= 0) {
				continue;
			}

			sfx = ch->sfxinfo;
			ltime = paintedtime;
			//printf("mixing %d %d %8s\n", ltime, ch->end, sfx->lumpname);
			//printf("mixing: %8s %d %d\n", sfx->lumpname, ch->left, ch->right);
			//if (sfx->snd_ptr == NULL)
			//{
				sfx->snd_ptr = W_CacheLumpNum(sfx->lumpnum, PU_CACHE);
			//}

			while (ltime < end)
			{	// paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0)
				{
					I_PaintChannelFrom8(ch, sfx, count);
					mixed++;
					ltime += count;
				}
				if (ltime >= ch->end)
				{
					ch->mo = 0;
					ch->sfxinfo = 0;
					//printf("sound-b ended %d %d\n", ltime, ch->end);
					break;
				}
			}
		}

		// transfer out according to DMA format
		I_TransferStereo16(end);
		paintedtime = end;
	}

	//printf("mixed: %d\n", mixed);
}

void S_Update() {
	unsigned        endtime;
	int				samps;

	if (!snd_init) {
		return;
	}

	// Updates DMA time
	GetSoundtime();

	// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		//Con_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	// mix ahead of current position
	endtime = soundtime + 0.1f * snd_speed;
	samps = snd_samples >> (snd_channels - 1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	//printf("%10d %10d %10d\n", soundtime, paintedtime, endtime);
	I_PaintChannels(endtime);

	dsp_submit();
}
void mus_stats();
void mus_dsp_submit();
void S_UpdateSounds(mobj_t *listener)
{
	I_UpdateChannels();
	S_Update();
	//mus_stats();
	mus_dsp_submit();
}

#ifdef _WIN32
void mus_init() {
}
void mus_exit() {
}
void mus_play_music(u8 *data) {
}
void mus_stop_music() {
}
void mus_update_volume() {
}
void mus_dsp_submit(void) {
}
#endif
