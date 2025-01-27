#ifndef OALCOMPAT_H
#define OALCOMPAT_H

#include <stdio.h>

#include "alc_compat.h"
#include "al_compat.h"

#ifndef OAL_COMPAT_NODEBUG
    #define OAL_LOG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
    /* Built with make CFLAGS=-DOAL_COMPAT_NODEBUG=1 */
    #define OAL_LOG(fmt, ...) (void)(fmt)
#endif

/* Approximate value based on http://www.fifi.org/cgi-bin/info2www?(openal)alc */
#define ALC_OLD_FREQUENCY  0x00000001
#define ALC_OLD_SYNC       0x00000002
#define ALC_OLD_BUFFERSIZE 0x00000003
#define ALC_OLD_SOURCES    0x00000004
#define ALC_OLD_BUFFERS    0x00000005

void _oalCompatMutexLock();
void _oalCompatMutexUnlock();
void _oalCompatDeviceCleanupCheck();

/* Reference to some OpenAL extensions */
AL_API void AL_APIENTRY alAttenuationScale_LOKI(ALfloat param);
AL_API void AL_APIENTRY alBufferi_LOKI(ALuint buffer, ALenum param, ALint value);
AL_API void AL_APIENTRY alBufferDataWithCallback_LOKI(ALuint bid,
    int (*Callback)(ALuint, ALuint, ALshort *, ALenum, ALint, ALint));

#endif
