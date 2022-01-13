# HatchGameEngine
Multiplatform engine powering projects and making ideas into reality.

I don't actively push to this, but this is here for portfolio reasons. Feel free to look around; however, this may not build without the correct libraries.

## Documentation

## Building
### Windows
Included in /VisualC is a Visual Studio 2019 solution. You'll need the x86 version of the [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019](https://support.microsoft.com/en-us/topic/the-latest-supported-visual-c-downloads-2647da03-1eea-4433-9aff-95f26a218cc0) installed to compile in Visual Studio.

### Linux
Use CMake to generate your build files.

```
cmake -S . -B ./builds/linux
```

#### Cross-compiling

You can build a Windows executable under Linux, using `CMakeLists.txt` combined with a toolchain. Included in /cmake is `cross_compile.sh`, which can do that for you. Run it while in the `cmake` directory. Example:

```
cd cmake
./cross_compile.sh
cd ../builds/cross-windows
make
```

## Dependencies
Required:
- SDL2 (https://www.libsdl.org/)
- Visual C++ (for Windows building)
- Android Studio (for Android building)
- Xcode 12 (for iOS building)
- devKitPro (for Nintendo Switch homebrew building)

Optional:
- FFmpeg (for video playback, currently broken)
- CURL (for simple HTTP network, currently broken)
- libpng
- libjpeg
- libogg
- libfreetype
