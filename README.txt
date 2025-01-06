Currently using:

Windows 10 (Do not have access to my machine currently; I intend to transfer to linux when possible)

CMake
ISO C++17 Standard

OneAPI Intel C++ Compiler
OneAPI TBB Install

Compiled embree4 binaries with default settings

ImageMagick

Any major project requirements will be listed here.


optional if CMakeList sets up properly:
To set up in visual studio code, go to project properties and add compiled embree's:
	inc to c/c++ -> general -> includes
	lib to linker -> general -> additional libary
	bin to (can't remember; argubarly optional??)
	and embree4.lib to linker's input additional libaries 