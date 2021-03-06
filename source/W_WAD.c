
//**************************************************************************
//**
//** w_wad.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: w_wad.c,v $
//** $Revision: 1.6 $
//** $Date: 95/10/06 20:56:47 $
//** $Author: cjr $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#ifdef NeXT
#include <libc.h>
#include <ctype.h>
#elif defined(_3DS)
#include <malloc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <malloc.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif
#include "h2def.h"



// MACROS ------------------------------------------------------------------

#ifdef NeXT
// NeXT doesn't need a binary flag in open call
#define O_BINARY 0
#define strcmpi strcasecmp
#endif

#define PACKEDATTR __attribute__((packed))

// TYPES -------------------------------------------------------------------

typedef struct
{
	char identification[4];
	int numlumps;
	int infotableofs;
} PACKEDATTR wadinfo_t;

typedef struct
{
	int filepos;
	int size;
	char name[8];
} PACKEDATTR filelump_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

lumpinfo_t *lumpinfo;
int numlumps = 0;
void **lumpcache = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lumpinfo_t *PrimaryLumpInfo;
static int PrimaryNumLumps;
static void **PrimaryLumpCache;
static lumpinfo_t *AuxiliaryLumpInfo;
static int AuxiliaryNumLumps;
static void **AuxiliaryLumpCache;
static int AuxiliaryHandle = 0;
boolean AuxiliaryOpened = false;

// CODE --------------------------------------------------------------------

#ifdef NeXT
//==========================================================================
//
// strupr
//
//==========================================================================

void strupr(char *s)
{
    while(*s)
	*s++ = toupper(*s);
}
#endif

//==========================================================================
//
// filelength
//
//==========================================================================
#ifndef _WIN32
int filelength(int handle)
{
    struct stat fileinfo;

    if(fstat(handle, &fileinfo) == -1)
	{
		I_Error("Error fstating");
	}
    return fileinfo.st_size;
}
#endif
//==========================================================================
//
// W_AddFile
//
// Files with a .wad extension are wadlink files with multiple lumps,
// other files are single lumps with the base filename for the lump name.
//
//==========================================================================

