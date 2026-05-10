#!/bin/bash
set -e

PREFIX="$HOME/barium-toolchain"
TARGET=x86_64-elf
PATH="$PREFIX/bin:$PATH"

BUILD_DIR="$HOME/barium-build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -d "binutils-2.41" ]; then
    curl -O https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.gz
    tar -xf binutils-2.41.tar.gz
fi

mkdir -p build-binutils
cd build-binutils
../binutils-2.41/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd ..

if [ ! -d "gcc-13.2.0" ]; then
    curl -O https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
    tar -xf gcc-13.2.0.tar.gz
fi

mkdir -p build-gcc
cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc
cd ..

echo "toolchain ready at $PREFIX"
