/******************************************************************************/
/* MIXER_FPU_RAMP.H                                                           */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#ifndef MIXER_FPU_RAMP_H
#define MIXER_FPU_RAMP_H

#define FSOUND_VOLUMERAMP_STEPS	128		// at 44.1khz

void FSOUND_Mixer_FPU_Ramp(float *mixptr, int len);

inline unsigned int mix_volumerampsteps;

#endif
