//===============================================================================================
// SIMPLE.EXE
// Copyright (c), Firelight Technologies, 2000-2004.
//
// This is a simple but descriptive way to get FMOD going, by loading a song and a few wav files
// and then playing them back.  It also shows how to do device enumeration and how to tweak
// various playback settings.
//
//===============================================================================================

#include <cstdio>
#include <cstdlib>
#ifdef WIN32
#include <conio.h>
#endif

#define USEMEMLOAD
//#define USEMEMLOADRESOURCE

#ifdef USEMEMLOAD
#include <cstring>
#endif

#ifdef USEMEMLOADRESOURCE
#define NOMINMAX
#include <Windows.h>
#endif

#include <minifmod/minifmod.h>

// this is if you want to replace the samples with your own (in case you have compressed them)
void sampleloadcallback(void *buff, int lenbytes, int numbits, int instno, int sampno)
{
	printf("pointer = %p length = %d bits = %d instrument %d sample %d\n", buff, lenbytes, numbits, instno, sampno);
}

#ifndef USEMEMLOAD

void* fileopen(const char *name)
{
	return fopen(name, "rb");
}

void fileclose(void* handle)
{
	fclose((FILE *)handle);
}

int fileread(void *buffer, int size, void* handle)
{
	return fread(buffer, 1, size, (FILE *)handle);
}

void fileseek(void* handle, int pos, int mode)
{
	fseek((FILE *)handle, pos, mode);
}

int filetell(void* handle)
{
	return ftell((FILE *)handle);
}

#else

struct MEMFILE
{
	int length;
	int pos;
	void *data;
};

void* memopen(const char *name)
{
    MEMFILE* memfile = (MEMFILE*)calloc(sizeof(MEMFILE), 1);

#ifndef USEMEMLOADRESOURCE
    {
        // load an external file and read it
        if (FILE* fp = fopen(name, "rb"))
		{
			fseek(fp, 0, SEEK_END);
			memfile->length = ftell(fp);
			memfile->data = calloc(memfile->length,1);
			memfile->pos = 0;

			fseek(fp, 0, SEEK_SET);
			fread(memfile->data, 1, memfile->length, fp);
			fclose(fp);
		}
	}
#else
	{	// hey look some load from resource code!

        HRSRC rec = FindResource(NULL, name, RT_RCDATA);
		HGLOBAL handle = LoadResource(NULL, rec);

		memfile->data = LockResource(handle);
		memfile->length = SizeofResource(NULL, rec);
		memfile->pos = 0;
	}
#endif

	return memfile;
}

void memclose(void* handle)
{
	MEMFILE *memfile = (MEMFILE *)handle;

#ifndef USEMEMLOADRESOURCE
	free(memfile->data);			// dont free it if it was initialized with LockResource
#endif

	free(memfile);
}

int memread(void *buffer, int size, void* handle)
{
	MEMFILE *memfile = (MEMFILE *)handle;

	if (memfile->pos + size >= memfile->length)
		size = memfile->length - memfile->pos;

	memcpy(buffer, (char *)memfile->data+memfile->pos, size);
	memfile->pos += size;

	return size;
}

void memseek(void* handle, int pos, int mode)
{
	MEMFILE *memfile = (MEMFILE *)handle;

	if (mode == SEEK_SET)
		memfile->pos = pos;
	else if (mode == SEEK_CUR)
		memfile->pos += pos;
	else if (mode == SEEK_END)
		memfile->pos = memfile->length + pos;

	if (memfile->pos > memfile->length)
		memfile->pos = memfile->length;
}

int memtell(void* handle)
{
    const MEMFILE *memfile = (MEMFILE *)handle;

	return memfile->pos;
}
#endif

/*
void songcallback(FMUSIC_MODULE *mod, unsigned char param)
{
	printf("order = %d, row = %d      \r", FMUSIC_GetOrder(mod), FMUSIC_GetRow(mod));
}
*/

/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
int main(int argc, char *argv[])
{
#ifndef USEMEMLOAD
	FSOUND_File_SetCallbacks(fileopen, fileclose, fileread, fileseek, filetell);
#else
	FSOUND_File_SetCallbacks(memopen, memclose, memread, memseek, memtell);
#endif

	if (argc < 2)
	{
		printf("-------------------------------------------------------------\n");
		printf("MINIFMOD example XM player.\n");
		printf("Copyright (c) Firelight Technologies, 2000-2004.\n");
		printf("-------------------------------------------------------------\n");
		printf("Syntax: simplest infile.xm\n\n");
		return 0;
	}

	// ==========================================================================================
	// LOAD SONG
	// ==========================================================================================
	FMUSIC_MODULE* mod = FMUSIC_LoadSong(argv[1], nullptr); //sampleloadcallback);
	if (!mod)
	{
		printf("Error loading song\n");
		return 0;
	}

	// ==========================================================================================
	// PLAY SONG
	// ==========================================================================================

    FMUSIC_PlaySong(mod);

	printf("Press any key to quit\n");
	printf("=========================================================================\n");
	printf("Playing song...\n");

	{
		int key = 0;
		do
		{
#ifdef WIN32
			if (kbhit())
			{
				key = getch();
			}
#endif
            const int ord = FMUSIC_GetOrder();
			const int row = FMUSIC_GetRow();
			const float mytime = (float)FMUSIC_GetTime() / 1000.0f;

			printf("ord %2d row %2d seconds %5.02f %s      \r", ord, row, mytime, (row % 8 ? "    " : "TICK"));

		} while (key != 27);
	}

	printf("\n");

	FMUSIC_FreeSong(mod);
}
