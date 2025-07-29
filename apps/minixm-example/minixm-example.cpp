//===============================================================================================
// minixm-example
// Pan/SpinningKids, 2022-2024.
//
// This is a simple but descriptive way to get minixm going, by loading a song and a few wav 
// files and then playing them back.  It also shows how to do device enumeration and how to tweak
// various playback settings.
//
//===============================================================================================

#include <cstdio>
#include <cstdlib>
#ifdef WIN32
#include <conio.h>
#endif

//#define USEMEMLOAD
//#define USEMEMLOADRESOURCE

#ifdef USEMEMLOAD
#include <cstring>
#endif

#ifdef USEMEMLOADRESOURCE
#define NOMINMAX
#include <Windows.h>
#endif

#include <minixm/system_file.h>
#include <minixm/module.h>
#include <minixm/player_state.h>

#include <winmm_playback/winmm_playback.h>

// this is if you want to replace the samples with your own (in case you have compressed them)
void sample_load_callback(void* buff, int lenbytes, int numbits, int instno, int sampno)
{
    printf("pointer = %p length = %d bits = %d instrument %d sample %d\n", buff, lenbytes, numbits, instno, sampno);
}

#ifndef USEMEMLOAD

void* fileopen(const char* name)
{
    return fopen(name, "rb");
}

void fileclose(void* handle)
{
    fclose((FILE*)handle);
}

int fileread(void* buffer, int size, void* handle)
{
    return fread(buffer, 1, size, (FILE*)handle);
}

void fileseek(void* handle, int pos, int mode)
{
    fseek((FILE*)handle, pos, mode);
}

int filetell(void* handle)
{
    return ftell((FILE*)handle);
}

#else

struct MEMFILE
{
    int length;
    int pos;
    char* data;
};

void* memopen(const char* name)
{
    const auto memfile = static_cast<MEMFILE*>(calloc(sizeof(MEMFILE), 1));

#ifndef USEMEMLOADRESOURCE
    {
        // load an external file and read it
        if (FILE* fp = fopen(name, "rb"))
        {
            fseek(fp, 0, SEEK_END);
            memfile->length = ftell(fp);
            memfile->data = static_cast<char*>(calloc(memfile->length, 1));
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
    const auto memfile = static_cast<MEMFILE*>(handle);

#ifndef USEMEMLOADRESOURCE
    free(memfile->data); // don't free it if it was initialized with LockResource
#endif

    free(memfile);
}

int memread(void* buffer, int size, void* handle)
{
    const auto memfile = static_cast<MEMFILE*>(handle);

    if (memfile->pos + size >= memfile->length)
        size = memfile->length - memfile->pos;

    memcpy(buffer, memfile->data + memfile->pos, size);
    memfile->pos += size;

    return size;
}

void memseek(void* handle, int pos, int mode)
{
    const auto memfile = static_cast<MEMFILE*>(handle);

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
    const MEMFILE* memfile = static_cast<MEMFILE*>(handle);

    return memfile->pos;
}
#endif

/*
void songcallback(PlayerState *mod, unsigned char param)
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
int main(int argc, char* argv[])
{
    constexpr unsigned int mix_rate = 96000;
#ifndef USEMEMLOAD
    minifmod::file_access.open = fileopen;
    minifmod::file_access.close = fileclose;
    minifmod::file_access.read = fileread;
    minifmod::file_access.seek = fileseek;
    minifmod::file_access.tell = filetell;
#else
    minifmod::file_access.open = memopen;
    minifmod::file_access.close = memclose;
    minifmod::file_access.read = memread;
    minifmod::file_access.seek = memseek;
    minifmod::file_access.tell = memtell;
#endif

    if (argc < 2)
    {
        printf("-------------------------------------------------------------\n");
        printf("MINIFMOD example XM player.\n");
        printf("Pan/SpinningKids, 2022-2024.\n");
        printf("-------------------------------------------------------------\n");
        printf("Syntax: simplest infile.xm\n\n");
        return 0;
    }

    // ==========================================================================================
    // LOAD SONG
    // ==========================================================================================
    std::unique_ptr<Module> mod;
    if (void* fp = minifmod::file_access.open(argv[1]))
    {
        // create a mod instance
        mod = std::make_unique<Module>(minifmod::file_access, fp, nullptr);
        minifmod::file_access.close(fp);
    }

    if (!mod)
    {
        printf("Error loading song\n");
        return 0;
    }

    // ==========================================================================================
    // PLAY SONG
    // ==========================================================================================

    PlayerState player_state{std::make_unique<WindowsPlayback>(mix_rate), std::move(mod)};

    player_state.start();

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
            const auto [position, samples] = player_state.getTimeInfo();

            printf("ord %2d row %2d seconds %5.02f %s      \r", position.order, position.row,
                   static_cast<double>(samples) / mix_rate, (position.row % 8) ? "    " : "TICK");
        }
        while (key != 27);
    }

    printf("\n");

    mod = player_state.stop();
}
