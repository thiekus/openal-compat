
/**
 *
 * OpenAL compatibility shim for old Loki Games ports
 * Copyright (c) 2025 Thiekus
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>

#include "al_compat.h"
#include "alc_compat.h"
#include "oalcompat.h"

/* Real libopenal.so library handle */
static void* gpRealOpenAL = NULL;

/* Mutex when perform context initialization and destrcution */
static pthread_mutex_t gOalMutex;

/* Single OpenAL device created by alcOpenDevice */
static ALCdevice* gDevice = NULL;

/* How many context was created. If zero, gDevice will be free */
static int giDeviceRefCount = 0;

static LPALCCREATECONTEXT real_alcCreateContext = NULL;
static LPALCMAKECONTEXTCURRENT real_alcMakeContextCurrent = NULL;
static LPALCPROCESSCONTEXT real_alcProcessContext = NULL;
static LPALCDESTROYCONTEXT real_alcDestroyContext = NULL;
static LPALCGETCURRENTCONTEXT real_alcGetCurrentContext = NULL;
static LPALCOPENDEVICE real_alcOpenDevice = NULL;
static LPALCCLOSEDEVICE real_alcCloseDevice = NULL;
static LPALCGETERROR real_alcGetError = NULL;
static LPALCISEXTENSIONPRESENT real_alcIsExtensionPresent = NULL;
static LPALCGETSTRING real_alcGetString = NULL;

static LPALENABLE real_alEnable = NULL;
static LPALDISABLE real_alDisable = NULL;
static LPALISENABLED real_alIsEnabled = NULL;
static LPALGETSTRING real_alGetString = NULL;
static LPALGETBOOLEANV real_alGetBooleanv = NULL;
static LPALGETINTEGERV real_alGetIntegerv = NULL;
static LPALGETFLOATV real_alGetFloatv = NULL;
static LPALGETDOUBLEV real_alGetDoublev = NULL;
static LPALGETBOOLEAN real_alGetBoolean = NULL;
static LPALGETINTEGER real_alGetInteger = NULL;
static LPALGETFLOAT real_alGetFloat = NULL;
static LPALGETDOUBLE real_alGetDouble = NULL;
static LPALGETERROR real_alGetError = NULL;
static LPALISEXTENSIONPRESENT real_alIsExtensionPresent = NULL;
static LPALGETPROCADDRESS real_alGetProcAddress = NULL;
static LPALGETENUMVALUE real_alGetEnumValue = NULL;
static LPALLISTENERF real_alListenerf = NULL;
static LPALLISTENER3F real_alListener3f = NULL;
static LPALLISTENERFV real_alListenerfv = NULL;
static LPALGETLISTENERF real_alGetListenerf = NULL;
static LPALGETLISTENERFV real_alGetListenerfv = NULL;
static LPALGETLISTENERI real_alGetListeneri = NULL;
static LPALGENSOURCES real_alGenSources = NULL;
static LPALDELETESOURCES real_alDeleteSources = NULL;
static LPALISSOURCE real_alIsSource = NULL;
static LPALSOURCEF real_alSourcef = NULL;
static LPALSOURCE3F real_alSource3f = NULL;
static LPALSOURCEFV real_alSourcefv = NULL;
static LPALSOURCEI real_alSourcei = NULL;
static LPALGETSOURCEF real_alGetSourcef = NULL;
static LPALGETSOURCEFV real_alGetSourcefv = NULL;
static LPALGETSOURCEI real_alGetSourcei = NULL;
static LPALSOURCEPLAYV real_alSourcePlayv = NULL;
static LPALSOURCESTOPV real_alSourceStopv = NULL;
static LPALSOURCEPLAY real_alSourcePlay = NULL;
static LPALSOURCESTOP real_alSourceStop = NULL;
static LPALSOURCEPAUSE real_alSourcePause = NULL;
static LPALGENBUFFERS real_alGenBuffers = NULL;
static LPALDELETEBUFFERS real_alDeleteBuffers = NULL;
static LPALISBUFFER real_alIsBuffer = NULL;
static LPALBUFFERDATA real_alBufferData = NULL;
/* static LPALBUFFERI real_alBufferi = NULL; */
static LPALGETBUFFERF real_alGetBufferf = NULL;
static LPALGETBUFFERI real_alGetBufferi = NULL;

