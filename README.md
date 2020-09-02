# Cortex Command Community Project Source #
*The Cortex Command Community Project is Free/Libre and Open Source under GNU AGPL v3*

This is a community-driven effort to continue the development of Cortex Command. Stay up to date in our [discord channel](https://discord.gg/SdNnKJN).

***

## Setup Process ##

First you need to download the necessary files:

1. Install the necessary tools.  
You'll probably want [Visual Studio Community Edition](https://visualstudio.microsoft.com/downloads/) (we mostly use 2017 but 2019 should be fine).  
You also need to have [Visual C++ Redistributable for Visual Studio 2015 (x86)](https://www.microsoft.com/en-us/download/confirmation.aspx?id=48145&6B49FDFB-8E5B-4B07-BC31-15695C5A2143=1) installed in order to run the compiled builds.  
You may also want to check out the list of recommended Visual Studio plugins [here](https://github.com/cortex-command-community/Cortex-Command-Community-Project-Source/wiki/Information,-Recommended-Plugins-and-Useful-Links).

2. Clone this Source Repository and the [Data Repository](https://github.com/cortex-command-community/Cortex-Command-Community-Project-Data) in neighboring folders.  
**Do Not** change the folder names unless you want to make trouble for yourself.

3. Copy the following libraries from `Cortex-Command-Community-Project-Source\external\lib\` into the Data Repository:
* `lua51.dll`
* `fmod.dll`
* `liblz4.dll`
* `zlibwapi.dll`

4. Copy `Scenes.rte` and `Metagames.rte` from your purchased copy of Cortex Command into the Data Repository.

Now you're ready to build and launch the game.  
Simply open `RTEA.sln` with Visual Studio, choose your configuration, and run the project.

* Use "`Debug Open Source`" configuration to debug (this runs very slowly)
* Use "`Minimal Debug Open Source`" configuration to debug with all visual debug elements disabled (this runs slightly faster)
* Use "`Final Open Source`" configuration to build release .exe

The first build will take a while, but future ones should be quicker.

## Linux Build Instructions ##
The Linux build uses the meson build system, and builds against system libraries

Dependencies:

* `allegro4`
* `loadpng`
* `flac`
* `luajit`
* `minizip`
* `lz4`
* `libpng`
* `libX11`
* `meson>=0.49`

Building:

1. Install Dependencies (see below for some distro-specific instructions)

2. Clone this Source Repository and the [Data Respository](https://github.com/cortex-command-community/Cortex-Command-Community-Project-Data), folder structure doesn't really matter here

3. open a terminal in the Source Repository

4. `meson builddir`

5. `cd builddir`

6. `meson compile` or `meson compile CCOSS` if you want a release build

Running:

1. Copy `CCOSS_debug.x86_64` or `CCOSS.x86_64` (depending on if you made a release build) into the **Data Repository**

2. Copy libfmod.so.11 and libfmod.so.11.11 from external/linux/x86_64 into the **Data Repository**

3. Run `env LD_LIBRARY_PATH=. ./CCOSS.x86_64` or `./CCOS_debug.x86_64` in the **Data Repository**

### Arch Linux ###
`pacman -S allegro4 flac luajit minizip lz4 libpng meson`
### Ubuntu ###
`apt-get install liballegro4.4 libloadpng4-dev libflac++-dev luajit-5.1-dev libminizip-dev liblz4-dev libpng++-dev libx11-dev meson`

## More Information ##

For more information and recommendations, see [here](https://github.com/cortex-command-community/Cortex-Command-Community-Project-Source/wiki/Information,-Recommended-Plugins-and-Useful-Links).
