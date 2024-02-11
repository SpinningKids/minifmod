/******************************************************************************/
/* SYSTEM_FILE.H                                                              */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#ifndef SYSTEM_FILE_H_
#define SYSTEM_FILE_H_

namespace minifmod {
    using FFileOpen = void* (const char* name);
    using FFileClose = void(void* handle);
    using FFileRead = int(void* buffer, int size, void* handle);
    using FFileSeek = void(void* handle, int pos, int mode);
    using FFileTell = int(void* handle);

    struct FileAccess
    {
        FFileOpen* open;
        FFileClose* close;
        FFileRead* read;
        FFileSeek* seek;
        FFileTell* tell;
    };

    inline FileAccess file_access;
}

#endif