#ifndef OAL_COMPAT_NODEBUG
static void _enumAvaliableDevice() {
    const ALCchar* devices;
    const ALCchar* defaultDevice;
    ALCint devNum = 0;
    const ALCchar* deviceNameList[0x7f];
    const ALCchar* seekName = NULL;
    int i;
    if (real_alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_TRUE) {
        defaultDevice = real_alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
        devices = real_alcGetString(NULL, ALC_DEVICE_SPECIFIER);
        OAL_LOG("OpenAL enumeration extension present, default device: %s\n", defaultDevice);
        memset(deviceNameList, 0, sizeof(deviceNameList));
        seekName = devices;
        if (seekName[0] != '\0') {
            deviceNameList[0] = seekName;
            devNum++;
            while ((*seekName != '\0') && (*(seekName+1) != '\0')) {
                if (*seekName == '\0') {
                    deviceNameList[devNum+1] = seekName+1;
                    devNum++;
                }
                seekName++;
            }
        }
        OAL_LOG("Enumerate %d OpenAL device\n", devNum);
        for (i = 0; i < devNum; i++) {
            OAL_LOG( "* OpenAL device %d: %s\n", i, deviceNameList[i]);
        }
    } else {
        OAL_LOG("OpenAL enumeration extension not present\n");
    }
}
#endif

static void* _resolveSymbol(void* module, char* sym) {
    void* result = dlsym(module, sym);
    if (result)
        OAL_LOG("Loading OpenAL symbol %s at 0x%.8x\n", sym, (size_t)result);
    else
        OAL_LOG("ERROR: Loading OpenAL symbol %s: %s\n", sym, dlerror());
    return result;
}

