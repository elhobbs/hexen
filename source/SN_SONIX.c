
//**************************************************************************
//**
//** sn_sonix.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: sn_sonix.c,v $
//** $Revision: 1.17 $
//** $Date: 95/10/05 18:25:44 $
//** $Author: paul $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include "h2def.h"
#include "soundst.h"

// MACROS ------------------------------------------------------------------

#define SS_MAX_SCRIPTS	64
#define SS_TEMPBUFFER_SIZE	1024
#define SS_SEQUENCE_NAME_LENGTH 32

#define SS_SCRIPT_NAME "SNDSEQ"
#define SS_STRING_PLAY			"play"
#define SS_STRING_PLAYUNTILDONE "playuntildone"
#define SS_STRING_PLAYTIME		"playtime"
#define SS_STRING_PLAYREPEAT	"playrepeat"
#define SS_STRING_DELAY			"delay"
#define SS_STRING_DELAYRAND		"delayrand"
#define SS_STRING_VOLUME		"volume"
#define SS_STRING_END			"end"
#define SS_STRING_STOPSOUND		"stopsound"

// TYPES -------------------------------------------------------------------

typedef enum
{
	SS_CMD_NONE,
	SS_CMD_PLAY,
	SS_CMD_WAITUNTILDONE, // used by PLAYUNTILDONE
	SS_CMD_PLAYTIME,
	SS_CMD_PLAYREPEAT,
	SS_CMD_DELAY,
	SS_CMD_DELAYRAND,
	SS_CMD_VOLUME,
	SS_CMD_STOPSOUND,
	SS_CMD_END
} sscmds_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void VerifySequencePtr(int *base, int *ptr);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern sfxinfo_t S_sfx[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static struct
{
	char name[SS_SEQUENCE_NAME_LENGTH];
	int scriptNum;
	int stopSound;
} STRUCTSPACKED SequenceTranslate[SEQ_NUMSEQ] =
{
	{ "Platform", 0, 0 },
	{ "Platform", 0, 0 },   	// a 'heavy' platform is just a platform
	{ "PlatformMetal", 0, 0 }, 	
	{ "Platform", 0, 0 },		// same with a 'creak' platform
	{ "Silence", 0, 0 },
	{ "Lava", 0, 0 },
	{ "Water", 0, 0 },
	{ "Ice", 0, 0 },
	{ "Earth", 0, 0 },
	{ "PlatformMetal2", 0, 0 },
	{ "DoorNormal", 0, 0 },
	{ "DoorHeavy", 0, 0 },
	{ "DoorMetal", 0, 0 },
	{ "DoorCreak", 0, 0 },
	{ "Silence", 0, 0 },
	{ "Lava", 0, 0 },
	{ "Water", 0, 0},
	{ "Ice", 0, 0 },
	{ "Earth", 0, 0},
	{ "DoorMetal2", 0, 0 },
	{ "Wind", 0, 0 }
};

static int *SequenceData[SS_MAX_SCRIPTS];

int ActiveSequences;
seqnode_t *SequenceListHead = 0;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// VerifySequencePtr
//
//   Verifies the integrity of the temporary ptr, and ensures that the ptr
// 		isn't exceeding the size of the temporary buffer
//==========================================================================

static void VerifySequencePtr(int *base, int *ptr)
{
	if(ptr-base > SS_TEMPBUFFER_SIZE)
	{
		I_Error("VerifySequencePtr:  tempPtr >= %d\n", SS_TEMPBUFFER_SIZE);
	}
}

//==========================================================================
//
// GetSoundOffset
//
//==========================================================================

static int GetSoundOffset(char *name)
{
	int i;
	
	waitforit2("GetSoundOffset 1");
	for(i = 0; i < NUMSFX; i++)
	{
		//printf("tag: %s %s\n", name, S_sfx[i].tagName);
		if(!strcasecmp(name, S_sfx[i].tagName))
		{
			//printf("%d\n", i);
			waitforit2("GetSoundOffset 1a");
			return i;
		}
	}
	SC_ScriptError("GetSoundOffset:  Unknown sound name\n");
	return 0;
}

//==========================================================================
//
// SN_InitSequenceScript
//
//==========================================================================

int waitforit2(char *str) {
#if 0
	printf("%s ...", str);
	do {
		hidScanInput();
	} while ((hidKeysHeld() & KEY_A) == 0);
	do {
		hidScanInput();
	} while ((hidKeysHeld() & KEY_A) != 0);
	printf("done.\n");
#endif
	return 0;
}

void SN_InitSequenceScript(void)
{
	int i, j;
	int inSequence;
	int *tempDataStart;
	int *tempDataPtr;

	inSequence = -1;
	ActiveSequences = 0;
	for(i = 0; i < SS_MAX_SCRIPTS; i++)
	{
		SequenceData[i] = NULL;
	} 
	SC_Open(SS_SCRIPT_NAME);
	while(SC_GetString())
	{
		waitforit2("sc");
		if(*sc_String == ':')
		{
			if(inSequence != -1)
			{
				SC_ScriptError("SN_InitSequenceScript:  Nested Script Error");
			}
			tempDataStart = (int *)Z_Malloc(SS_TEMPBUFFER_SIZE, 
				PU_STATIC, NULL);
			memset(tempDataStart, 0, SS_TEMPBUFFER_SIZE);
			tempDataPtr = tempDataStart;
			for(i = 0; i < SS_MAX_SCRIPTS; i++)
			{
				if(SequenceData[i] == NULL)
				{
					break;
				}
			}
			if(i == SS_MAX_SCRIPTS)
			{
				I_Error("Number of SS Scripts >= SS_MAX_SCRIPTS");
			}
			for(j = 0; j < SEQ_NUMSEQ; j++)
			{
				if(!strcasecmp(SequenceTranslate[j].name, sc_String+1))
				{
					SequenceTranslate[j].scriptNum = i;
					inSequence = j;
					break;
				}
			}					
			continue; // parse the next command
		}
		if(inSequence == -1)
		{
			continue;
		}
		if(SC_Compare(SS_STRING_PLAYUNTILDONE))
		{
			int jj;
			waitforit2("sc 1");
			VerifySequencePtr(tempDataStart, tempDataPtr);
			waitforit2("sc 2");
			SC_MustGetString();
			waitforit2("sc 3");
			*tempDataPtr++ = SS_CMD_PLAY;
			jj = GetSoundOffset(sc_String);
			//printf("%08x %d\n", tempDataPtr, jj);
			waitforit2("sc 3a");
			*tempDataPtr++ = jj;
			*tempDataPtr++ = SS_CMD_WAITUNTILDONE;
			waitforit2("sc 4");
		}
		else if(SC_Compare(SS_STRING_PLAY))
		{
			VerifySequencePtr(tempDataStart, tempDataPtr);
			SC_MustGetString();
			*tempDataPtr++ = SS_CMD_PLAY;
			*tempDataPtr++ = GetSoundOffset(sc_String);
		}
		else if(SC_Compare(SS_STRING_PLAYTIME))
		{
			VerifySequencePtr(tempDataStart, tempDataPtr);
			SC_MustGetString();
			*tempDataPtr++ = SS_CMD_PLAY;
			*tempDataPtr++ = GetSoundOffset(sc_String);
			SC_MustGetNumber();
			*tempDataPtr++ = SS_CMD_DELAY;	
			*tempDataPtr++ = sc_Number;
		}
		else if(SC_Compare(SS_STRING_PLAYREPEAT))
		{
			VerifySequencePtr(tempDataStart, tempDataPtr);
			SC_MustGetString();
			*tempDataPtr++ = SS_CMD_PLAYREPEAT;
			*tempDataPtr++ = GetSoundOffset(sc_String);
		}
		else if(SC_Compare(SS_STRING_DELAY))
		{
			VerifySequencePtr(tempDataStart, tempDataPtr);
			*tempDataPtr++ = SS_CMD_DELAY;
			SC_MustGetNumber();
			*tempDataPtr++ = sc_Number;
		}
		else if(SC_Compare(SS_STRING_DELAYRAND))
		{
			VerifySequencePtr(tempDataStart, tempDataPtr);
			*tempDataPtr++ = SS_CMD_DELAYRAND;
			SC_MustGetNumber();
			*tempDataPtr++ = sc_Number;
			SC_MustGetNumber();
			*tempDataPtr++ = sc_Number;
		}
		else if(SC_Compare(SS_STRING_VOLUME))
		{
			VerifySequencePtr(tempDataStart, tempDataPtr);
			*tempDataPtr++ = SS_CMD_VOLUME;
			SC_MustGetNumber();
			*tempDataPtr++ = sc_Number;
		}
		else if(SC_Compare(SS_STRING_END))
		{
			int dataSize;

			*tempDataPtr++ = SS_CMD_END;
			dataSize = (tempDataPtr-tempDataStart)*sizeof(int);
			SequenceData[i] = (int *)Z_Malloc(dataSize, PU_STATIC,
				NULL);
			memcpy(SequenceData[i], tempDataStart, dataSize);
			Z_Free(tempDataStart);
			inSequence = -1;
		}
		else if(SC_Compare(SS_STRING_STOPSOUND))
		{
			SC_MustGetString();
			SequenceTranslate[inSequence].stopSound =
				GetSoundOffset(sc_String);
			*tempDataPtr++ = SS_CMD_STOPSOUND;
		}
		else
		{
			SC_ScriptError("SN_InitSequenceScript:  Unknown commmand.\n");
		}
	}
}

//==========================================================================
//
//  SN_StartSequence
//
//==========================================================================

void SN_StartSequence(mobj_t *mobj, int sequence)
{
	seqnode_t *node;
	
	SN_StopSequence(mobj); // Stop any previous sequence
	node = (seqnode_t *)Z_Malloc(sizeof(seqnode_t), PU_STATIC, NULL);
	node->sequencePtr = SequenceData[SequenceTranslate[sequence].scriptNum];
	node->sequence = sequence;
	node->mobj = mobj;
	node->delayTics = 0;
	node->stopSound = SequenceTranslate[sequence].stopSound;
	node->volume = 127; // Start at max volume

	if(!SequenceListHead)
	{
		SequenceListHead = node;
		node->next = node->prev = NULL;
	}
	else
	{
		SequenceListHead->prev = node;
		node->next = SequenceListHead;
		node->prev = NULL;
		SequenceListHead = node;
	}
	ActiveSequences++;
	return;
}

//==========================================================================
//
//  SN_StartSequenceName
//
//==========================================================================

void SN_StartSequenceName(mobj_t *mobj, char *name)
{
	int i;

	for(i = 0; i < SEQ_NUMSEQ; i++)
	{
		if(!strcmp(name, SequenceTranslate[i].name))
		{
			SN_StartSequence(mobj, i);
			return;
		}
	}
}

//==========================================================================
//
//  SN_StopSequence
//
//==========================================================================

void SN_StopSequence(mobj_t *mobj)
{
	seqnode_t *node;

	for(node = SequenceListHead; node; node = node->next)
	{
		if(node->mobj == mobj)
		{
			S_StopSound(mobj);
			if(node->stopSound)
			{
				//printf("SN_StopSequence\n");
				S_StartSoundAtVolume(mobj, node->stopSound, node->volume);
			}
			if(SequenceListHead == node)
			{
				SequenceListHead = node->next;
			}
			if(node->prev)
			{
				node->prev->next = node->next;
			}
			if(node->next)
			{
				node->next->prev = node->prev;
			}
			Z_Free(node);
			ActiveSequences--;
		}
	}
}

//==========================================================================
//
//  SN_UpdateActiveSequences
//
//==========================================================================

void SN_UpdateActiveSequences(void)
{
	seqnode_t *node;
	boolean sndPlaying;

	if(!ActiveSequences || paused)
	{ // No sequences currently playing/game is paused
		return;
	}
	for(node = SequenceListHead; node; node = node->next)
	{
		if(node->delayTics)
		{
			node->delayTics--;
			continue;
		}
		sndPlaying = S_GetSoundPlayingInfo(node->mobj, node->currentSoundID);
		switch(*node->sequencePtr)
		{
			case SS_CMD_PLAY:
				if(!sndPlaying)
				{
					node->currentSoundID = *(node->sequencePtr+1);
					//printf("SS_CMD_PLAY\n");
					S_StartSoundAtVolume(node->mobj, node->currentSoundID,
						node->volume);
				}
				node->sequencePtr += 2;
				break;
			case SS_CMD_WAITUNTILDONE:
				if(!sndPlaying)
				{
					node->sequencePtr++;
					node->currentSoundID = 0;
				}
				break;
			case SS_CMD_PLAYREPEAT:
				if(!sndPlaying)
				{
					node->currentSoundID = *(node->sequencePtr+1);
					//printf("SS_CMD_PLAYREPEAT\n");
					S_StartSoundAtVolume(node->mobj, node->currentSoundID,
						node->volume);
				}
				break;
			case SS_CMD_DELAY:
				node->delayTics = *(node->sequencePtr+1);
				node->sequencePtr += 2;
				node->currentSoundID = 0;
				break;
			case SS_CMD_DELAYRAND:
				node->delayTics = *(node->sequencePtr+1)+
					M_Random()%(*(node->sequencePtr+2)-*(node->sequencePtr+1));
				node->sequencePtr += 2;
				node->currentSoundID = 0;
				break;
			case SS_CMD_VOLUME:
				node->volume = (127*(*(node->sequencePtr+1)))/100;
				node->sequencePtr += 2;
				break;
			case SS_CMD_STOPSOUND:
				// Wait until something else stops the sequence
				break;
			case SS_CMD_END:
				SN_StopSequence(node->mobj);
				break;
			default:	
				break;
		}
	}
}

//==========================================================================
//
//  SN_StopAllSequences
//
//==========================================================================

void SN_StopAllSequences(void)
{
	seqnode_t *node;

	for(node = SequenceListHead; node; node = node->next)
	{
		node->stopSound = 0; // don't play any stop sounds
		SN_StopSequence(node->mobj);
	}
}
	
//==========================================================================
//
//  SN_GetSequenceOffset
//
//==========================================================================

int SN_GetSequenceOffset(int sequence, int *sequencePtr)
{
	return (sequencePtr-SequenceData[SequenceTranslate[sequence].scriptNum]);
}

//==========================================================================
//
//  SN_ChangeNodeData
//
// 	nodeNum zero is the first node
//==========================================================================

void SN_ChangeNodeData(int nodeNum, int seqOffset, int delayTics, int volume,
	int currentSoundID)
{
	int i;
	seqnode_t *node;

	i = 0;
	node = SequenceListHead;
	while(node && i < nodeNum)
	{
		node = node->next;
		i++;
	}
	if(!node)
	{ // reach the end of the list before finding the nodeNum-th node
		return;
	}
	node->delayTics = delayTics;
	node->volume = volume;
	node->sequencePtr += seqOffset;
	node->currentSoundID = currentSoundID;
}
