# SoundBlaster digital audio playback library

This library implements digital audio playback on a SoundBlaster or compatible sound device.
It can play back audio files in the "Creative Voice" format (`.VOC`),
including files that use Creative ADPCM compression.

The library also detects the presence of SoundBlaster and/or AdLib cards in the system,
taking the `BLASTER` environment variable into account if present,
and then sets global variables to inform client code about which audio devices are present.

Written by Jason Blochowiak, the code is largely quite similar to sound code found in Wolfenstein 3D,
aside from the VOC file support (Wolfenstein uses a different file format).

I believe that this library was delivered as a header file and compiled object file,
not as source code.
This is because the code was built using Borland C++ 3.1 instead of Turbo C++ 3.0 like the rest of the project,
and the compiler flags used are also fairly different - for example,
the code only uses 8088/8086-compatible instructions and was compiled with a high optimization level (`-O2 -Z`).
The rest of the executable uses 80286-compatible instructions and was compiled with very few optimizations.

I organized the code accordingly,
to try and recreate how it might have looked like originally.
The header file and object file here is what the original Duke Nukem II developers
likely had to work with,
while the `SRC` subdirectory contains the source code that Jason would have had.

This arrangement is interesting, since it seems that Apogee previously had source code access to Id's sound code,
e.g. in older games like Cosmo.

Another interesting observation is that the library replaces most standard library functions
with its own versions.
We can only speculate as to why, but perhaps this was done to make the file size cost of including the sound library
more predictable. Borland compilers from this era always link the C library statically,
which means that using a standard library function that's not used anywhere else in the executable
would increase the executable's file size.
By avoiding the standard library, the sound library's footprint in the final binary becomes relatively constant.
Now, it's not 100% constant because certain code constructs are still compiled into C runtime function calls,
even though they look like plain code.
For example, multiplying 32-bit integers or incrementing a `huge` pointer.
I guess it was very likely for these internal C runtime functions to already be
present in most executables, so that their use in the sound library was unlikely to add any
extra weight to the C runtime section of the binary.
