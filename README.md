# minifmod

MiniFMOD 2.2.0 C++ version

Copyright Firelight Technologies, 1999-2003.

Heavy refactoring/cleanup/rewrite and CMake adaption by Pan/SpinningKids 2022-2025

## DOCUMENTATION 

### Disclaimer and legal (from original author)
- *MiniFMOD is not FMOD*.  The code is a complete hack, and does 
  by no way represent the source contained within the main FMOD library.  
  I stripped and copied code around, took out functions and
  inlined them (ie i put waveout code in the music file!), and generally 
  turned it upside down to try and squeeze some size!
- MiniFMOD may not replay XM files 100% as FMOD does, due to some cutting of 
  corners.  It seems to be ok so far.
- This source is provided as-is.  Firelight Technologies will not support or answer 
  questions about the source provided.  (unless there are some REALLY nasty bugs.. 
  remember im NOT going to spend time on this! fix it yourself!)
- MiniFMOD Sourcecode is copyright (c) 2000, Firelight Technologies.
- This source must not be redistributed without this text. 
- The source can be modified, and redistributed as long as no copyright or comment
  blocks (see at the top of each source file) are removed.

AGAIN : DONT NAG ME WITH QUESTIONS ABOUT THE SOURCE, ASK SOMEONE ELSE. 
        (i recommend #coders on ircnet.)

### Additional notes (from Pan/SpinningKids)

  The original disclaimer and legal from the original author is still valid.
  Being this is a community, if you find a bug/problem or add a feature, we
  would love to hear from you, and it would be great if you can contribute it
  back to the community.

  All this work is completely diverged from FMOD in early 2000s, and even more since
  2022, and are now maintained from a different person, who do does not belong nor 
  have contacts with the MiniFMOD and/or FMOD original author(s).
  This should be considered a derivative work, heavily refactored, and
  developed independently under completely different premises - please don't
  consider this code as representative in any way of FMOD or the original MiniFMOD
  features or quality.

### Usage Instructions

- See `apps` subdirectory for an example of how to use minifmod and minixm.
- You MUST set file IO callbacks for MiniFMOD to use. This is a very flexible way to
  work, and saves having multiple file routines for file, memory or resource loading!
  Just rip the sample if you dont know what it is about.
- There are no error codes.  All functions return true or false, and it should be obvious why they fail.
- Compile your exe with SIZE optimizations ON. (like the test example)
- Compress your exe with UPX or other packer. You can find UPX at https://upx.github.io/

#### minifmod library

- Pass a callback function to FMUSIC_LoadSong or NULL.  This will allow you to get a
  callback when each sample is loaded in a song.  Here you can then fill in the data for
  the sample yourself.  Note if you set callbacks, the samples are not loaded from file,
  and will be just silence if not filled in.
- If rewriting C standard libraries you need to supply some functions for fmod music playback routine.
  FMUSIC_XMLINEARPERIOD2HZ uses pow and not a lookup table because it would bloat the size.

#### minixm library

- This library has a C++ interface, and it has a slightly more efficient interface (size-wise).
- If rewriting C standard libraries you need to supply some functions for fmod music playback routine.
  For example, `XMLinearPeriod2Frequency` uses `exp2f` and not a lookup table because it would bloat the size.

#### xmformat library

- This is a header-only library containing all the structures from xm headers.
- This is a mix between the MiniFMOD headers, and the original documentation xm.txt written by Mr.H/Triton
  (and available with FT2)

#### Making your exe even smaller with XMEFFECTS.H and FEXP.EXE!

- Note all the effects in the xm replay code are wrapped with #ifdef's.  They are defined in 
  xmeffects.h.
- Have a look in the `/tools` directory: There is an executable called `FEXP.EXE`
- Use `FEXP.EXE` on an xm file and it will generate `xmeffects.h` for you!
- Recompile the library with the new `xmeffects.h`
- See your exe size go down AGAIN!
- (only play the song you fexp'ed or it will screw up on other songs probably :) ..)

Update since v2.0.0:

- FExp is no longer distrubuted in bundle, for security reasons.
- FExp source was not (and is not) available.
- The format of xmeffects.h isn't changed, but it's part of the minixm library.

## Revision History

### Version 2.2.0 (2025/12/15 Pan/SK)

- Support for both MingW and MSVC on Windows: minor breaking change on file support
  (callback definition changed)

### Version 2.1.0 (2025/08/24 Pan/SK)

- Linux support added!
    - I'm not happy with it, the interface is clunky, but we have something.

- Removed unnecessary modern C++ (std::function), removed warnings, code cleanup

- Update to min cmake 3.10 (required for compatibility with cmake 4+)

### Version 2.0.0 (2024/07/16 Pan/SK)

- Conversion to C++
    - Adoption of selected parts of the standard library (std::thread, std::min, std::max, std::clamp)
    - General abuse of max/min/clamp, in particular for volume fadeout.
    - Careful adoption of modern C++ features
    - Improved type safety

- Several size optimizations (some performance downgrades, but hey, it's 2022+)
    - More floating point processing (volume, pan, mixing, ramps, ...)
    - Simplified load function (to be improved - target is to avoid load entirely, and just reassign pointers into memory-mapped XM file)
    - Changed volume ramps to filtered volume processing (simpler&smaller code, a bit heavier on the FPU)
    - Split vibrato and tremolo structures, created structures, and centralized handling
    - Centralized envelope control, volume/pan clamping, vibrato and tremolo.
- Features
    - More precise volume processing
    - Reintroduces FSOUND_Init (to be able to change the mixing frequency)
    - #ifdef'ed out windows-specific lines (TODO: finalize linux version)
- Bugfixes:
    - volume slide won't trigger volume update
    - sampleoffset was never transmitted from FMUSIC_Channel to FSOUND_Channel (probably a transient bug)
    - jumping out of the loop with sampleoffset will result in negative samples_to_mix (probably a transient bug)
    - E5x changed finetune permanently rather than per-note

### Version 1.8.0 (2022/01/15 Pan/SK)

- After just 19 years... the CMake version!
- Asm code removed, x64 compilation enabled!
- Not really optimizing for size anymore in the default settings, but you can recreate your
  custom project files from CMake and customize them.

### Version 1.7 (5/12/03)

- Whats this! an update? 
- FSOUND_Init and FSOUND_Close removed, who needs them!
- FMUSIC_StopSong fixed so it doesnt just sit there and repeat.  Now you can call play and stop
  to your hearts content.
- playback bug fixed in pattern loop command.
- tremolo fix.
- memory leak fixes.
- crash fixes calling play multiple times in a row.

### Version 1.6

- Minifmod now is 100% click free.  even though changes in volumes were ramped, if one sound
  cut another off suddenly you could hear a click.  To fix this it now uses twice the amount of 
  channels and flips between channels that arent playing so that this situation never occurs again.
- Change in fsound.c that removes any last possibilities of the sound breaking up.

### Version 1.5

- Default xmeffects.h was supposed to have ALL effects enabled, it only had about HALF! fixed. 
- New more accurate FEXP.EXE
- Previous version didnt load finetune value.. Finetunes now work.
- mixer bug fixed.. volume ramps werent working properly on some notes.

### Version 1.4

* Source code release!
+ Added FEXP, a tool to generate XMEFFECTS.H, and disregard code that isnt needed for your song!
* Now possible to only have 5k footprint!!!

### Version 1.3

+ New prebuffered output! - this means NO latency and NO 
  jittering/skipping sound! (due to small buffersizes)
+ GetRow/GetOrder/GetTime are now latency free.. you can sync EXACTLY 
  to when you hear the sound.
- FSOUND_SetBufferSize removed.. there is no need for this now!
- Fixed click at end of non looping samples
- Fixed bug in fine portamento
- Fixed normal and instrument vibrato.
- Fixed panning/volume bug
* Note most xm of these replay bugs were introduced after size optimizations :)

### Version 1.2

- Now 7.5k footprint!!
- Fixed naughty XM replay bug - accidently left some test code in instrument vibrato!
- Load from windows resource example added in main.cpp
- I doubt it will get any smaller.  If i totally rewrote the XM player i could halve the
  size maybe, but i dont have the time or courage to do that :).

### Version 1.1

- New volume ramping mixer
- 8k footprint instead of 8.5k
- MSVC 5.0 support added
- small harmonic noise removed from mixing routine = clearer sound.
- Added sample load callback, when the XM loader loads samples, it calls your callback and 
  you can fill in the data if you like (you might have a better compression algorithm)
- XM porta between instrument samples fixed.
- removed dependancy on stdlib func strncmp so you dont have to write your own if you are replacing C stdlib.

### Version 1.0

- Only 8.5k footprint in exe after being compressed with UPX.  (maybe less if you dont call info functions)
- Statically linkable .lib.
- XM only
- Windows Multimedia Wave Out only.  (140ms latency)
- FPU Interpolating, volume ramping mixer only.
- Only the basic API functions needed to play and synchronize a mod.
- Tremolo has been disabled
- MSVC 6 only 
- No file routines, you have to supply your own using FSOUND_SetFile
 
## Final Notes

Firelight Technologies is a registered Australian company.

Pan/SpinningKids is not affiliated with Firelight Technology, but requested and received written
authorization to modify and redistribute this code on GitHUB for historic preservation and for
use in new demos. This library is not intended to be a MiniFMOD replacement, if you're working on
a commercial project, consider using FMOD.
