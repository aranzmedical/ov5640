# OV5640-ARANZ Driver #

This repository contains our modified OV5640 driver, with enhancements to speed and quality, and extra controls.

## Building ##

It is recommended you only build this within the Yocto buildsystem, as it can be quite tricky to get the environment 
set up right.

It is possible to build stand-alone, but you will need to set up your environment *EXACTLY* or the module's 'vermagic'
won't match, or worse, it will be built against the wrong kernel and completely fail to load. To ensure your
environment is set up right, follow these steps:

### Environment ###

Like building anything outside of Yocto, you must set up the environment, through the environment-setup-* script 
(which may be either the one in build/tmp/ or the one provided by the SDK installer)

Compiling linux modules requires some extra effort though - you need to tell it where it can find the linux source code 
and build artifacts. If compiling from the command-line, run:

    > export KERNEL_SRC=<yocto_root>/build/tmp/work-shared/<machine>/kernel-source
    > export KERNEL_BUILD=<yocto_root/build/tmp/work-shared/<machine>/kernel-build-artifacts

If compiling from eclipse, you can set an Environment variable under Project->Properties->C/C++ Build->Environment, 
then add KERNEL_SRC & KERNEL_BUILD set to the above paths.

Make sure the kernel AND image is built before attempting to build this driver! (`bitbake virtual/kernel && bitbake <image>`)
If the image is not build, the `work-shared/<machine>/kernel-*` directories won't be updated with the latest build.

### Dependencies ###

    ov5640_camera_mipi: no symbol version for module_layout
    modprobe: ERROR: could not insert 'ov5640_camera_mipi': Exec format error

If you get symbol errors when trying to modprobe the module, and it is missing dependencies when you 'modinfo' it,
then you're probably missing the 'Module.symvers' file which sets up dependencies to other modules. You could 
also get this error if the Module.symvers file is different from the one in the kernel build.

Note that the Makefile automatically links to the Module.symvers file in the kernel-build-artifacts directory.

### 'vermagic' ###

If the module fails to modprobe at all, run `modinfo ov5640_camera_mipi` and check the vermagic property
matches the other modules on the system. If they are different, linux will not let you modprobe it!

You can work around this by forcefully modprobing it (`modprobe ov5640_camera_mipi -f`), which is probably
fine if you're just doing some quick development.

If you encounter a -dirty string in the vermagic string, this is caused by a dirty git directory
(ie, contains modified files which haven't been comitted)

For more information about vermagic strings, see the following links:

http://linux.die.net/lkmpg/x380.html

http://billauer.co.il/blog/2013/10/version-magic-insmod-modprobe-force/


