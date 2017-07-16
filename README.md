Term48 is a terminal emulator for BlackBerry. It implements (relevant parts of) the [ECMA-48 standard][ecma], but also includes some other control sequences to make it compliant with the xterm-256color terminfo specification. It is a work in progress, but is good enough to be used regularly by the author.

You can get Term48 from BB World.

[http://appworld.blackberry.com/webstore/content/26272878/](http://appworld.blackberry.com/webstore/content/26272878/)

The current version runs on BB10 devices, and requires version >= 10.3. An older version is available for the Playbook.

To compile Term48, you will need some additional libraries:

* [libSDL][libsdl]
* [Touch Control Overlay][tco]
* [libconfig][libconfig]

Prebuilt versions of these shared libraries are available in `external/lib` (see Makefile); to build from source you will need to check out the submodules (call `git clone` with the `--recursive` option) and build them with the Momentics IDE.

You can build and deploy Term48 without using Momentics IDE:

* Load the proper `bbndk-env` file
* Copy your debug token to `signing/debugtoken.bar` (or see section below on generating a debug token)
* Populate the `BBIP` and `BBPASS` fields in `signing/bbpass` with your device's dev-mode IP address and device password
* `make`
* `make deploy`

You can set whatever PackageID and whatnot that you like in the bar-descriptor.xml file. If you want to be able to access shared files on the device, check the 'Files' box under `Application/Permissions`.

As this is a work in progress, pull requests or feature requests are welcome. Please report any bugs with an Issue.

[ecma]: http://www.ecma-international.org/publications/standards/Ecma-048.htm
[libsdl]: https://github.com/mordak/SDL/tree/term48
[tco]: https://github.com/blackberry/TouchControlOverlay
[libconfig]: http://www.hyperrealm.com/libconfig/

# Generating a Debug Token

* Use this form to obtain your `bbidtoken.csk` file: https://developer.blackberry.com/codesigning/
* Copy `bbidtoken.csk` to `signing/bbidtoken.csk`
* In `signing/bbpass`, fill in:
  - `CNNAME`: the Common Name for your signing cert (usually your name)
  - `KEYSTOREPASS`: CSK password you entered in step 1 signup
  - `BBPIN`: target device's PIN
  - `BBPASS`: target device's password

Important: any symbols need to be escaped according to bash / Makefile rules e.g. backslashes before symbols `\!` and double dollar signs `\$$`.
