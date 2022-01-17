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

#ifndef _SYSTEM_FILE_H_
#define _SYSTEM_FILE_H_

extern void*		(*FSOUND_File_Open)(char *name);
extern void			(*FSOUND_File_Close)(void* handle);
extern int			(*FSOUND_File_Read)(void *buffer, int size, void* handle);
extern void			(*FSOUND_File_Seek)(void* handle, int pos, int mode);
extern int			(*FSOUND_File_Tell)(void* handle);

#endif
