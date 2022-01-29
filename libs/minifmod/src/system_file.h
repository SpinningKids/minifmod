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

inline void*    (*FSOUND_File_Open)(const char *name);
inline void	    (*FSOUND_File_Close)(void* handle);
inline int	    (*FSOUND_File_Read)(void *buffer, int size, void* handle);
inline void	    (*FSOUND_File_Seek)(void* handle, int pos, int mode);
inline int	    (*FSOUND_File_Tell)(void* handle);

#endif