void W_AddFile(char *filename)
{
	wadinfo_t header;
	lumpinfo_t *lump_p;
	unsigned i, j;
	int handle, length, cb_read;
	int startlump;
	filelump_t *fileinfo, singleinfo;
	filelump_t *freeFileInfo = 0;

	if((handle = open(filename, O_RDONLY|O_BINARY)) == -1)
	{ // Didn't find file
		return;
	}
	startlump = numlumps;
	if(strcasecmp(filename+strlen(filename)-3, "wad"))
	{ // Single lump file
		fileinfo = &singleinfo;
		singleinfo.filepos = 0;
		singleinfo.size = LONG(filelength(handle));
		M_ExtractFileBase(filename, singleinfo.name);
		numlumps++;
	}
	else
	{ // WAD file
		cb_read = read(handle, &header, sizeof(header));
		if (cb_read != sizeof(header)) {
			I_Error("read:  %d %d\n", cb_read, sizeof(header));
		}
		if(strncmp(header.identification, "IWAD", 4))
		{
			if(strncmp(header.identification, "PWAD", 4))
			{ // Bad file id
				I_Error("Wad file %s doesn't have IWAD or PWAD id\n",
					filename);
			}
		}
		//printf("%d %d\n", header.numlumps, header.infotableofs);
		header.numlumps = LONG(header.numlumps);
		header.infotableofs = LONG(header.infotableofs);
		length = header.numlumps*sizeof(filelump_t);
		//printf("%d %d %d\n", header.numlumps, header.infotableofs, length);
		//		fileinfo = alloca(length);
		if(!(fileinfo = hmalloc(length)))
		{
			I_Error("W_AddFile:  fileinfo malloc failed\n");
		}
		freeFileInfo = fileinfo;
		lseek(handle, header.infotableofs, SEEK_SET);
		cb_read = read(handle, fileinfo, length);
		if (cb_read != length) {
			I_Error("read:  %d %d\n", cb_read, length);
		}
		numlumps += header.numlumps;
	}

	// Fill in lumpinfo
	//printf("%p %d\n", lumpinfo, numlumps);
	{
		lumpinfo_t *temp = hmalloc(numlumps * sizeof(lumpinfo_t));
		if (startlump > 0) {
			memcpy(temp, lumpinfo, startlump * sizeof(lumpinfo_t));
		}
		//printf("lumpinfo: %p\n", lumpinfo);
		hfree(lumpinfo);
		lumpinfo = temp;
		/*for (i = 0; i < header.numlumps; i++)
		{
			printf("%4d: %8s %p\n", i, fileinfo[i].name, &fileinfo[i]);
			if (i > 5) break;
		}*/
	}
	//lumpinfo = realloc(lumpinfo, numlumps*sizeof(lumpinfo_t));
	if ((((u32)lumpinfo) & 0x3) != 0) {
		I_Error("misaligned lumpinfo");
	}
	if(!lumpinfo)
	{
		I_Error("Couldn't realloc lumpinfo");
	}
	lump_p = &lumpinfo[startlump];
	//printf("%p %d %p %d\n", lumpinfo, startlump, fileinfo, numlumps);
	for(i = startlump,j=0; i < numlumps; i++, j++)
	{
		lumpinfo[i].handle = handle;
		lumpinfo[i].position = (fileinfo[j].filepos);
		lumpinfo[i].size = (fileinfo[j].size);
		memcpy(lumpinfo[i].name, fileinfo[j].name, 8);
		//printf("%4d: %8s %p %p\n", i, fileinfo[j].name, &fileinfo[j], &lumpinfo[i]);
		//if (i > 5) break;
	}
	if(freeFileInfo)
	{
		hfree(freeFileInfo);
	}
	//while (1);
}

//==========================================================================
//
// W_InitMultipleFiles
//
// Pass a null terminated list of files to use.  All files are optional,
// but at least one file must be found.  Lump names can appear multiple
// times.  The name searcher looks backwards, so a later file can
// override an earlier one.
//
//==========================================================================

void W_InitMultipleFiles(char **filenames)
{
	int size;

	// Open all the files, load headers, and count lumps
	numlumps = 0;
	lumpinfo = hmalloc(16); // Will be realloced as lumps are added

	for(; *filenames; filenames++)
	{
		W_AddFile(*filenames);
	}
	if(!numlumps)
	{
		I_Error("W_InitMultipleFiles: no files found");
	}

	// Set up caching
	size = numlumps*sizeof(*lumpcache);
	lumpcache = hmalloc(size);
	if(!lumpcache)
	{
		I_Error("Couldn't allocate lumpcache");
	}
	memset(lumpcache, 0, size);

	PrimaryLumpInfo = lumpinfo;
	PrimaryLumpCache = lumpcache;
	PrimaryNumLumps = numlumps;
}

//==========================================================================
//
// W_InitFile
//
// Initialize the primary from a single file.
//
//==========================================================================

void W_InitFile(char *filename)
{
	char *names[2];

	names[0] = filename;
	names[1] = NULL;
	W_InitMultipleFiles(names);
}

//==========================================================================
//
// W_OpenAuxiliary
//
//==========================================================================

