![](https://git.keimai.space/algebraity/bestiary/raw/branch/main/bestiary-banner.png)

# Bestiary - v1.0.0: release the BEASTs!!

...the Bundles of Efficient Algorithms for Science and Technology, that is!!

Bestiary is a language, command-line interface, and GUI application through which a collection of C-based animal-themed mathematics libraries are accessed and used together seamlessly, as though they were all one program. Each library is referred to as a "BEAST," and provides a set of commands that can be accessed in Bestiary. All commands in Bestiary start with a backslash `\` character, and the language is themed after LaTeX, such that many objects can be defined in a way identical to how they would be typed in LaTeX.

Each BEAST is written from scratch in C, and the wxWidgets-based GUI application, which allows for easy access to documentation while working and running multiple Bestiary shells in parallel, is written in the C++ language. For full documentation on each of the hundreds of commands and operations available in Bestiary, see the "Help" tab in the application. More information on each BEAST and its functionality can be found below

## Usage examples

To get an idea of how Bestiary is used and why it is powerful, consider a series of commands executed in Bestiary:

```
Bestiary v0.0.1 (flags: --tokens --ast)  -- Ctrl+C cancels, Ctrl-D quits
> A = \begin{matrix} 1 & 1 \\ 1 & 0 \end{matrix}
=> ()
> A
=> 
| 1 , 1 |
| 1 , 0 |

> Aeasy = [1, 1; 1, 0]
=> ()
> Aeasy
=> 
| 1 , 1 |
| 1 , 0 |

> A == Aeasy
=> true
> 
```

As can be seen from this example, there are both easier native ways to define objects, and methods which are identical to how LaTeX is commonly used to define objects. This means it is in theory possible to paste a line into Bestiary directly from a paper to compute a result. This goes farther than just defining objects. Consider this example, using the same definition of `A` as in the first:

```
> A \otimes A
=> 
| 1 , 1 , 1 , 1 |
| 1 , 0 , 1 , 0 |
| 1 , 1 , 0 , 0 |
| 1 , 0 , 0 , 0 |

> \det(A \otimes A)
=> 1
```

Yet again, a line that is often used in LaTeX source documents can be directly pasted into Bestiary to get the desired result. There's no need to study documentation carefully and learn how to perform basic operations or compute invariants: you can type as you would a paper, and it will work out.

There are many commands in Bestiary that are not commonly used in TeX, but they are a combination of TeX syntax with intuitive function names, e.g. `\isIrrep` determines whether a representation is irreducible or not, and `\listElements` lists the elements of a group or a ring. The idea is that all you need to learn Bestiary is familiarity with TeX, which any mathematician will have, and the rest will follow naturally. That's what makes it powerful.


## Meet the BEASTs

Bestiary is powered by 7 C-based mathematics libraries, each of which is named after the Japanese word for an animal, which also functions as a backronym for the full name. They each provide different commands and functions to the program, and they build on and require each other to be complete.

* HEBI (蛇): Highly Efficient Basic math Interpreter
  * the versatile serpent, provides shared libaries used by the other BEASTs
  * powers other powerful BEASTs, and provides shared libraries for basic mathematics
  * Main features: complex arithmetic, basic functions, transcendental constants
* OOKAMI-ng (狼): Operations Over K-fold Addition and Multiplicative Integer sets
  * the fearless wolf, hunting down the properites of sets of integers with ferocious intensity
  * a self-contained libary for integer combinatorics based on [OOKAMI](https://git.keimai.space/algebraity/ookami)
  * Main features: computing sumsets, difference sets, product sets, rep functions, energies, APs and GPs, and much more
* Sokko (速狐): Speedy Kitsune
  * the swift fox, providing a full linear algebra library at the speed of fast, well-written C
  * provides fully self-contained matrix and vector operations, and provides the backbone for other BEASTs requiring linear algebra
  * Main features: matrices, vectors, row reduction, column reduction, eigenvalues and eigenvectors, efficient LU decomposition
* USAGI (兎): Utilities Bundle for Algebra and Group Invariants
  * the petite rabbit, hopping through rings, skipping through fields, and always staying in a group (;3)
  * provides a fully library for finite group and ring theory, including constructors for common objects, quotients, homomorphisms, and much more
  * Main features: groups, rings, fields, sub-objects, quotients, homomorphisms, ideals, advanced computations
* PONI (ポニ): Physics Operations and Numerical Interpreter
  * the majestic pony, providing solutions to basic physics problems and multibody simulations
  * makes use of Sokko to provide vector-based kinematics and dynamics, with more planned for future updates
  * Main features: kinemtics, dynamics, force, energy, work, multibody simulations
* TORA (虎): Toolkit Of Representation-theoretic Algorithms
  * the ferocious tiger, suitable for wrangling representations of even the trickiest of finite groups
  * provides a full library for representation and character computation
  * Main features: representations, characters, speedy character table computations for finite groups


## GUI application

Bestiary is both a CLI-based language and interpreter, and a GUI application. Use of the latter is recommended for serious work, as the GUI provides convenience and access to extensive and searchable documentation, and this is the intended way to use the program for most users.

Here are some screenshots from Bestiary:

![](https://git.keimai.space/algebraity/bestiary/raw/branch/main/art/2026-04-26-183526_hyprshot.png)

![](https://git.keimai.space/algebraity/bestiary/raw/branch/main/art/2026-04-26-184635_hyprshot.png)

![](https://git.keimai.space/algebraity/bestiary/raw/branch/main/art/2026-04-26-184559_hyprshot.png)

## Scripting

Bestiary can run any text file as a line-by-line script:

```sh
./build/bin/linux/bestiary file.bsy
```

Inside the REPL, use `\run{file.bsy}` or `\run{"path with spaces.bsy"}` to run
a script in the current session. Script lines share the same evaluator and
environment as the REPL, so assignments made by a script remain available after
`\run` finishes. Scripts can also be loaded into the GUI application as tabs.

To write a script, simply create a text file with a series of commands as you would type them
into the interpreter, and running the script will have the same result as if the commands had
been run in succession in the REPL. Semicolons are not needed to ends lines.

## Dependencies

For GNU/Linux, Bestiary is expected to run out of the box. for Windows, the necessary DLL files are shipped with `bestiary.exe`, and are installed when the user runs `install.bat`. If any dependencies are missing, please create an issue to report a bug.

## Building

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

The Windows build process produces `build/bin/windows/repl.exe` when a MinGW-w64 compiler such as
`x86_64-w64-mingw32-gcc` is installed. The Windows target statically links the
MinGW support library where possible; with UCRT-based MinGW toolchains it still
uses Microsoft's standard Windows/UCRT runtime DLLs. `make release` builds both
supported platform targets.

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

## License and attribution

The contents of this repository and the corresponding GitHub Releases page are licensed under the GNU General Public License v3.0 (GPL-3.0).
