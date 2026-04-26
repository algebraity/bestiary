![](https://git.keimai.space/algebraity/bestiary/raw/branch/main/bestiary-banner.png)

# Bestiary: release the BEASTs!!

...the Bundles of Efficient Algorithms for Science and Technology, that is!!

---

HEBI
---
the core beast.
it stands for Highly Efficient Basic math Interpreter.
HEBI is a highly efficient basic math interpreter that provides shared libaries used by the other beasts.
HEBI powers other powerful beasts, and provides shared libraries for basic mathematics

OOKAMI-ng
---
OOKAMI is a special one of the beasts.
it stands for Operations Over K-fold Additive and Multiplicative Integer sets.
OOKAMI is a library for Operations Over K-fold Additive and Multiplicative Integer sets.

Sokko
---
the speedy kitsune.
it is a library for linear algebra, math with vectors and matrices.

USAGI
---
the tools of power.
it stands for Utility Suite for Algebra and Group Invariants.
USAGI is for abstract algebra, group and ring theories.

PONI
---
the spatial warrior.
it stands for Physics Opertions and Numerical Interpreter.
PONI is suited for physics, simple kinematics and dynamics.

TORA
---
the swiss army knife, but rawr.
it stands for Toolkit Of Representation-theoretic Algorithms.
it is helpful for representation theory of finite groups.

Building
---
GNU/Linux is the default build path and uses GNU readline for interactive
history, editing, and tab completion:

```sh
make
./build/bin/linux/bestiary
```

The Windows build path uses the vendored linenoise backend for interactive
history, editing, and tab completion without requiring readline:

```sh
make windows
```

That produces `build/bin/windows/repl.exe` when a MinGW-w64 compiler such as
`x86_64-w64-mingw32-gcc` is installed. The Windows target statically links the
MinGW support library where possible; with UCRT-based MinGW toolchains it still
uses Microsoft's standard Windows/UCRT runtime DLLs. `make release` builds both
supported platform targets.

GUI
---
The wxWidgets GUI is an optional wrapper around the `bestiary` executable:

```sh
make gui
./build/bin/linux/bestiary-gui
```

The GUI opens at 480x480 with a pinned start page, tabbed Bestiary sessions,
script launching, and a placeholder help tab. It expects `wx-config` on
GNU/Linux. For Windows cross-builds, use:

```sh
make windows-gui
```

That target expects a MinGW-compatible wxWidgets config tool named
`x86_64-w64-mingw32-wx-config`, or pass `WX_CONFIG=/path/to/wx-config`. On
systems where the packaged symlink is broken, the Makefile falls back to
`/usr/x86_64-w64-mingw32/bin/wx-config` automatically when it exists.
The Windows GUI uses ConPTY for the embedded terminal, so `bestiary-gui.exe`
requires Windows 10 version 1809 or newer (or Windows 11).

To make a runnable Windows bundle with the required MinGW runtime DLLs copied
next to the executables, use:

```sh
make windows-bundle
```

That produces a portable folder at `build/dist/windows/bestiary/` containing
`bestiary.exe`, `repl.exe`, `bestiary-gui.exe`, `bestiary-banner.png`, and the
needed MinGW DLLs such as `libpng16-16.dll`, `libtiff-6.dll`, `libjpeg-8.dll`,
`libexpat-1.dll`, `zlib1.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`,
`libssp-0.dll`, and `liblzma-5.dll`. This is the simplest way to distribute
the Windows build to users.

The bundle also includes `install.bat`, which copies the whole folder into
`%LOCALAPPDATA%\Bestiary` by default and creates Start Menu shortcuts when
possible. Users can also run `bestiary-gui.exe` directly from the portable
bundle folder. Do not distribute or move the `.exe` files by themselves; the
DLLs need to stay next to the executables.

If you want a traditional installer that creates Start Menu entries or installs
into `Program Files`, use an external installer builder such as Inno Setup or
NSIS and include the contents of that bundle folder. Bestiary itself should not
try to install DLLs into a user's system directories at runtime.

Scripts
---
Bestiary can run any text file as a line-by-line script:

```sh
./build/bin/linux/bestiary file.bsy
```

Inside the REPL, use `\run{file.bsy}` or `\run{"path with spaces.bsy"}` to run
a script in the current session. Script lines share the same evaluator and
environment as the REPL, so assignments made by a script remain available after
`\run` finishes.