void W_OpenAuxiliary(char *filename)
{
	int i;
	int size;
	wadinfo_t header;
	int handle;
	int length;
	filelump_t *fileinfo;
	filelump_t *sourceLump;
	lumpinfo_t *destLump;

	if(AuxiliaryOpened)
	{
		W_CloseAuxiliary();
	}
	if((handle = open(filename, O_RDONLY|O_BINARY)) == -1)
	{
		I_Error("W_OpenAuxiliary: %s not found.", filename);
		return;
	}
	AuxiliaryHandle = handle;
	read(handle, &header, sizeof(header));
	if(strncmp(header.identification, "IWAD", 4))
	{
		if(strncmp(header.identification, "PWAD", 4))
		{ // Bad file id
			I_Error("Wad file %s doesn't have IWAD or PWAD id\n",
				filename);
		}
	}
	header.numlumps = LONG(header.numlumps);
	header.infotableofs = LONG(header.infotableofs);
	length = header.numlumps*sizeof(filelump_t);
	fileinfo = Z_Malloc(length, PU_STATIC, 0);
	lseek(handle, header.infotableofs, SEEK_SET);
	read(handle, fileinfo, length);
	numlumps = header.numlumps;

	// Init the auxiliary lumpinfo array
	lumpinfo = Z_Malloc(numlumps*sizeof(lumpinfo_t), PU_STATIC, 0);
	sourceLump = fileinfo;
	destLump = lumpinfo;
	for(i = 0; i < numlumps; i++, destLump++, sourceLump++)
	{
		destLump->handle = handle;
		destLump->position = LONG(sourceLump->filepos);
		destLump->size = LONG(sourceLump->size);
		strncpy(destLump->name, sourceLump->name, 8);
	}
	Z_Free(fileinfo);

	// Allocate the auxiliary lumpcache array
	size = numlumps*sizeof(*lumpcache);
	lumpcache = Z_Malloc(size, PU_STATIC, 0);
	memset(lumpcache, 0, size);

	AuxiliaryLumpInfo = lumpinfo;
	AuxiliaryLumpCache = lumpcache;
	AuxiliaryNumLumps = numlumps;
	AuxiliaryOpened = true;
}

//==========================================================================
//
// W_CloseAuxiliary
//
//==========================================================================

void W_CloseAuxiliary(void)
{
	int i;

	if(AuxiliaryOpened)
	{
		W_UseAuxiliary();
		for(i = 0; i < numlumps; i++)
		{
			if(lumpcache[i])
			{
				Z_Free(lumpcache[i]);
			}
		}
		Z_Free(AuxiliaryLumpInfo);
		Z_Free(AuxiliaryLumpCache);
		W_CloseAuxiliaryFile();
		AuxiliaryOpened = false;
	}
	W_UsePrimary();
}

//==========================================================================
//
// W_CloseAuxiliaryFile
//
// WARNING: W_CloseAuxiliary() must be called before any further
// auxiliary lump processing.
//
//==========================================================================

void W_CloseAuxiliaryFile(void)
{
	if(AuxiliaryHandle)
	{
		close(AuxiliaryHandle);
		AuxiliaryHandle = 0;
	}
}

//==========================================================================
//
// W_UsePrimary
//
//==========================================================================

void W_UsePrimary(void)
{
	lumpinfo = PrimaryLumpInfo;
	numlumps = PrimaryNumLumps;
	lumpcache = PrimaryLumpCache;
}

//==========================================================================
//
// W_UseAuxiliary
//
//==========================================================================

void W_UseAuxiliary(void)
{
	if(AuxiliaryOpened == false)
	{
		I_Error("W_UseAuxiliary: WAD not opened.");
	}
	lumpinfo = AuxiliaryLumpInfo;
	numlumps = AuxiliaryNumLumps;
	lumpcache = AuxiliaryLumpCache;
}

//==========================================================================
//
// W_NumLumps
//
//==========================================================================

int	W_NumLumps(void)
{
	return numlumps;
}

//==========================================================================
//
// W_CheckNumForName
//
// Returns -1 if name not found.
//
//==========================================================================

int W_CheckNumForName(char *name)
{
	int align4[4];
	char *name8 = (char *)align4;
	int v1, v2;
	lumpinfo_t *lump_p;
	
	// Make the name into two integers for easy compares
	strncpy(name8, name, 8);
	name8[8] = 0; // in case the name was a full 8 chars
	strupr(name8); // case insensitive
	v1 = *(int *)name8;
	v2 = *(int *)&name8[4];

	// Scan backwards so patch lump files take precedence
	lump_p = lumpinfo+numlumps;
	while(lump_p-- != lumpinfo)
	{
		//printf("%8s %p\n", lump_p->name, lump_p->name);
		if(*(int *)lump_p->name == v1 && *(int *)&lump_p->name[4] == v2)
		{
			return lump_p-lumpinfo;
		}
	}
	return -1;
}

