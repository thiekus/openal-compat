# OpenAL Compatibility Shim

This OpenAL Compatibility Shim is intended for old Loki games that uses very old version of OpenAL, which simply segfault crash if using later version of OpenAL due to ABI breaking changes since year 2000. Also the library that ABI compatible, `libopenal-0.0.so`, is only supported old OSS and ESD backend which obviously don't work in modern Linux distros since `/dev/dsp` was removed for long time ago. Finally, this library is intended to be paired with [this version of OpenAL](https://github.com/thiekus/openal-loki) since later OpenAL and OpenAL Soft fork is removed Loki extension `*_LOKI()` functions since 2006, attempt to run game that relies of that functions like Loki port of SimCity 3000 Unlimted will crash.

## OpenAL ABI Breakdown and Problems

This compatibility shim is intended to solve these following problems:
* In older OpenAL implementation, there's no `alcOpenDevice` and `alcCloseDevice`. The `alcCreateContext` and `alcGetError` instead only need one param to `attrList`. Since symbol name is same, game will runs but segfault crash due to stack corruption.
* The `alcCreateContext` attrList also have different const values and will not recognized by newer OpenAL, so attrList rebuilding is necessary to set into correct params.
* Context is activated by default after it's creation in older OpenAL. In later version, it's not selected by default and must be selected by calling `alcMakeContextCurrent`.
* `alcUpdateContext` seems replaced by `alcProcessContext`.
* While most `*_LOKI()` function still intact in this [OpenAL Loki](https://github.com/thiekus/openal-loki), still missing a bit of `*_LOKI()` extension functions, notably `alAttenuationScale_LOKI` is removed before 2001 (comparing with [this old Debian source](https://archive.debian.org/debian/pool/main/o/openal/openal_0.2001061600.orig.tar.gz)) and crashing SC3U. Currently I stubbed that functions.
* Old version only has OSS `/dev/dsp` and ESD backend driver which sound will not works without installing this [OSS Proxy](https://github.com/OpenMandrivaSoftware/ossp).

## Known Issues

Since this is my attempt to fix OpenAL issues of SimCity 3000 Unlimited Linux, here is my known issues so far.

* Play, stop and volume control is seems broken. For example music in SimCity 3000 will be overlapped simultaneously after you load another city when music still running. Volume in preferences didn't apply until you restart the game.
* On some soundcard or older Linux (tested on Ubuntu 6.06 Qemu KWM), SDL backend is delayed or garbled depending of soundcard you choose if using OpenAL SDL backend. There's no problem in my Ubuntu 24.04 machine.

## Building

You don't need much depedencies since this shim only need GCC that capable to build Intel x86 32 bit (by installing `gcc-multiarch`), but I recomend to build [this version of OpenAL](https://github.com/thiekus/openal-loki) since we will need to properly using this shim.

Clone this repo and build
```
git clone https://github.com/thiekus/openal-compat
cd openal-compat
make
```

Or run `make CFLAGS=-DOAL_COMPAT_NODEBUG=1` if you didn't want stderr debug log. It will be built `oalcompat.so` library.

## Usage

Install to target game directory by using symlink:
```
ln -s /path/to/oalcompat.so libopenal-0.0.so
```
It will be create `libopenal-0.0.so` symlinked to `oalcompat.so`.

By default, it will try to load real OpenAL library named `libopenal.so.0` from current directory or system library. If you follow OpenAL Loki build instruction on another directory, set by define `OAL_COMPAT_LIBRARY` environment variable, e.g `OAL_COMPAT_LIBRARY=/path/to/libopenal.so`.

## Running SimCity 3000

You still need [sc3u-nptl](https://github.com/twolife/sc3u-nptl) and [old libstdc++ from Debian](https://snapshot.debian.org/archive/debian/20060714T000000Z/pool/main/g/gcc-2.95/libstdc%2B%2B2.10-glibc2.2_2.95.4-27_i386.deb). Installing `patchelf` is not required since we use library symlinks instead of altering ELF executable. Obviously OSS Proxy Daemon isn't required.

* Ensure you have apply v2.0a patch since we need `sc3u.dynamic` instead statically linked `sc3u`.
* Make sure you have `libsdl1.2-compat-shim:i386` (or `libsdl1.2debian:i386` in older Ubuntu/Debian) installed.
* Download old libstdc++, extract `libstdc++-libc6.2-2.so.3` and `libstdc++-3-libc6.2-2-2.10.0.so` from `/lib` directory and put them on SimCity install directory alongside `sc3u.dynamic` executable.
* Clone sc3u-nptl and build.
* Clone [OpenAL Loki](https://github.com/thiekus/openal-loki), install these depedecies and build.
* Clone this repo and build this shim.
* Enter SimCity 3000 install directory, set symlinks
```
ln -s /lib/i386-linux-gnu/libSDL-1.2.so.0 libSDL-1.1.so.0
ln -s /path/to/sc3u-nptl.so sc3u-nptl.so
ln -s /path/to/oalcompat.so libopenal-0.0.so
```
* Run SimCity 3000 game with this command (set `libopenal.so.0.0.0` path to out previously build OpenAL Loki)
```
LD_LIBRARY_PATH=$(pwd) LD_PRELOAD=sc3u-nptl.so:libstdc++-libc6.2-2.so.3 OAL_COMPAT_LIBRARY=/path/to/libopenal.so.0.0.0 ./sc3u.dynamic
```
* Run SimCity 3000 BAT with this command
```
LD_LIBRARY_PATH=$(pwd) LD_PRELOAD=sc3u-nptl.so:libstdc++-libc6.2-2.so.3 OAL_COMPAT_LIBRARY=/path/to/libopenal.so.0.0.0 ./sc3bat.dynamic
```