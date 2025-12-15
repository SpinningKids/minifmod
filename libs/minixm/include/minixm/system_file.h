/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* This library (minixm) is maintained by Pan/SpinningKids, 2022-2024         */
/******************************************************************************/

#pragma once

namespace minifmod
{
    using FFileOpen = void*(const char* name);
    using FFileClose = void(void* handle);
    using FFileRead = size_t(void* buffer, size_t count, void* handle);
    using FFileSeek = void(void* handle, long pos, int mode);
    using FFileTell = long(void* handle);

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