//==========================================================================
//
// W_GetNumForName
//
// Calls W_CheckNumForName, but bombs out if not found.
//
//==========================================================================

int	W_GetNumForName (char *name)
{
	int	i;

	i = W_CheckNumForName(name);
	if(i != -1)
	{
		return i;
	}
	I_Error("W_GetNumForName: %s not found (%d) %d!", name, numlumps, sizeof(lumpinfo_t));
	return -1;
}

//==========================================================================
//
// W_LumpLength
//
// Returns the buffer size needed to load the given lump.
//
//==========================================================================

int W_LumpLength(int lump)
{
	if(lump >= numlumps)
	{
		I_Error("W_LumpLength: %i >= numlumps", lump);
	}
	return lumpinfo[lump].size;
}

//==========================================================================
//
// W_ReadLump
//
// Loads the lump into the given buffer, which must be >= W_LumpLength().
//
//==========================================================================

void W_ReadLump(int lump, void *dest)
{
	int c;
	lumpinfo_t *l;

	if(lump >= numlumps)
	{
		I_Error("W_ReadLump: %i >= numlumps", lump);
	}
	l = lumpinfo+lump;
	//I_BeginRead();
	lseek(l->handle, l->position, SEEK_SET);
	c = read(l->handle, dest, l->size);
	if(c < l->size)
	{
		I_Error("W_ReadLump: only read %i of %i on lump %i",
			c, l->size, lump);
	}
	//I_EndRead();
}

//==========================================================================
//
// W_CacheLumpNum
//
//==========================================================================

void *W_CacheLumpNum(int lump, int tag)
{
	byte *ptr;

	if((unsigned)lump >= numlumps)
	{
		I_Error("W_CacheLumpNum: %i >= numlumps", lump);
	}
	if(!lumpcache[lump])
	{ // Need to read the lump in
		ptr = Z_Malloc(W_LumpLength(lump), tag, &lumpcache[lump]);
		W_ReadLump(lump, lumpcache[lump]);
	}
	else
	{
		Z_ChangeTag(lumpcache[lump], tag);
	}
	return lumpcache[lump];
}

//==========================================================================
//
// W_CacheLumpName
//
//==========================================================================

void *W_CacheLumpName(char *name, int tag)
{
	return W_CacheLumpNum(W_GetNumForName(name), tag);
}

//==========================================================================
//
// W_Profile
//
//==========================================================================

// Ripped out for Heretic
/*
int	info[2500][10];
int	profilecount;

void W_Profile (void)
{
	int		i;
	memblock_t	*block;
	void	*ptr;
	char	ch;
	FILE	*f;
	int		j;
	char	name[9];
	
	
	for (i=0 ; i<numlumps ; i++)
	{	
		ptr = lumpcache[i];
		if (!ptr)
		{
			ch = ' ';
			continue;
		}
		else
		{
			block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
			if (block->tag < PU_PURGELEVEL)
				ch = 'S';
			else
				ch = 'P';
		}
		info[i][profilecount] = ch;
	}
	profilecount++;
	
	f = fopen ("waddump.txt","w");
	name[8] = 0;
	for (i=0 ; i<numlumps ; i++)
	{
		memcpy (name,lumpinfo[i].name,8);
		for (j=0 ; j<8 ; j++)
			if (!name[j])
				break;
		for ( ; j<8 ; j++)
			name[j] = ' ';
		fprintf (f,"%s ",name);
		for (j=0 ; j<profilecount ; j++)
			fprintf (f,"    %c",info[i][j]);
		fprintf (f,"\n");
	}
	fclose (f);
}
*/