#define OAL_MUST_RESOLVE_SYMBOL(v,s) \
    (v) = _resolveSymbol(pRealOpenAL, #s); \
    if (!v) { \
        bHasSymbolFailToLoad = 1; \
        goto skip_resolve; \
    }

__attribute__((constructor))
void initOpenALCompat(int argc, char** argv, char** envp) {
    void* pRealOpenAL;
    int bHasSymbolFailToLoad = 0;
    char defaultLibName[] = "libopenal.so.0.0.0";
    char* openalLibName = getenv("OAL_COMPAT_LIBRARY");

    /* Stick with libopenal.so */
    if ((openalLibName == NULL) || (strcmp(openalLibName, "") == 0))
        openalLibName = defaultLibName;

    OAL_LOG("OpenAL Compatibility Shim by Thiekus 2025\n");
    OAL_LOG("Opening OpenAL library: %s\n", openalLibName);
    pRealOpenAL = dlopen(openalLibName, RTLD_LAZY | RTLD_GLOBAL);
    if (!pRealOpenAL) {
        OAL_LOG("ERROR: Opening OpenAL library: %s\n", dlerror());
        return;
    }
    OAL_LOG("OpenAL library loaded at 0x%.8x\n", (size_t)pRealOpenAL);

    /* Load alc* functions that implemented in old OpenAL */
    OAL_MUST_RESOLVE_SYMBOL(real_alcCreateContext, alcCreateContext)
    OAL_MUST_RESOLVE_SYMBOL(real_alcMakeContextCurrent, alcMakeContextCurrent)
    OAL_MUST_RESOLVE_SYMBOL(real_alcProcessContext, alcProcessContext)
    OAL_MUST_RESOLVE_SYMBOL(real_alcDestroyContext, alcDestroyContext)
    OAL_MUST_RESOLVE_SYMBOL(real_alcGetCurrentContext, alcGetCurrentContext)
    OAL_MUST_RESOLVE_SYMBOL(real_alcOpenDevice, alcOpenDevice)
    OAL_MUST_RESOLVE_SYMBOL(real_alcCloseDevice, alcCloseDevice)
    OAL_MUST_RESOLVE_SYMBOL(real_alcGetError, alcGetError)
    OAL_MUST_RESOLVE_SYMBOL(real_alcIsExtensionPresent, alcIsExtensionPresent)
    OAL_MUST_RESOLVE_SYMBOL(real_alcGetString, alcGetString)

    /* Load remaining al* that implemented */
    OAL_MUST_RESOLVE_SYMBOL(real_alEnable, alEnable)
    OAL_MUST_RESOLVE_SYMBOL(real_alDisable, alDisable)
    OAL_MUST_RESOLVE_SYMBOL(real_alIsEnabled, alIsEnabled)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetString, alGetString)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetBooleanv, alGetBooleanv)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetIntegerv, alGetIntegerv)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetFloatv, alGetFloatv)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetDoublev, alGetDoublev)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetBoolean, alGetBoolean)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetInteger, alGetInteger)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetFloat, alGetFloat)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetDouble, alGetDouble)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetError, alGetError)
    OAL_MUST_RESOLVE_SYMBOL(real_alIsExtensionPresent, alIsExtensionPresent)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetProcAddress, alGetProcAddress)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetEnumValue, alGetEnumValue)
    OAL_MUST_RESOLVE_SYMBOL(real_alListenerf, alListenerf)
    OAL_MUST_RESOLVE_SYMBOL(real_alListener3f, alListener3f)
    OAL_MUST_RESOLVE_SYMBOL(real_alListenerfv, alListenerfv)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetListenerf, alGetListenerf)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetListenerfv, alGetListenerfv)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetListeneri, alGetListeneri)
    OAL_MUST_RESOLVE_SYMBOL(real_alGenSources, alGenSources)
    OAL_MUST_RESOLVE_SYMBOL(real_alDeleteSources, alDeleteSources)
    OAL_MUST_RESOLVE_SYMBOL(real_alIsSource, alIsSource)
    OAL_MUST_RESOLVE_SYMBOL(real_alSourcef, alSourcef)
    OAL_MUST_RESOLVE_SYMBOL(real_alSource3f, alSource3f)
    OAL_MUST_RESOLVE_SYMBOL(real_alSourcefv, alSourcefv)
    OAL_MUST_RESOLVE_SYMBOL(real_alSourcei, alSourcei)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetSourcef, alGetSourcef)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetSourcefv, alGetSourcefv)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetSourcei, alGetSourcei)
    OAL_MUST_RESOLVE_SYMBOL(real_alSourcePlayv, alSourcePlayv)
    OAL_MUST_RESOLVE_SYMBOL(real_alSourceStopv, alSourceStopv)
    OAL_MUST_RESOLVE_SYMBOL(real_alSourcePlay, alSourcePlay)
    OAL_MUST_RESOLVE_SYMBOL(real_alSourceStop, alSourceStop)
    OAL_MUST_RESOLVE_SYMBOL(real_alSourcePause, alSourcePause)
    OAL_MUST_RESOLVE_SYMBOL(real_alGenBuffers, alGenBuffers)
    OAL_MUST_RESOLVE_SYMBOL(real_alDeleteBuffers, alDeleteBuffers)
    OAL_MUST_RESOLVE_SYMBOL(real_alIsBuffer, alIsBuffer)
    OAL_MUST_RESOLVE_SYMBOL(real_alBufferData, alBufferData)
    /* OAL_MUST_RESOLVE_SYMBOL(real_alBufferi, alBufferi) */
    OAL_MUST_RESOLVE_SYMBOL(real_alGetBufferf, alGetBufferf)
    OAL_MUST_RESOLVE_SYMBOL(real_alGetBufferi, alGetBufferi)

skip_resolve:
    /* Has failed to load symbol previously? */
    if (bHasSymbolFailToLoad) {
        dlclose(pRealOpenAL);;
        return;
    }
    gpRealOpenAL = pRealOpenAL;

    #ifndef OAL_COMPAT_NODEBUG
    _enumAvaliableDevice();
    #endif
}

__attribute__((destructor))
void finalizeOpenALCompat() {
    if (giDeviceRefCount > 0) {
        OAL_LOG("OpenAL context might be leaked, refCount=%d\n", giDeviceRefCount);
        /* Force zero to close next */
        giDeviceRefCount = 0;
    }
    if (gDevice) {
        OAL_LOG("OpenAL device 0x%.8x not released before\n", (size_t)gDevice);
    }
    _oalCompatDeviceCleanupCheck();
    if (gpRealOpenAL) {
        OAL_LOG("Closing real OpenAL library at 0x%.8x\n", (size_t)gpRealOpenAL);
        dlclose(gpRealOpenAL);
        gpRealOpenAL = NULL;
    }
}

/* Mutex for this shim */

void _oalCompatMutexLock() {
    pthread_mutex_lock(&gOalMutex);
}

void _oalCompatMutexUnlock() {
    pthread_mutex_unlock(&gOalMutex);
}

