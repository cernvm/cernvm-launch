How to build CernVM-Launch
==========================

CernVM-Launch has several dependencies: `JsonCPP`, `OpenSSL`, `zlib`, `libcurl` and `libcernvm`.

You may optionally provide your system versions (see `CMakeLists.txt` and
[`libcernvm` repository](https://github.com/cernvm/libcernvm) for details). However, in
that case, it is not guaranteed that the build will be successful, because not all
versions of the dependencies are compatible. Also, you need to provide static
libraries/binaries of the dependencies, because CernVM-Launch is building a fully static
binary.

If you do not provide the dependencies from the system, CernVM-Launch build system
will try to download and build them automatically.

1. Clone the repository and run the `prepare-*.sh` script according to your platform.
2. Go into the created `build` directory: `cd build`.
3. Build the sources: `cmake --build .`. For the initial build, do NOT use a parallel build
   (e.g. `cmake --build . -- -j 4`), because the OpenSSL build might fail in that case.
   After you have built OpenSSL, you may use parallel build as you wish.

