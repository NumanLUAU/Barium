# building barium

this guide will show you how to build barium from scratch. we'll be using **wsl2** (windows subsystem for linux) because it's the easiest way to get a working build environment on windows.

## 1. get wsl2
if you don't have wsl yet, open up **powershell** (as admin) and type:
```powershell
wsl --install
```
restart your computer if it tells you to. 

## 2. prepare your environment
open your wsl terminal. the first thing you should do is move to your linux home directory:
```bash
cd ~
```

now install the basic tools:
```bash
sudo apt update
sudo apt install -y build-essential bison flex libgmp3-dev libmpfr-dev libmpc-dev texinfo make nasm gcc-mingw-w64-x86-64 mtools dosfstools qemu-system-x86 curl
```

## 3. clone the code
```bash
git clone https://github.com/pxlatd/Barium.git
cd Barium
```

## 4. build the toolchain
we need a version of the `gcc` compiler that builds code differently. we've included a script that does this for you automatically. **this will take a while** (10-20 mins), so grab a snack.

```bash
chmod +x toolchain/build.sh
./toolchain/build.sh
```

once it's done, you need to tell your computer where the new compiler is:
```bash
echo 'export PATH="$HOME/barium-toolchain/bin:$PATH"' >> ~/.bashrc
```

## 5. build barium
now you can finally build the OS:
```bash
make
```
this will create `barium.img`.

## 6. run it
to test it in qemu:
```bash
make run-img
```

## tips
- if `make` says `command not found`, make sure you did the `export PATH` step correctly.
- always make sure you are in your linux home folder (`~`) before cloning.