void _oalCompatDeviceCleanupCheck() {
    if (giDeviceRefCount <= 0) {
        _oalCompatMutexLock();
        if (gDevice) {
            OAL_LOG("alcCloseDevice=0x%.8x\n", (size_t)gDevice);
            real_alcCloseDevice(gDevice);
            gDevice = NULL;
        }
        _oalCompatMutexUnlock();
    }
}

/* alc* functions */

ALC_API ALCcontext* ALC_APIENTRY alcCreateContext(const ALCint *attrlist) {
    ALCdevice* pDev;
    ALCcontext* result;
    ALCint newAttr[0x7f];
    ALCint* attrParam = NULL;
    ALCint attrCount, newAttrCount;
    ALCint attrCurrent, attrVal;
    char* openalDevice = NULL;
    _oalCompatMutexLock();
    if (!gDevice) {
        openalDevice = getenv("OAL_COMPAT_DEVICE");
        if (openalDevice) {
            if (strcmp(openalDevice, "") == 0)
                openalDevice = NULL;
            else
                OAL_LOG("Override OAL_COMPAT_DEVICE=%s\n", openalDevice);
        }
        pDev = real_alcOpenDevice(NULL);
        OAL_LOG("alcOpenDevice=0x%.8x\n", (size_t)pDev);
        if (pDev) {
            gDevice = pDev;
        } else {
            _oalCompatMutexUnlock();
            return NULL;
        }
    }
    _oalCompatMutexUnlock();
    /* Rebuild attrList if needed, since old attrList not compatible with later OpenAL */
    if (attrlist) {
        attrCount = 0;
        newAttrCount = 0;
        memset(newAttr, ALC_INVALID, sizeof(newAttr));
        attrCurrent = attrlist[0];
        while ((attrCurrent != ALC_INVALID) && (newAttrCount < sizeof(newAttr)-1)) {
            attrVal = attrlist[attrCount+1];
            switch (attrCurrent) {
            case ALC_OLD_FREQUENCY:
                OAL_LOG("alcCreateContext: attr ALC_FREQUENCY=%dHz\n", attrVal);
                newAttr[newAttrCount] = ALC_FREQUENCY;
                newAttr[newAttrCount+1] = attrVal;
                newAttrCount += 2;
                break;
            case ALC_OLD_SYNC:
                OAL_LOG("alcCreateContext: attr ALC_SYNC=%d\n", attrVal);
                newAttr[newAttrCount] = ALC_SYNC;
                newAttr[newAttrCount+1] = attrVal;
                newAttrCount += 2;
                break;
            case ALC_OLD_BUFFERSIZE:
                OAL_LOG("alcCreateContext: unimplemented attr ALC_BUFFERSIZE=0x%.8x\n", attrVal);
                break;
            case ALC_OLD_SOURCES:
                OAL_LOG("alcCreateContext: unimplemented attr ALC_SOURCES=%d\n", attrVal);
                break;
            case ALC_OLD_BUFFERS:
                OAL_LOG("alcCreateContext: unimplemented attr ALC_BUFFERS=%d\n", attrVal);
                break;
            default:
                OAL_LOG("alcCreateContext: Unknown attr=0x%.8x 0x%.8x\n", attrCurrent, attrVal);
            }
            attrCount += 2;
            attrCurrent = attrlist[attrCount];
        }
        /* Ensure attrList terminated */
        newAttr[newAttrCount] = ALC_INVALID;
        attrParam = newAttr;
    }
    result = real_alcCreateContext(gDevice, attrParam);
    OAL_LOG("alcCreateContext=0x%.8x\n", (size_t)result);
    if (result) {
        _oalCompatMutexLock();
        giDeviceRefCount++;
        /* In older OpenAL, context was selected right after context creation. But must be explicitly select on later OpenAL */
        if (giDeviceRefCount == 1) {
            alcMakeContextCurrent(result);
        }
        _oalCompatMutexUnlock();
    } else {
        _oalCompatDeviceCleanupCheck();
    }
    return result;
}

ALC_API ALCboolean ALC_APIENTRY alcMakeContextCurrent(ALCcontext *context) {
    ALCboolean result = real_alcMakeContextCurrent(context);
    OAL_LOG("alcMakeContextCurrent set to 0x%.8x=%d\n", (size_t)context, result);
    return result;
}

/* This was missing on later OpenAL specification, but seems return that context if succeeded */
ALC_API ALCcontext* ALC_APIENTRY alcUpdateContext(ALCcontext *context) {
    real_alcProcessContext(context);
    return context;
}

