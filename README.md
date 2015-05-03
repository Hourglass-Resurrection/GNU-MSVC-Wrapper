# GNU-MSVC-Wrapper
GNU toolchain wrappers for Visual Studio 2010 SP1 (and probably newer).  
Allows building with the GNU toolchain through custom compiler flags and keeps the MSVC toolchain intact.

To use this wrapper:
* Install the mingw64-32bit toolchain
* Open the solution and build Release or Debug (Release recommended)
* Open the Release or Debug folder, open mingw32_bindir.txt
* Paste the absolute path to the mingw64 bin folder.
* Save file.
* Copy the cl.exe and the mingw32_bindir.txt file to the directories containing .vcxproj files
* Use the (or create a) Release-GCC and/or Debug-GCC target by adding the custom compile switch /GCCBuild

Please note that linking with GNU ld is not yet implemented.