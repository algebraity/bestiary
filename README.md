![](https://git.keimai.space/algebraity/bestiary/raw/branch/main/bestiary-banner.png)

# Bestiary - v1.0.0: Release the BEASTs!

...the Bundles of Efficient Algorithms for Science and Technology, that is!!

Bestiary is a LaTeX-inspired mathematical computing environment built around seven C-based, animal-themed mathematics libraries. It combines a language, command-line interface, and GUI application so that these libraries can be used together as one unified system. Its core idea is simple: mathematical expressions should be typed the way mathematicians already write them. Each library is referred to as a "BEAST," and provides a set of commands that can be accessed in Bestiary. All commands in Bestiary start with a backslash `\` character, and the language is themed after LaTeX, such that many objects can be defined in a way identical to how they would be typed in LaTeX.

Each BEAST is written from scratch in C, and the wxWidgets-based GUI application, which allows for easy access to documentation while working and running multiple Bestiary shells in parallel, is written in the C++ language. For full documentation on each of the hundreds of commands and operations available in Bestiary, see the "Help" tab in the application. More information on each BEAST and its functionality can be found below.

Bestiary is currently in active development, but as of v1.0.0, it can be expected to be stable and efficient for regular use. Future releases will add new features to the core BEAST libraries where useful and fix any bugs that may be discovered, but the core architecture and functionality will remain the same. Bestiary has mostly been tested on the GNU/Linux operating system, but it is fully supported and developed for Windows as well.

## Installation

To install Bestiary, download the latest release for your platform from the repository's Releases page. Extract the downloaded archive to a convenient location.

On Windows, open the extracted folder and run:

```bat
install.bat
```
This installs Bestiary into the default local application folder and creates shortcuts when possible. You can also run `bestiary.exe` directly from the extracted folder if you prefer to use it as a portable application.

On GNU/Linux, the extracted archive contains the Bestiary executables. You can run them directly from the extracted folder, or move them to a directory used for applications on your system, such as `/bin`, `/usr/bin`, `/usr/local/bin`, or another directory on your `PATH`. If you want Bestiary to appear in your desktop environment's application launcher, you may also create a `.desktop` file pointing to the bestiary executable.

## GUI application

Bestiary can be used either through the GUI application, `bestiary`, or through the command-line interpreter, `bestiary-cli`. The GUI is recommended for serious work, since it provides multiple shell tabs, searchable documentation, script tabs, and convenient access to the same evaluator used by the CLI.

Here are some screenshots from Bestiary:

<table>
  <tr>
    <td align="center" width="33%">
      <img src="https://git.keimai.space/algebraity/bestiary/raw/branch/main/art/screenshots/2026-04-26-201124_hyprshot.png" alt="The Bestiary start page" width="300"><br>
      <em>The Bestiary start page.</em>
    </td>
    <td align="center" width="33%">
      <img src="https://git.keimai.space/algebraity/bestiary/raw/branch/main/art/screenshots/2026-04-26-201240_hyprshot.png" alt="An example of a Bestiary shell" width="300"><br>
      <em>An example of a Bestiary shell.</em>
    </td>
    <td align="center" width="33%">
      <img src="https://git.keimai.space/algebraity/bestiary/raw/branch/main/art/screenshots/2026-04-26-201253_hyprshot.png" alt="The main help page, with links to BEAST-specific help pages" width="300"><br>
      <em>The Sokko help page.</em>
    </td>
  </tr>
</table>

## Usage examples

To get an idea of how Bestiary is used and why it is powerful, consider a series of commands executed in Bestiary:

```
Bestiary v1.0.0 -- Ctrl+C cancels, Ctrl+D quits, Ctrl+L clears screen
\help lists commands, \help{commandName} displays command info
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

As can be seen from this example, there are both easier native ways to define objects, and methods which are identical to how LaTeX is commonly used. In many cases, this makes it possible to paste mathematical expressions directly from LaTeX source into Bestiary and compute with them. This goes farther than just defining objects. Consider this example, using the same definition of `A` as in the first:

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

Yet again, a line that is often used in LaTeX source documents can be directly pasted into Bestiary to get the desired result. For common operations and invariants, the goal is that you should not need to translate mathematics into a special programming idiom: you should be able to type something close to the way you would write it in a paper.

There are many commands in Bestiary that are not commonly used in LaTeX, but they are a combination of LaTeX syntax with intuitive function names, e.g. `\isIrrep` determines whether a representation is irreducible or not, and `\listElements` lists the elements of a group or a ring. The idea is that all you need to learn Bestiary is familiarity with LaTeX, which any mathematician will have, and the rest will follow naturally. That's what makes it powerful.

## Scripting

Bestiary can run any ".bsy" file as a line-by-line script:

```sh
./build/bin/linux/bestiary-cli file.bsy
```

Inside of a Bestiary environment, use `\run{file.bsy}` or `\run{"path with spaces.bsy"}` to run
a script in the current session. Script lines share the same evaluator and
environment as the REPL, so assignments made by a script remain available after
`\run` finishes. Scripts can also be loaded into the GUI application as tabs.

To write a script, simply create a `.bsy` file with a series of commands as you would type them into the interpreter. Semicolons are not needed to end lines.

## Meet the BEASTs

Bestiary is powered by 7 C-based mathematics libraries, each of which is named after the Japanese word for an animal, which also functions as a backronym for the full name. They each provide different commands and functions to the program, and they build on and require each other to be complete.

* HEBI (蛇): Highly Efficient Basic math Interpreter (basic math and shared libraries)
  * the versatile serpent, providing shared libraries used by the other BEASTs
  * powers other powerful BEASTs, and provides efficient functions and structs for basic mathematics
  * Main features: complex arithmetic, basic functions, transcendental constants
* Sokko (速狐): Speedy Kitsune (linear algebra)
  * the swift fox, providing a fast, self-contained linear algebra library
  * provides fully self-contained matrix and vector operations, and provides the backbone for other BEASTs requiring linear algebra
  * Main features: matrices, vectors, row reduction, column reduction, eigenvalues and eigenvectors, efficient LU decomposition
* USAGI (兎): Utilities Bundle for Algebra and Group Invariants (finite group and ring theory)
  * the petite rabbit, hopping through rings, skipping through fields, and always staying in a group (;3)
  * provides a full library for finite group and ring theory, including constructors for common objects, quotients, homomorphisms, and much more
  * Main features: groups, rings, fields, sub-objects, quotients, homomorphisms, ideals, advanced computations
* NEKO (猫): Numerical Estimation Kernel with Optimizations (calculus,numerical analysis, and solvers)
  * the supple feline, squeezing quickly and leanly into even the tightest of functions and ODEs
  * offers symbolic and numerical methods for calculus and applications
  * Main features: differentiation, integration, numerical rootfinding, ODE solvers, polynomial factorization over R and C
* TORA (虎): Toolkit Of Representation-theoretic Algorithms (representation theory of finite groups)
  * the ferocious tiger, suitable for wrangling representations of even the trickiest of finite groups
  * provides a full library for representation and character computation
  * Main features: representations, characters, speedy character table computations for finite groups
* PONI (ポニ): Physics Operations and Numerical Interpreter (physics, kinematics, simulations)
  * the majestic pony, providing solutions to basic physics problems and multibody simulations
  * makes use of Sokko to provide vector-based kinematics and dynamics, with more planned for future updates
  * Main features: kinematics, dynamics, force, energy, work, multibody simulations
* OOKAMI-ng (狼): Operations Over K-fold Addition and Multiplicative Integer sets (integer combinatorics)
  * the fearless wolf, hunting down the properties of sets of integers with ferocious intensity
  * a self-contained library for integer combinatorics based on [OOKAMI](https://git.keimai.space/algebraity/ookami)
  * Main features: computing sumsets, difference sets, product sets, rep functions, energies, APs and GPs, and much more

## Dependencies

For GNU/Linux, Bestiary is expected to run out of the box. For Windows, the necessary DLL files are shipped with `bestiary.exe`, and are installed when the user runs `install.bat`. If any dependencies are missing, please create an issue to report a bug.

## Building

Bestiary can be built on GNU/Linux with:

```sh
make
```

This produces the GUI application and command-line interpreter under:

```sh
./build/bin/linux/
```

The main executables are:

```sh
./build/bin/linux/bestiary
./build/bin/linux/bestiary-cli
```

To build the Windows version from GNU/Linux, use a MinGW-w64 toolchain and run:

```sh
make windows
```

To build release artifacts for all supported platforms, run:

```sh
make release
```

A portable Windows bundle can be created with:

```sh
make windows-bundle
```

This places the Windows executables, required DLLs, banner image, and installer script in:

```sh
build/dist/windows/bestiary/
```

The resulting folder can be distributed directly. On Windows, users may either run `bestiary.exe` from the bundle folder or run `install.bat` to install Bestiary into `%LOCALAPPDATA%\Bestiary`.

## License and attribution

The contents of this repository and the corresponding GitHub Releases page are licensed under the GNU General Public License v3.0 (GPL-3.0).