ALC_API void ALC_APIENTRY alcDestroyContext(ALCcontext *context) {
    real_alcDestroyContext(context);
    _oalCompatMutexLock();
    if (giDeviceRefCount > 0)
        giDeviceRefCount--;
    OAL_LOG("alcDestroyContext=0x%.8x refCount=%d\n", (size_t)context, giDeviceRefCount);
    _oalCompatMutexUnlock();
    _oalCompatDeviceCleanupCheck();
    return;
}

ALC_API ALCcontext* ALC_APIENTRY alcGetCurrentContext() {
    return real_alcGetCurrentContext();
}

ALC_API ALCenum ALC_APIENTRY alcGetError() {
    if (!gDevice) {
        return ALC_INVALID_DEVICE;
    }
    return real_alcGetError(gDevice);
}

/* Rest of al* functions */

AL_API void AL_APIENTRY alEnable(ALenum capability) {
    real_alEnable(capability);
}

AL_API void AL_APIENTRY alDisable(ALenum capability) {
    real_alDisable(capability);
}

AL_API ALboolean AL_APIENTRY alIsEnabled(ALenum capability) {
    return real_alIsEnabled(capability);
}

AL_API const ALchar* AL_APIENTRY alGetString(ALenum param) {
    return real_alGetString(param);
}

AL_API void AL_APIENTRY alGetBooleanv(ALenum param, ALboolean* data) {
    real_alGetBooleanv(param, data);
}

AL_API void AL_APIENTRY alGetIntegerv(ALenum param, ALint* data) {
    real_alGetIntegerv(param, data);
}

AL_API void AL_APIENTRY alGetFloatv(ALenum param, ALfloat* data) {
    real_alGetFloatv(param, data);
}

AL_API void AL_APIENTRY alGetDoublev(ALenum param, ALdouble* data) {
    real_alGetDoublev(param, data);
}

AL_API ALboolean AL_APIENTRY alGetBoolean(ALenum param) {
    return real_alGetBoolean(param);
}

AL_API ALint AL_APIENTRY alGetInteger(ALenum param) {
    return real_alGetInteger(param);
}

AL_API ALfloat AL_APIENTRY alGetFloat(ALenum param) {
    return real_alGetFloat(param);
}

AL_API ALdouble AL_APIENTRY alGetDouble(ALenum param) {
    return real_alGetDouble(param);
}

AL_API ALenum AL_APIENTRY alGetError() {
    return real_alGetError();
}

AL_API ALboolean AL_APIENTRY alIsExtensionPresent(const ALchar* extname) {
    ALboolean result = real_alIsExtensionPresent(extname);
    OAL_LOG("alIsExtensionPresent %s=%d\n", extname, result);
    return result;
}

AL_API void* AL_APIENTRY alGetProcAddress(const ALchar* fname) {
    int bForced = 0;
    void* result = real_alGetProcAddress(fname);
    OAL_LOG("alGetProcAddress %s=0x%.8x\n", fname, (size_t)result);
    if (result == NULL) {
        if (strcmp("alAttenuationScale_LOKI", fname) == 0) {
            result = (void*)&alAttenuationScale_LOKI;
            bForced = 1;
            goto skip_check;
        }
        #if 0
        if (strcmp("alBufferi_LOKI", fname) == 0) {
            result = (void*)&alBufferi_LOKI;
            bForced = 1;
            goto skip_check;
        }
        if (strcmp("alBufferDataWithCallback_LOKI", fname) == 0) {
            result = (void*)alBufferDataWithCallback_LOKI;
            bForced = 1;
            goto skip_check;
        }
        #endif
        skip_check:
        if (bForced)
            OAL_LOG("Force alGetProcAddress %s=0x%.8x\n", fname, (size_t)result);
    }
    return result;
}

AL_API ALenum AL_APIENTRY alGetEnumValue(const ALchar* ename) {
    return real_alGetEnumValue(ename);
}

AL_API void AL_APIENTRY alListenerf(ALenum param, ALfloat value) {
    real_alListenerf(param, value);
}

AL_API void AL_APIENTRY alListener3f(ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) {
    real_alListener3f(param, value1, value2, value3);
}

AL_API void AL_APIENTRY alListenerfv(ALenum param, const ALfloat* values) {
    real_alListenerfv(param, values);
}

AL_API void AL_APIENTRY alGetListenerf(ALenum param, ALfloat* value) {
    real_alGetListenerf(param, value);
}

