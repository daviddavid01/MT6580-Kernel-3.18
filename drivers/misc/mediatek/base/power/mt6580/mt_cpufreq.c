name: Build MT6580 LineageOS Kernel Online

on:
  workflow_dispatch:

env:
  ARCH: arm
  SUBARCH: arm
  CROSS_COMPILE: arm-linux-gnueabi-
  # Suppress modern toolchain warnings and symbol conflicts on vintage 3.18 trees
  HOSTCFLAGS: "-O2 -fcommon"
  CFLAGS: "-O2 -fcommon -Wno-pointer-sign -Wno-error=implicit-function-declaration"
  KCFLAGS: "-O2 -fcommon -Wno-pointer-sign"

jobs:
  build:
    runs-on: ubuntu-latest
    
    # Modern runner environment pointing back to a 20.04 compiler subsystem
    container:
      image: ubuntu:20.04
      options: --user root

    steps:
    - name: Checkout Kernel Source Tree
      uses: actions/checkout@v4

    - name: Initialize Environment Dependencies
      run: |
        apt-get update && apt-get install -y sudo software-properties-common tzdata gnupg2
        sudo dpkg --add-architecture i386
        sudo apt-get update
        sudo DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential libncurses5-dev bison flex libssl-dev libelf-dev bc git ccache \
          lib32z1 lib32ncurses6 lib32stdc++6 u-boot-tools libncurses5:i386 \
          gcc-arm-linux-gnueabi gcc-arm-linux-gnueabihf

    - name: Patch Cross-Compiler Header Tree
      run: |
        # Seamless legacy compiler translation loop matching the host container matrix
        for VER in 5 6 7 8 9 10 11 12 13 14; do
          if [ ! -f "include/linux/compiler-gcc${VER}.h" ]; then
            cp include/linux/compiler-gcc4.h include/linux/compiler-gcc${VER}.h 2>/dev/null || touch include/linux/compiler-gcc${VER}.h
          fi
        done

    - name: Configure Target Architecture Matrix
      run: |
        # Explicit search priority order custom tailored to Cubot Rainbow porting vectors
        if [ -f "arch/arm/configs/rainbow_defconfig" ]; then
          make rainbow_defconfig
        elif [ -f "arch/arm/configs/p4200_debug_defconfig" ]; then
          make p4200_debug_defconfig
        elif [ -f "arch/arm/configs/cro_u00_defconfig" ] || [ -f "arch/arm/configs/huawei_y3_defconfig" ]; then
          make *cro*defconfig || make *y3*defconfig
        else
          echo "Fallback mapping to standard platform tree profile..."
          make mt6580_defconfig
        fi

    - name: Execute Source Compilation
      run: |
        # Direct generation of the kernel image via multi-threaded allocation
        make -j$(nproc) zImage

    - name: Archive Kernel Deliverables
      uses: actions/upload-artifact@v4
      with:
        name: MT6580-zImage-Lineage-14.1
        path: arch/arm/boot/zImage
