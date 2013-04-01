Term48 is a terminal emulator for BlackBerry (PlayBook). It implements (relevant parts of) the [ECMA-48 standard][ecma], but also includes some other control sequences to make it compliant with the qansi terminfo specification. It is a work in progress, but is good enough to be used regularly by the author.

To compile Term48, you will need some additional libraries:

* [libSDL][libsdl]
* [Touch Control Overlay][tco]
* [libconfig][libconfig]

Once these have been downloaded and compiled with the BlackBerry dev tools, you can build Term48 like so:

* Create a new C/C++ project in Momentics
* Pull down the source from this repo into the src directory of your project
* Add libSDL12.so, libTouchControlOverlay.so, and libconfig.so.10 as assets in bar-descriptor.xml
* In the project Properties, under C/C++ Build/Settings:
    * Add the following include directories to the QCC Compiler Preprocessor:
        * SDL/include
        * TouchControlOverlay/inc
        * libconfig/include
    * Under the QCC Compiler Preprocessor Defines, add `__PLAYBOOK__`
    * Under the QCC Linker Libraries, add the following Library Paths:
        * SDL12/Device-Release
        * TouchControlOverlay/Device-Release
        * libconfig/lib
    * Under the QCC Linker Libraries, add the following Libraries:
        * bps
        * icui18n (BB10 users may also need icuuc)
        * SDL12
        * screen
        * m
        * freetype
        * TouchControlOverlay
        * config
        * clipboard
* In the project Properties, under Project References, you can check the boxes for SDL12 and TouchControlOverlay if you like.

Once this is done, the Term48 source should compile and link. You can set whatever PackageID and whatnot that you like in the bar-descriptor.xml file. If you want to be able to access shared files on the device, check the 'Files' box under Application/Permissions.

As this is a work in progress, pull requests or feature requests are welcome. Please report any bugs with an Issue.

[ecma]: http://www.ecma-international.org/publications/standards/Ecma-048.htm
[libsdl]: https://github.com/mordak/SDL/tree/term48
[tco]: https://github.com/blackberry/TouchControlOverlay
[libconfig]: http://www.hyperrealm.com/libconfig/