AL_API void AL_APIENTRY alGetListenerfv(ALenum param, ALfloat* values) {
    real_alGetListenerfv(param, values);
}
AL_API void AL_APIENTRY alGetListeneri(ALenum param, ALint* value) {
    real_alGetListeneri(param, value);
}

AL_API void AL_APIENTRY alGenSources(ALsizei n, ALuint* sources) {
    real_alGenSources(n, sources);
}

AL_API void AL_APIENTRY alDeleteSources(ALsizei n, const ALuint* sources) {
    real_alDeleteSources(n, sources);
}

AL_API ALboolean AL_APIENTRY alIsSource(ALuint sid) {
    return real_alIsSource(sid);
}

AL_API void AL_APIENTRY alSourcef(ALuint sid, ALenum param, ALfloat value) {
    real_alSourcef(sid, param, value);
}

AL_API void AL_APIENTRY alSource3f(ALuint sid, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) {
    real_alSource3f(sid, param, value1, value2, value3);
}

AL_API void AL_APIENTRY alSourcefv(ALuint sid, ALenum param, const ALfloat* values) {
    real_alSourcefv(sid, param, values);
}

AL_API void AL_APIENTRY alSourcei(ALuint sid, ALenum param, ALint value) {
    real_alSourcei(sid, param, value);
}

AL_API void AL_APIENTRY alGetSourcef(ALuint sid, ALenum param, ALfloat* value) {
    real_alGetSourcef(sid, param, value);
}

AL_API void AL_APIENTRY alGetSourcefv(ALuint sid, ALenum param, ALfloat* values) {
    real_alGetSourcefv(sid, param, values);
}

AL_API void AL_APIENTRY alGetSourcei(ALuint sid, ALenum param, ALint* value) {
    real_alGetSourcei(sid, param, value);
}

AL_API void AL_APIENTRY alSourcePlayv(ALsizei ns, const ALuint *sids) {
    real_alSourcePlayv(ns, sids);
}

AL_API void AL_APIENTRY alSourceStopv(ALsizei ns, const ALuint *sids) {
    real_alSourceStopv(ns, sids);
}

AL_API void AL_APIENTRY alSourcePlay(ALuint sid) {
    real_alSourcePlay(sid);
}

AL_API void AL_APIENTRY alSourceStop(ALuint sid) {
    real_alSourceStop(sid);
}

AL_API void AL_APIENTRY alSourcePause(ALuint sid) {
    real_alSourcePause(sid);
}

AL_API void AL_APIENTRY alGenBuffers(ALsizei n, ALuint* buffers) {
    real_alGenBuffers(n, buffers);
}

AL_API void AL_APIENTRY alDeleteBuffers(ALsizei n, const ALuint* buffers) {
    real_alDeleteBuffers(n, buffers);
}

AL_API ALboolean AL_APIENTRY alIsBuffer(ALuint bid) {
    return real_alIsBuffer(bid);
}

AL_API void AL_APIENTRY alBufferData(ALuint bid, ALenum format, const ALvoid* data, ALsizei size, ALsizei freq) {
    real_alBufferData(bid, format, data, size, freq);
}

AL_API void AL_APIENTRY alGetBufferf(ALuint bid, ALenum param, ALfloat* value) {
    real_alGetBufferf(bid, param, value);
}

AL_API void AL_APIENTRY alGetBufferi(ALuint bid, ALenum param, ALint* value) {
    real_alGetBufferi(bid, param, value);
}

/* LOKI ext */

/* Only stub, nuked before 2001 */
AL_API void AL_APIENTRY alAttenuationScale_LOKI(ALfloat param) {
    OAL_LOG("alAttenuationScale_LOKI=%f (STUB)\n", param);
}

#if 0
AL_API void AL_APIENTRY alBufferi_LOKI(ALuint buffer, ALenum param, ALint value) {
    // real_alBufferi(buffer, param, value);
}

/* Unimplemented. This make SC3K no music at gameplay and intro video, but will play fine elsewhere */
AL_API void AL_APIENTRY alBufferDataWithCallback_LOKI(ALuint bid, int (*Callback)(ALuint, ALuint, ALshort *, ALenum, ALint, ALint)) {
    OAL_LOG("alBufferDataWithCallback_LOKI bid=%0.x8 cb=%0.x8\n", bid, Callback);
    return;
}
#endif
