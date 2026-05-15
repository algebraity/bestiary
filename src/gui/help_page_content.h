#ifndef BESTIARY_HELP_PAGE_CONTENT_H
#define BESTIARY_HELP_PAGE_CONTENT_H

#include<cstddef>

namespace BestiaryHelpPage {

enum class Beast {
    Basic,
    Hebi,
    Sokko,
    Neko,
    Poni,
    Usagi,
    Tora,
    Ookami,
    Kuma,
    Quaternionic,
};

struct Section {
    Beast beast;
    const char* title;
    const char* fullTitle;
    const char* gettingStarted;
};

struct Command {
    const char* name;
    int arity;
    Beast beast;
    const char* purpose;
    const char* values;
    const char* returns;
};

static constexpr const char* kIntroText =
    "Bestiary is a language, command-line interface, and GUI application through which a collection of C-based animal-themed mathematics libraries are accessed and used together seamlessly, as though they were all one program. Objects are defined by expressions like `A = { 1, 2, 3 }` and can be used with operations such as `A \\otimes A`. The language used in Bestiary is themed after LaTeX, and many objects can be defined in a way identical to how they would be typed in LaTeX.\n\n"
    "All commands in Bestiary start with a backslash `\\` character, like LaTeX. There are hundreds of commands, and while they can all be used together, they are provided by different BEASTs (Bundles of Efficient Algorithms for Science and Technology) and are used for different purposes. You should start by learning the commands for the application you need most.\n\n"
    "Each of the linked help pages below provides the help information for every command associated with a specific area of Bestiary, and describes the objects it provides and how to define and manipulate them. These pages can be searched with Ctrl+F. It is recommended to read the Basic features and commands page first, then HEBI, as HEBI is the most general BEAST and is used by all the others.\n\n"
    "To get started, click one of the links below to open its help page. For more information or to submit issues or feature requests, visit the git page.";

static constexpr Section kSections[] = {
    {
        Beast::Basic,
        "Basic features and commands",
        "Basic features and commands",
        "* Creating a new tab: use Ctrl+T or the \"+\" button on the tab bar to create a new tab containing a Bestiary shell.\n\n"
        "* Navigating tabs: Use Ctrl+1 to move to the start page, Ctrl+2 through Ctrl+9 to move to later tabs, and Ctrl+D to close the current tab.\n\n"
        "* Zoom: Use Ctrl+- and Ctrl++ to zoom out and in to a terminal or help page. Bestiary remembers your zoom level and shares it across tabs of the same type! In graphs, use the scroll wheel or touchpad to zoom.\n\n"
        "* Graphing: Use the `\\graph{function}` command to graph a function in a new Graph tab, `\\graph{function}{n}` to add a function to the nth Graph tab, or `\\graph{x=c}` to graph a vertical line at a real constant c. Expressions involving y, such as `\\graph{x*y}`, are graphed implicitly as `x*y = 0`, and equations such as `\\graph{x^2 + y^2 = 1}` are shifted internally to graph the zero set. The graph command treats x and y as graph axes even if variables with those names exist in the shell.\n\n"
        "* Persistence: Bestiary remembers what tabs you have open when you close it, and restores them for you automatically. Use `\\export{\"filename.bsy\"}` to create a script that restores your session, so you can share your work with others!\n\n"
        "* Loops: Bestiary supports for and while loops via the backslash commands `\\for{i = start; i < end; inc (e.g. i += 1)}{stuff}` and `\\while{condition}{stuff}`.\n\n"
        "* Conditionals: Bestiary supports conditions via the command `\\if{condition}{stuff}{else this stuff}`.\n\n"
        "* User-defined functions: You can define your own function with: `\\def{funcName}{input}{stuff...optional \\return{output}}`. Shift+Enter produces a new line while inside curly braces, like in other languages with support for conditionals and functions. After defining them, your functions may be called with `\\funcName{input}`.\n\n"
        "* Mutable lists: Define a list with `list = \\list{item1, item2, ...}`, append elements with `\\append{list}{element}`, remove a matching element with `\\remove{list}{element}`, and conveniently access list elements with `list[index]`."
    },
    {
        Beast::Hebi,
        "HEBI (basic math and common libraries)",
        "HEBI: Highly Efficient Basic Math Interpreter",
        "* define scalars inline: `x = 2 + 3 / 4` and `y = (\\phi^2 - \\pi) / e`\n"
        "* define an exact fraction: `q = \\frac{3,4}`\n"
        "* define a complex scalar inline: `z = 1 + 2i`\n"
        "* construct fields: `F = \\QQ`, `R = \\RR`, `C = \\CC`, `\\mathbb{Q}`, and `K = \\GF{5}`\n"
        "* construct field elements: `a = \\fieldElement{F}{\\frac{1}{2}}`\n"
        "* use powers and products inline: `w = (1 + i)^3 / 2`\n"
        "* update variables in place: `x += 1`, `A *= B`, and `n %= 3`\n"
        "* inspect scalar parts: `\\re{z}`, `\\im{z}`, and `\\conj{z}`\n"
        "* compare values inline: `1 + 1 == 2`\n"
        "* generate random values: `\\randInt{1}{6}`, `\\randReal{0}{1}`, and `\\randComplexMod{0}{2}`\n"
        "* solve low-degree equations over the complex numbers: `\\solveQuadratic{1,-3,2}`"
    },
    {
        Beast::Sokko,
        "Sokko (linear algebra)",
        "Sokko: Speedy Kitsune",
        "* define vectors inline: `u = [1,2,3]`, `v = [3,2,1]`, and `b = [1,0]`\n"
        "* define a matrix: `A = [1,2;3,4]`\n"
        "* multiply inline: `A v` and `u \\cdot v`\n"
        "* use matrix superscripts inline: `A^T` and `A^{-1}`\n"
        "* build tensor products inline: `A \\otimes A`\n"
        "* solve a linear system: `\\solveLinEq{A,b}`\n"
        "* inspect eigen-data: `\\eigenvalues{A}` and `\\eigenvectors{A}`"
    },
    {
        Beast::Neko,
        "NEKO (calculus, numerical analysis, DE solvers)",
        "NEKO: Numerical Estimation Kernel with Optimizations",
        "* define a symbolic expression inline: `f = \\sin{x} + x^2`\n"
        "* differentiate inline: `f'` and `f''`\n"
        "* use implicit multiplication inline: `g = x y + y^2`\n"
        "* take a partial derivative: `\\partialDerivative{g}{y}`\n"
        "* compute a gradient or Hessian: `\\gradient{x^2 + y^2}` and `\\hessian{x^2 + y^2}`\n"
        "* evaluate by substitution: `f(3)`\n"
        "* find roots or solve ODEs: `\\roots{x^2 - 4}` and `\\solveODE{\"y' = 2x\"}`"
    },
    {
        Beast::Poni,
        "PONI (physics)",
        "PONI: Physics Operations and Numerical Interpreter",
        "* define vectors inline: `x0 = [0,0]` and `v0 = [2,0]`\n"
        "* define a force: `F = \\force{\"gravity\",[0,-9.8],[0,0]}`\n"
        "* define a body: `b = \\body{5,x0,v0}`\n"
        "* attach model forces: `\\addForce{b}{\\dragForce{b,0.2}}` or `\\addForce{b}{\\springForce{b,[0,0],3,1}}`\n"
        "* define a body system: `sys = \\bodySystem{b1,b2}`\n"
        "* step or simulate it: `\\step{sys,0.1}` and `\\simulate{sys,0.1,100}`\n"
        "* inspect totals: `\\totalMomentum{sys}` and `\\totalEnergy{sys}`\n"
        "* compute projectile or work data: `\\projectileInfo{20,45,0}` and `\\work{F,[5,0]}`"
    },
    {
        Beast::Usagi,
        "USAGI (group and ring theory)",
        "USAGI: Utility Suite for Algebra and Group Invariants",
        "* define a group and ring: `G = \\ZnGroup{6}` and `R = \\ZnRing{12}`\n"
        "* define a finite field ring: `F = \\FFRing{2}{3}` or `F = \\FFRing{2^3}`\n"
        "* define elements: `g = \\getElement{G,\"2\"}` and `a = \\getElement{R,\"4\"}`\n"
        "* define a subgroup: `H = \\subgroupGeneratedBy{G,g}`\n"
        "* define a subring and ideals: `S = \\subring{a}`, `I = \\leftIdeal{a}`, and `J = \\rightIdeal{a}`\n"
        "* form quotient structures inline: `G / \\groupCenter{G}` and `R / I`\n"
        "* compute commutators and associators: `\\commutator{g}{h}` and `\\associator{a}{b}{c}`\n"
        "* test zero divisors: `\\isLeftZeroDivisor{a}` and `\\isRightZeroDivisor{a}`\n"
        "* define homomorphisms: `phi = \\groupHomomorphism{G,H,[g0:h0,g1:h1,...]}` and `psi = \\ringHomomorphism{R,S,[a0:b0,a1:b1,...]}`\n"
        "* inspect structure info: `\\groupInfo{G}`, `\\subgroupInfo{H}`, `\\ringInfo{R}`, and `\\idealInfo{I}`"
    },
    {
        Beast::Tora,
        "TORA (representation theory of finite groups)",
        "TORA: Toolkit Of Representation-theoretic Algorithms",
        "* define a representation: `rho = \\regularRep{\\Sn{3}}`\n"
        "* define more representations: `\\trivialRep{\\ZnGroup{4}}`, `\\signRep{\\Sn{4}}`, and `\\standardRep{\\Sn{4}}`\n"
        "* define a character and a character table: `chi = \\getIrrep{\\Sn{4},1}` and `T = \\charTable{\\Sn{4}}`\n"
        "* use inner products inline: `<chi, chi>`\n"
        "* inspect representations: `\\repDegree{rho}` and `\\isIrrep{rho}`\n"
        "* build related objects: `\\dualRep{rho}`, `\\tensorRep{rho,\\dualRep{rho}}`, and `pi = \\projectToAbelianization{\\Sn{3}}`\n"
        "* print or decompose: `\\printCharTable{T}` and `\\decomposeRep{rho}`"
    },
    {
        Beast::Ookami,
        "OOKAMI (additive and multiplicative combinatorics)",
        "OOKAMI: Operations Over K-fold Additive and Multiplicative Integer Sets",
        "* define finite integer sets inline: `A = {1,2,3}` and `B = {0,2,4}`\n"
        "* define constructor-based sets: `P = \\AP{2,3,5}`, `G = \\GP{2,3,4}`, and `R = \\rangeSet{1,10,2}`\n"
        "* use set operations inline: `A + B`, `A - B`, and `A * B`\n"
        "* translate or dilate inline: `A + 10` and `A * 2`\n"
        "* build k-fold sets inline: `k * A` gives A + A + ... + A, while `A^k` gives A * A * ... * A\n"
        "* compute subset sums: `\\subsetSums{A}` and `\\subsetSums{A,2}`\n"
        "* inspect structure: `\\isAP{P}`, `\\isGP{G}`, and `\\ddsCard{A}`\n"
        "* measure additive behavior: `\\energyAdd{A}` and `\\ruzsaDistance{A,B}`"
    },
    {
        Beast::Kuma,
        "KUMA (statistics and probability)",
        "KUMA: Kernel for Uncertainty Measurement and Analysis",
        "* compute combinatorics: `\\factorial{n}`, `\\ncr{n}{r}`, `\\npr{n}{r}`, and `\\multinomial{n}{parts}`\n"
        "* summarize data lists: `\\mean{data}`, `\\median{data}`, `\\variance{data}`, and `\\stddev{data}`\n"
        "* measure frequencies: `\\frequency{data}{x}`, `\\countDistinct{data}`, and `\\frequencies{data}`\n"
        "* compare paired data: `\\covariance{x}{y}`, `\\correlation{x}{y}`, and `\\linearRegressionSlope{x}{y}`\n"
        "* construct distributions: `\\bernoulli{p}`, `\\normal{mu}{sigma}`, and `\\customDiscrete{values}{probabilities}`\n"
        "* evaluate probabilities: `\\pmf{dist}{x}`, `\\pdf{dist}{x}`, `\\cdf{dist}{x}`, and `\\sample{dist}`\n"
        "* build graphable continuous functions: `\\pdfFunc{dist}` and `\\cdfFunc{dist}`\n"
        "* define random variables: `X = \\rv{X}{dist}` and use `\\rvExpectedValue{X}` or `\\rvAffine{X}{a}{b}`"
    },
    {
        Beast::Quaternionic,
        "Quaternionic (Cayley-Dickson algebras)",
        "Quaternionic: Cayley-Dickson, Quaternion, and Octonion Arithmetic",
        "* construct standard algebras: `H = \\HH` and `O = \\OO`\n"
        "* construct Cayley-Dickson algebras: `A = \\cdAlgebra{\\RR}{-1,-1}`\n"
        "* construct quaternions: `H = \\quaternionAlgebra{\\RR}{-1}{-1}` and `q = \\quaternion{H}{1}{2}{3}{4}`\n"
        "* use Hamilton defaults inline: `1 + j`, `1 + i + j`, and `1 + i*j`\n"
        "* construct octonions: `O = \\octonionAlgebra{\\RR}{-1}{-1}{-1}` and `x = \\octonion{O}{1,0,0,0,0,0,0,0}`\n"
        "* use ordinary operations: `x + y`, `x - y`, `x * y`, `x / y`, and `x^{-1}`\n"
        "* compute the Cayley-Dickson norm with `|x|`, `||x||`, or `\\cdNorm{x}`\n"
        "* build generated objects: `\\cdLeftIdeal{x}`, `\\cdRightIdeal{x}`, `\\cdTwoSidedIdeal{x}`, and `\\cdSubalgebra{x,y}`\n"
        "* test membership and intersections: `x \\in I`, `I \\cap J`, and `\\intersect{S}{T}`\n"
        "* use ideal arithmetic: `I + J`, `I - J`, `x * I`, `I * x`, and `2 * I`\n"
        "* inspect multiplication operators: `\\cdLeftMatrix{x}`, `\\cdRightMatrix{x}`, `\\cdCommutatorMatrix{x}`, and `\\cdAssociatorMatrix{x}{y}`\n"
        "* use vector and representation helpers: `\\cdToVector{x}`, `\\cdSpanBasis{x,y}`, and `\\quaternionToMatrix{x}`\n"
        "* compute structure data: `\\center{A}`, `\\nucleus{A}`, `\\cdLeftAnnihilator{x}`, and `\\cdRightAnnihilator{x}`"
    },
};

#define HELP_CMD(name, arity, beast, purpose, values, returns) { name, arity, beast, purpose, values, returns }

static constexpr Command kCommands[] = {
    HELP_CMD("help", -1, Beast::Basic, "Displays command usage, accepted value types, and return type.", "zero arguments to list commands, or one command name as a Symbol or String", "String"),
    HELP_CMD("run", 1, Beast::Basic, "Runs a text file as a Bestiary script, evaluating each nonblank line in the current context.", "String filename, or an unquoted filename in braces such as \\run{script.bsy}", "String summary, or Error if the file cannot be opened"),
    HELP_CMD("list", -1, Beast::Basic, "Constructs a dynamic Bestiary list.", "zero or more values", "List"),
    HELP_CMD("copy", 1, Beast::Basic, "Creates an independently owned copy of a supported value.", "scalar, String, Symbol, List, Matrix, Vector, CombSet, Field, FieldElement, CD algebra, CD element, NEKO expression, KUMA distribution, KUMA random variable, or supported TORA value", "same kind as input"),
    HELP_CMD("sort", 1, Beast::Basic, "Sorts a List of numeric values in ascending order.", "List containing only Int, Fraction, and Decimal values", "List"),
    HELP_CMD("sortedCopy", 1, Beast::Basic, "Creates a sorted copy of a numeric List without mutating the original.", "List containing only Int, Fraction, and Decimal values", "List"),
    HELP_CMD("shuffle", 1, Beast::Basic, "Randomly shuffles a List in place using HEBI's PRG.", "List", "List"),
    HELP_CMD("randInt", 2, Beast::Hebi, "Generates a random integer in an inclusive range using HEBI's PRG.", "Int lower and Int upper", "Int"),
    HELP_CMD("randFrac", 4, Beast::Hebi, "Generates a random fraction with numerator and denominator in inclusive integer ranges using HEBI's PRG.", "Int numLower, Int numUpper, Int denomLower, Int denomUpper", "Fraction"),
    HELP_CMD("randReal", 2, Beast::Hebi, "Generates a random real number in an inclusive range using HEBI's PRG.", "real numeric lower and upper", "Decimal"),
    HELP_CMD("randComplexComp", 4, Beast::Hebi, "Generates a random complex number from inclusive real and imaginary ranges using HEBI's PRG.", "real lower, real upper, imaginary lower, imaginary upper", "Complex"),
    HELP_CMD("randComplexMod", 2, Beast::Hebi, "Generates a random complex number whose modulus lies in an inclusive range using HEBI's PRG.", "nonnegative real modulus lower and upper", "Complex"),
    HELP_CMD("solveQuadratic", -1, Beast::Hebi, "Solves a quadratic equation over the complex numbers.", "polynomial in x, List of coefficients, or coefficients a,b,c for a*x^2+b*x+c", "List of Decimal or Complex roots"),
    HELP_CMD("solveCubic", -1, Beast::Hebi, "Solves a cubic equation over the complex numbers.", "polynomial in x, List of coefficients, or coefficients a,b,c,d for a*x^3+b*x^2+c*x+d", "List of Decimal or Complex roots"),
    HELP_CMD("solveQuartic", -1, Beast::Hebi, "Solves a quartic equation over the complex numbers.", "polynomial in x, List of coefficients, or coefficients a,b,c,d,e for a*x^4+b*x^3+c*x^2+d*x+e", "List of Decimal or Complex roots"),
    HELP_CMD("if", -1, Beast::Basic, "Evaluates the result branch when a condition is true, otherwise evaluates the optional else branch.", "Bool or truthy condition, result expression, and optional else expression", "selected branch value or none"),
    HELP_CMD("while", 2, Beast::Basic, "Evaluates a body repeatedly while a condition remains true.", "truthy condition expression and loop body", "last body value or none"),
    HELP_CMD("for", 2, Beast::Basic, "Evaluates an init, condition, and step header around a repeated body.", "header block of init; condition; step and loop body", "last body value or none"),
    HELP_CMD("break", 0, Beast::Basic, "Exits the nearest active loop.", "no values", "None"),
    HELP_CMD("continue", 0, Beast::Basic, "Skips the rest of the current loop body.", "no values", "None"),
    HELP_CMD("def", 3, Beast::Basic, "Defines a user function with local-only variables.", "function name, parameter list, and body", "None"),
    HELP_CMD("return", 1, Beast::Basic, "Ends the current user function and returns a value.", "single value", "the returned value"),
    HELP_CMD("print", 1, Beast::Basic, "Prints a value to stdout, using raw text and $name interpolation for strings.", "any single value", "None"),
    HELP_CMD("+", 2, Beast::Hebi, "Adds compatible values.", "Int/Fraction/Decimal/Complex with numeric; FieldElement with compatible scalar or FieldElement; CD element with CD element from the same algebra; CD ideal with CD ideal from the same algebra; Matrix with same-size Matrix; Vector with same-dimension Vector; CombSet with Int for translation; CombSet with CombSet for sumset; RingElement with RingElement from the same Ring; Ideal with Ideal from the same Ring and side; Symbol/NEKO expression/numeric for symbolic addition", "same family as the operands, or numeric/symbolic result"),
    HELP_CMD("-", 2, Beast::Hebi, "Subtracts compatible values.", "Int/Fraction/Decimal/Complex with numeric; FieldElement with compatible scalar or FieldElement; CD element with CD element from the same algebra; CD ideal with CD ideal from the same algebra; Matrix with same-size Matrix; Vector with same-dimension Vector; CombSet with CombSet for difference set; RingElement with RingElement from the same Ring; Symbol/NEKO expression/numeric for symbolic subtraction", "same family as the operands, or numeric/symbolic result"),
    HELP_CMD("*", 2, Beast::Hebi, "Multiplies compatible values.", "numeric with numeric; FieldElement with compatible scalar or FieldElement; CD element with CD element from the same algebra; CD ideal with compatible scalar or CD element on either side; Matrix with compatible Matrix; Matrix with compatible Vector; Vector with Vector for dot product; Vector/Matrix with numeric scalar; CombSet with CombSet for product set; CombSet with Int for dilation; Int with CombSet for repeated sum/difference set; GroupElement with GroupElement from the same Group; RingElement with RingElement from the same Ring; RingElement with Int; Ideal with Ideal from the same Ring and side; Symbol/NEKO expression/numeric for symbolic multiplication", "same family as the operation, numeric scalar for dot products, or symbolic expression"),
    HELP_CMD("/", 2, Beast::Hebi, "Divides compatible values.", "numeric numerator and nonzero numeric denominator; FieldElement by compatible scalar or FieldElement; CD element by CD element as x*y^{-1}; Group by normal SubGroup for quotient group; Ring by Ideal for quotient ring; GroupElement by GroupElement from the same Group; RingElement by invertible RingElement from the same Ring; Symbol/NEKO expression/numeric for symbolic quotient", "numeric, FieldElement, CD element, Group, Ring, GroupElement, RingElement, or NEKO expression"),
    HELP_CMD("%", 2, Beast::Hebi, "Computes integer remainder.", "two Int values with nonzero divisor", "Int"),
    HELP_CMD("==", 2, Beast::Hebi, "Tests two Bestiary values for equality.", "two values of comparable Bestiary kinds", "Bool"),
    HELP_CMD("in", 2, Beast::Quaternionic, "Tests whether a CD element lies in a CD ideal or subalgebra.", "CD element and CD ideal or CD subalgebra", "Bool"),
    HELP_CMD("<", 2, Beast::Hebi, "Tests whether one real numeric value is less than another.", "two real numeric values", "Bool"),
    HELP_CMD(">", 2, Beast::Hebi, "Tests whether one real numeric value is greater than another.", "two real numeric values", "Bool"),
    HELP_CMD("u-", 1, Beast::Hebi, "Negates one value.", "numeric value, FieldElement, CD element, Vector, RingElement, CombSet, Symbol, or NEKO expression", "same kind as the input, or NEKO expression"),
    HELP_CMD("u+", 1, Beast::Hebi, "Returns one value unchanged.", "any single Bestiary value", "same value kind as the input"),
    HELP_CMD("^", 2, Beast::Hebi, "Raises a supported base to a power or applies a matrix superscript.", "numeric base with numeric exponent; FieldElement or CD element with exponent -1; Matrix with Int exponent or Symbol T/t; CombSet with positive Int exponent; GroupElement with Int exponent; RingElement with Int exponent; Symbol/NEKO expression/numeric for symbolic power", "numeric, FieldElement, CD element, Matrix, CombSet, GroupElement, RingElement, or NEKO expression"),
    HELP_CMD("pi", 0, Beast::Hebi, "Returns the mathematical constant pi.", "no values", "Decimal"),
    HELP_CMD("e", 0, Beast::Hebi, "Returns Euler's number.", "no values", "Decimal"),
    HELP_CMD("phi", 0, Beast::Hebi, "Returns the golden ratio.", "no values", "Decimal"),
    HELP_CMD("frac", 2, Beast::Hebi, "Constructs an exact rational fraction.", "Int numerator and Int denominator", "Fraction, or Int when normalized elsewhere"),
    HELP_CMD("QQ", 0, Beast::Hebi, "Constructs the rational field.", "no values", "Field"),
    HELP_CMD("RR", 0, Beast::Hebi, "Constructs the real field.", "no values", "Field"),
    HELP_CMD("CC", 0, Beast::Hebi, "Constructs the complex field.", "no values", "Field"),
    HELP_CMD("HH", 0, Beast::Quaternionic, "Constructs the standard Hamilton quaternion algebra over RR.", "no values", "CD algebra"),
    HELP_CMD("OO", 0, Beast::Quaternionic, "Constructs the standard octonion algebra over RR.", "no values", "CD algebra"),
    HELP_CMD("mathbb", 1, Beast::Hebi, "Maps LaTeX blackboard-bold names Q, R, C, H, and O to their Bestiary commands.", "Symbol or String Q, R, C, H, or O", "Field or CD algebra"),
    HELP_CMD("GF", 1, Beast::Hebi, "Constructs a prime finite field.", "prime Int p", "Field"),
    HELP_CMD("fieldElement", 2, Beast::Hebi, "Constructs an element of a field.", "Field and compatible scalar", "FieldElement"),
    HELP_CMD("zero", 1, Beast::Hebi, "Returns the additive identity of a field.", "Field", "FieldElement"),
    HELP_CMD("one", 1, Beast::Hebi, "Returns the multiplicative identity of a field.", "Field", "FieldElement"),
    HELP_CMD("quadraticExtension", 2, Beast::Hebi, "Constructs a quadratic field extension.", "Field and compatible radicand", "Field"),
    HELP_CMD("quadExt", 2, Beast::Hebi, "Alias for quadraticExtension.", "Field and compatible radicand", "Field"),
    HELP_CMD("cdAlgebra", -1, Beast::Quaternionic, "Constructs a Cayley-Dickson algebra over a field.", "Field followed by one or more doubling parameters", "CD algebra"),
    HELP_CMD("cdElement", -1, Beast::Quaternionic, "Constructs a Cayley-Dickson element.", "CD algebra followed by dim(A) coefficients", "CD element"),
    HELP_CMD("quaternionAlgebra", 3, Beast::Quaternionic, "Constructs a quaternion algebra.", "Field and two doubling parameters", "CD algebra"),
    HELP_CMD("quaternion", -1, Beast::Quaternionic, "Constructs a quaternion element.", "Quaternion algebra and four coefficients", "CD element"),
    HELP_CMD("octonionAlgebra", 4, Beast::Quaternionic, "Constructs an octonion algebra.", "Field and three doubling parameters", "CD algebra"),
    HELP_CMD("octonion", -1, Beast::Quaternionic, "Constructs an octonion element.", "Octonion algebra and eight coefficients", "CD element"),
    HELP_CMD("cdAdd", 2, Beast::Quaternionic, "Adds two Cayley-Dickson elements.", "two CD elements from the same algebra", "CD element"),
    HELP_CMD("cdSub", 2, Beast::Quaternionic, "Subtracts two Cayley-Dickson elements.", "two CD elements from the same algebra", "CD element"),
    HELP_CMD("cdMul", 2, Beast::Quaternionic, "Multiplies two Cayley-Dickson elements.", "two CD elements from the same algebra", "CD element"),
    HELP_CMD("cdConj", 1, Beast::Quaternionic, "Computes Cayley-Dickson conjugation.", "CD element", "CD element"),
    HELP_CMD("cdNorm", 1, Beast::Quaternionic, "Computes the Cayley-Dickson norm.", "CD element", "FieldElement"),
    HELP_CMD("cdInv", 1, Beast::Quaternionic, "Computes the Cayley-Dickson inverse.", "invertible CD element", "CD element"),
    HELP_CMD("cdCommutator", 2, Beast::Quaternionic, "Computes xy - yx.", "two CD elements from the same algebra", "CD element"),
    HELP_CMD("cdAssociator", 3, Beast::Quaternionic, "Computes (xy)z - x(yz).", "three CD elements from the same algebra", "CD element"),
    HELP_CMD("cdLeftDivide", 3, Beast::Quaternionic, "Computes y^{-1}x in a Cayley-Dickson algebra.", "CD algebra A and two elements x, y of A", "CD element"),
    HELP_CMD("cdRightDivide", 3, Beast::Quaternionic, "Computes xy^{-1} in a Cayley-Dickson algebra.", "CD algebra A and two elements x, y of A", "CD element"),
    HELP_CMD("cdLeftMatrix", 1, Beast::Quaternionic, "Computes the left multiplication matrix of a CD element.", "CD element", "Matrix"),
    HELP_CMD("cdRightMatrix", 1, Beast::Quaternionic, "Computes the right multiplication matrix of a CD element.", "CD element", "Matrix"),
    HELP_CMD("cdToVector", 1, Beast::Quaternionic, "Converts a CD element to its coefficient vector.", "CD element", "Vector"),
    HELP_CMD("cdFromVector", 2, Beast::Quaternionic, "Converts a coefficient vector to a CD element.", "CD algebra and Vector", "CD element"),
    HELP_CMD("cdSpanBasis", -1, Beast::Quaternionic, "Computes a basis for the span of CD elements.", "one or more CD elements from the same algebra, or a List of them", "List of CD elements"),
    HELP_CMD("cdInSpan", -1, Beast::Quaternionic, "Tests whether a CD element lies in the span of generators.", "CD element followed by span generators or a List of them", "Bool"),
    HELP_CMD("cdBasisElement", 2, Beast::Quaternionic, "Returns a standard basis element by zero-based index.", "CD algebra and nonnegative Int index", "CD element"),
    HELP_CMD("cdStandardBasis", 1, Beast::Quaternionic, "Returns the standard basis of a CD algebra.", "CD algebra", "List of CD elements"),
    HELP_CMD("quaternionMatrixRep", 1, Beast::Quaternionic, "Constructs the standard 2x2 matrix representation data for a quaternion algebra.", "Quaternion algebra", "Quaternion matrix representation"),
    HELP_CMD("quaternionToMatrix", -1, Beast::Quaternionic, "Converts a quaternion to its standard 2x2 matrix representation.", "Quaternion element and optional Quaternion matrix representation", "Matrix"),
    HELP_CMD("cdLeftIdeal", -1, Beast::Quaternionic, "Constructs the left ideal generated by CD elements.", "one or more CD elements from the same algebra, or a List of them", "CD ideal"),
    HELP_CMD("cdRightIdeal", -1, Beast::Quaternionic, "Constructs the right ideal generated by CD elements.", "one or more CD elements from the same algebra, or a List of them", "CD ideal"),
    HELP_CMD("cdTwoSidedIdeal", -1, Beast::Quaternionic, "Constructs the two-sided ideal generated by CD elements.", "one or more CD elements from the same algebra, or a List of them", "CD ideal"),
    HELP_CMD("cdSubalgebra", -1, Beast::Quaternionic, "Constructs the unital subalgebra generated by CD elements.", "one or more CD elements from the same algebra, or a List of them", "CD subalgebra"),
    HELP_CMD("cdCommutatorMatrix", 1, Beast::Quaternionic, "Computes the matrix of y -> xy - yx.", "CD element", "Matrix"),
    HELP_CMD("cdAssociatorMatrix", 2, Beast::Quaternionic, "Computes the matrix of z -> (xy)z - x(yz).", "two CD elements", "Matrix"),
    HELP_CMD("cdLeftAnnihilator", 1, Beast::Quaternionic, "Computes the left annihilator of a CD element.", "CD element", "CD ideal"),
    HELP_CMD("cdRightAnnihilator", 1, Beast::Quaternionic, "Computes the right annihilator of a CD element.", "CD element", "CD ideal"),
    HELP_CMD("center", 1, Beast::Quaternionic, "Computes the center of a CD algebra.", "CD algebra", "CD subalgebra"),
    HELP_CMD("nucleus", 1, Beast::Quaternionic, "Computes the nucleus of a CD algebra.", "CD algebra", "CD subalgebra"),
    HELP_CMD("commutator", 2, Beast::Quaternionic, "Computes the commutator for CD, group, or ring elements.", "two compatible CD, GroupElement, or RingElement values", "same algebraic element family"),
    HELP_CMD("associator", 3, Beast::Quaternionic, "Computes the associator for CD, group, or ring elements.", "three compatible CD, GroupElement, or RingElement values", "same algebraic element family"),
    HELP_CMD("isLeftZeroDivisor", 1, Beast::Quaternionic, "Tests whether a CD or ring element is a left zero divisor.", "CD element or RingElement", "Bool"),
    HELP_CMD("isRightZeroDivisor", 1, Beast::Quaternionic, "Tests whether a CD or ring element is a right zero divisor.", "CD element or RingElement", "Bool"),
    HELP_CMD("sqrt", 1, Beast::Hebi, "Computes or constructs a principal square root.", "Int, Fraction, Decimal, Complex, Symbol, or NEKO expression", "Int/Fraction when exact, Decimal or Complex when numeric, or NEKO expression"),
    HELP_CMD("cbrt", 1, Beast::Hebi, "Computes a principal cube root.", "Int, Fraction, Decimal, or Complex", "Decimal or Complex"),
    HELP_CMD("conj", 1, Beast::Hebi, "Computes complex or Cayley-Dickson conjugation.", "Int, Fraction, Decimal, Complex, or CD element", "same scalar family, Complex, or CD element"),
    HELP_CMD("abs", 1, Beast::Hebi, "Computes an absolute value, CD norm, or constructs abs(x).", "Int, Fraction, Decimal, Complex, CD element, Symbol, or NEKO expression", "Int/Fraction for exact real input, Decimal for numeric magnitude, FieldElement for CD input, or NEKO expression"),
    HELP_CMD("arg", 1, Beast::Hebi, "Computes the principal complex argument.", "Int, Fraction, Decimal, or Complex", "Decimal"),
    HELP_CMD("re", 1, Beast::Hebi, "Extracts the real part of a scalar.", "Int, Fraction, Decimal, or Complex", "same scalar family for real input, Decimal for Complex input"),
    HELP_CMD("im", 1, Beast::Hebi, "Extracts the imaginary part of a scalar.", "Int, Fraction, Decimal, or Complex", "Int 0 for real input, Decimal for Complex input"),
    HELP_CMD("isPrime", 1, Beast::Hebi, "Tests whether an integer is prime.", "Int", "Bool"),

    HELP_CMD("factorial", 1, Beast::Kuma, "Computes n factorial.", "Int n >= 0", "Int"),
    HELP_CMD("ncr", 2, Beast::Kuma, "Computes the binomial coefficient n choose r.", "Int n and Int r with 0 <= r <= n", "Int"),
    HELP_CMD("npr", 2, Beast::Kuma, "Computes the number of ordered r-permutations of n objects.", "Int n and Int r with 0 <= r <= n", "Int"),
    HELP_CMD("multinomial", -1, Beast::Kuma, "Computes a multinomial coefficient.", "Int n and List of nonnegative Int parts summing to n, or Int n followed by parts", "Int"),
    HELP_CMD("sum", 1, Beast::Kuma, "Computes the sum of numeric data.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("product", 1, Beast::Kuma, "Computes the product of numeric data.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("mean", 1, Beast::Kuma, "Computes the arithmetic mean of numeric data or the expected value of a distribution.", "nonempty List of Int, Fraction, or Decimal values, ProbabilityDistribution, or RandomVariable", "Int, Fraction, or Decimal"),
    HELP_CMD("median", 1, Beast::Kuma, "Computes the median of numeric data.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("mode", 1, Beast::Kuma, "Computes the smallest mode of numeric data.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("modes", 1, Beast::Kuma, "Returns every modal value of numeric data.", "nonempty List of Int, Fraction, or Decimal values", "List"),
    HELP_CMD("min", 1, Beast::Kuma, "Computes the minimum of numeric data.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("max", 1, Beast::Kuma, "Computes the maximum of numeric data.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("range", 1, Beast::Kuma, "Computes max(data) - min(data).", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("variance", 1, Beast::Kuma, "Computes population variance or distribution variance.", "nonempty List of Int, Fraction, or Decimal values, ProbabilityDistribution, or RandomVariable", "Int, Fraction, or Decimal"),
    HELP_CMD("sampleVariance", 1, Beast::Kuma, "Computes sample variance with denominator n - 1.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("stddev", 1, Beast::Kuma, "Computes population standard deviation or distribution standard deviation.", "nonempty List of Int, Fraction, or Decimal values, ProbabilityDistribution, or RandomVariable", "Decimal"),
    HELP_CMD("sampleStddev", 1, Beast::Kuma, "Computes sample standard deviation with denominator n - 1.", "nonempty List of Int, Fraction, or Decimal values", "Decimal"),
    HELP_CMD("meanAbsDev", 1, Beast::Kuma, "Computes mean absolute deviation from the mean.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("medianAbsDev", 1, Beast::Kuma, "Computes median absolute deviation from the median.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("percentile", 2, Beast::Kuma, "Computes a percentile using KUMA interpolation.", "nonempty numeric List and numeric percentile p from 0 to 100", "Int, Fraction, or Decimal"),
    HELP_CMD("quartile", 2, Beast::Kuma, "Computes quartile q as the 25qth percentile.", "nonempty numeric List and Int q in {0,1,2,3,4}", "Int, Fraction, or Decimal"),
    HELP_CMD("iqr", 1, Beast::Kuma, "Computes the interquartile range Q3 - Q1.", "nonempty List of Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("geometricMean", 1, Beast::Kuma, "Computes the geometric mean of positive numeric data.", "nonempty List of positive Int, Fraction, or Decimal values", "Decimal"),
    HELP_CMD("harmonicMean", 1, Beast::Kuma, "Computes the harmonic mean of nonzero numeric data.", "nonempty List of nonzero Int, Fraction, or Decimal values", "Int, Fraction, or Decimal"),
    HELP_CMD("frequency", 2, Beast::Kuma, "Counts how often a numeric value occurs in numeric data.", "nonempty numeric List and numeric value", "Int"),
    HELP_CMD("countDistinct", 1, Beast::Kuma, "Counts distinct values in numeric data.", "nonempty List of Int, Fraction, or Decimal values", "Int"),
    HELP_CMD("frequencies", 1, Beast::Kuma, "Returns sorted value-count pairs for numeric data.", "nonempty List of Int, Fraction, or Decimal values", "List of [value, count] pairs"),
    HELP_CMD("covariance", 2, Beast::Kuma, "Computes population covariance of paired numeric data.", "two same-length nonempty numeric Lists", "Int, Fraction, or Decimal"),
    HELP_CMD("sampleCovariance", 2, Beast::Kuma, "Computes sample covariance with denominator n - 1.", "two same-length nonempty numeric Lists", "Int, Fraction, or Decimal"),
    HELP_CMD("correlation", 2, Beast::Kuma, "Computes Pearson correlation of paired numeric data.", "two same-length nonempty numeric Lists", "Decimal"),
    HELP_CMD("linearRegressionSlope", 2, Beast::Kuma, "Computes the least-squares regression slope for y on x.", "two same-length nonempty numeric Lists", "Int, Fraction, or Decimal"),
    HELP_CMD("linearRegressionIntercept", 2, Beast::Kuma, "Computes the least-squares regression intercept for y on x.", "two same-length nonempty numeric Lists", "Int, Fraction, or Decimal"),
    HELP_CMD("linearRegressionPredict", 3, Beast::Kuma, "Predicts y from slope, intercept, and x.", "numeric slope, numeric intercept, and numeric x", "Int, Fraction, or Decimal"),
    HELP_CMD("bernoulli", 1, Beast::Kuma, "Constructs a Bernoulli distribution.", "probability p", "ProbabilityDistribution"),
    HELP_CMD("binomial", 2, Beast::Kuma, "Constructs a binomial distribution.", "Int n >= 0 and probability p", "ProbabilityDistribution"),
    HELP_CMD("geometric", 1, Beast::Kuma, "Constructs a geometric distribution.", "probability p with 0 < p <= 1", "ProbabilityDistribution"),
    HELP_CMD("poisson", 1, Beast::Kuma, "Constructs a Poisson distribution.", "positive rate lambda", "ProbabilityDistribution"),
    HELP_CMD("discreteUniform", 2, Beast::Kuma, "Constructs a discrete uniform distribution.", "integer lower and upper bounds with lower <= upper", "ProbabilityDistribution"),
    HELP_CMD("continuousUniform", 2, Beast::Kuma, "Constructs a continuous uniform distribution.", "numeric lower and upper bounds with lower < upper", "ProbabilityDistribution"),
    HELP_CMD("normal", 2, Beast::Kuma, "Constructs a normal distribution.", "numeric mean mu and positive standard deviation sigma", "ProbabilityDistribution"),
    HELP_CMD("exponential", 1, Beast::Kuma, "Constructs an exponential distribution.", "positive rate lambda", "ProbabilityDistribution"),
    HELP_CMD("customDiscrete", 2, Beast::Kuma, "Constructs a finite custom discrete distribution.", "List of numeric values and same-length List of probabilities summing to 1", "ProbabilityDistribution"),
    HELP_CMD("affineDistribution", 3, Beast::Kuma, "Constructs the affine image scalar*X + shift of a distribution.", "ProbabilityDistribution, numeric scalar, and numeric shift", "ProbabilityDistribution"),
    HELP_CMD("sumIndependentDistributions", 2, Beast::Kuma, "Constructs the sum of independent distributions when supported.", "two compatible ProbabilityDistribution values", "ProbabilityDistribution"),
    HELP_CMD("productIndependentDistributions", 2, Beast::Kuma, "Constructs the product of independent distributions when supported.", "two compatible ProbabilityDistribution values", "ProbabilityDistribution"),
    HELP_CMD("pmf", 2, Beast::Kuma, "Evaluates the probability mass function of a distribution or random variable.", "ProbabilityDistribution or RandomVariable, then numeric x", "Int, Fraction, or Decimal"),
    HELP_CMD("pdf", 2, Beast::Kuma, "Evaluates the probability density function of a distribution or random variable.", "ProbabilityDistribution or RandomVariable, then numeric x", "Decimal"),
    HELP_CMD("cdf", 2, Beast::Kuma, "Evaluates the cumulative distribution function of a distribution or random variable.", "ProbabilityDistribution or RandomVariable, then numeric x", "Int, Fraction, or Decimal"),
    HELP_CMD("expectedValue", 1, Beast::Kuma, "Computes the expected value of a distribution or random variable.", "ProbabilityDistribution or RandomVariable", "Int, Fraction, or Decimal"),
    HELP_CMD("sample", 1, Beast::Kuma, "Samples from a distribution or random variable using KUMA's sampler.", "ProbabilityDistribution or RandomVariable", "Int, Fraction, or Decimal"),
    HELP_CMD("pdfFunc", 1, Beast::Kuma, "Builds a graphable NEKO expression for a supported PDF.", "supported continuous ProbabilityDistribution or RandomVariable", "NEKO expression"),
    HELP_CMD("cdfFunc", 1, Beast::Kuma, "Builds a graphable NEKO expression for a supported CDF.", "supported continuous ProbabilityDistribution or RandomVariable", "NEKO expression"),
    HELP_CMD("randomVariable", 2, Beast::Kuma, "Constructs a named random variable from a copied distribution.", "Symbol or String name and ProbabilityDistribution", "RandomVariable"),
    HELP_CMD("rv", 2, Beast::Kuma, "Alias for randomVariable.", "Symbol or String name and ProbabilityDistribution", "RandomVariable"),
    HELP_CMD("rvPMF", 2, Beast::Kuma, "Evaluates a random variable PMF.", "RandomVariable and numeric x", "Int, Fraction, or Decimal"),
    HELP_CMD("rvPDF", 2, Beast::Kuma, "Evaluates a random variable PDF.", "RandomVariable and numeric x", "Decimal"),
    HELP_CMD("rvCDF", 2, Beast::Kuma, "Evaluates a random variable CDF.", "RandomVariable and numeric x", "Int, Fraction, or Decimal"),
    HELP_CMD("rvExpectedValue", 1, Beast::Kuma, "Computes a random variable expected value.", "RandomVariable", "Int, Fraction, or Decimal"),
    HELP_CMD("rvVariance", 1, Beast::Kuma, "Computes a random variable variance.", "RandomVariable", "Int, Fraction, or Decimal"),
    HELP_CMD("rvStddev", 1, Beast::Kuma, "Computes a random variable standard deviation.", "RandomVariable", "Decimal"),
    HELP_CMD("rvSample", 1, Beast::Kuma, "Samples a random variable.", "RandomVariable", "Int, Fraction, or Decimal"),
    HELP_CMD("rvScale", 2, Beast::Kuma, "Constructs the scaled random variable scalar*X.", "RandomVariable and numeric scalar", "RandomVariable"),
    HELP_CMD("rvShift", 2, Beast::Kuma, "Constructs the shifted random variable X + shift.", "RandomVariable and numeric shift", "RandomVariable"),
    HELP_CMD("rvAffine", 3, Beast::Kuma, "Constructs the affine random variable scalar*X + shift.", "RandomVariable, numeric scalar, and numeric shift", "RandomVariable"),
    HELP_CMD("rvSumIndependent", 2, Beast::Kuma, "Constructs the sum of independent random variables when supported.", "two compatible RandomVariable values", "RandomVariable"),
    HELP_CMD("rvProductIndependent", 2, Beast::Kuma, "Constructs the product of independent random variables when supported.", "two compatible RandomVariable values", "RandomVariable"),

    HELP_CMD("iMatrix", 1, Beast::Sokko, "Constructs an identity matrix.", "positive Int dimension", "Matrix"),
    HELP_CMD("zeroMatrix", 1, Beast::Sokko, "Constructs a square zero matrix.", "positive Int dimension", "Matrix"),
    HELP_CMD("isSquare", 1, Beast::Sokko, "Tests whether a matrix is square.", "Matrix", "Bool"),
    HELP_CMD("isSymmetric", 1, Beast::Sokko, "Tests whether a matrix equals its transpose.", "Matrix", "Bool"),
    HELP_CMD("isAntisymmetric", 1, Beast::Sokko, "Tests whether a matrix equals the negative of its transpose.", "Matrix", "Bool"),
    HELP_CMD("isUnitary", 1, Beast::Sokko, "Tests whether a square matrix is unitary.", "Matrix", "Bool"),
    HELP_CMD("isOrthogonal", 1, Beast::Sokko, "Tests whether a square real matrix is orthogonal.", "Matrix", "Bool"),
    HELP_CMD("rank", 1, Beast::Sokko, "Computes matrix rank.", "Matrix", "Int"),
    HELP_CMD("nullity", 1, Beast::Sokko, "Computes matrix nullity.", "Matrix", "Int"),
    HELP_CMD("trace", 1, Beast::Sokko, "Computes the trace of a square matrix.", "square Matrix", "Int, Decimal, Fraction, or Complex scalar"),
    HELP_CMD("frobeniusNorm", 1, Beast::Sokko, "Computes the Frobenius norm of a matrix.", "Matrix", "Decimal"),
    HELP_CMD("det", 1, Beast::Sokko, "Computes the determinant of a square matrix.", "square Matrix", "Int, Decimal, Fraction, or Complex scalar"),
    HELP_CMD("transpose", 1, Beast::Sokko, "Computes the transpose of a matrix.", "Matrix", "Matrix"),
    HELP_CMD("adjoint", 1, Beast::Sokko, "Computes the conjugate transpose of a matrix.", "Matrix", "Matrix"),
    HELP_CMD("inverse", 1, Beast::Sokko, "Computes an inverse.", "invertible square Matrix, FieldElement, or CD element", "Matrix, FieldElement, or CD element"),
    HELP_CMD("rref", 1, Beast::Sokko, "Computes the row-reduced echelon form of a matrix.", "Matrix", "Matrix"),
    HELP_CMD("eigenvalues", 1, Beast::Sokko, "Computes eigenvalues of a supported square matrix.", "square Matrix", "List of numeric or Complex values"),
    HELP_CMD("eigenvectors", 1, Beast::Sokko, "Computes eigenvectors of a supported square matrix.", "square Matrix", "List of Vector values or none entries"),
    HELP_CMD("columnReduce", 1, Beast::Sokko, "Computes the column-reduced form of a matrix.", "Matrix", "Matrix"),
    HELP_CMD("rowSpace", 1, Beast::Sokko, "Computes a basis for the row space of a matrix.", "Matrix", "List of Vector values"),
    HELP_CMD("columnSpace", 1, Beast::Sokko, "Computes a basis for the column space of a matrix.", "Matrix", "List of Vector values"),
    HELP_CMD("solveLinEq", 2, Beast::Sokko, "Solves A*x=b.", "square Matrix A and compatible column Vector or Matrix b", "Vector when the solution has one column, otherwise Matrix"),
    HELP_CMD("l2Norm", 1, Beast::Sokko, "Computes vector Euclidean norm. Bar syntax |v| also computes this for Vector input.", "Vector", "Decimal"),
    HELP_CMD("normalize", 1, Beast::Sokko, "Normalizes a nonzero vector.", "Vector", "Vector"),
    HELP_CMD("vdist", -1, Beast::Sokko, "Computes Euclidean distance between two vectors.", "two same-dimension Vectors, or one List containing two Vectors", "Decimal"),
    HELP_CMD("vangle", -1, Beast::Sokko, "Computes the angle in radians between two nonzero vectors.", "two compatible nonzero Vectors, or one List containing two Vectors", "Decimal"),
    HELP_CMD("vproj", -1, Beast::Sokko, "Projects one vector onto another.", "two compatible Vectors, or one List containing two Vectors", "Vector"),
    HELP_CMD("cdot", 2, Beast::Sokko, "Computes the vector dot product.", "two same-dimension Vectors", "Int, Decimal, Fraction, or Complex scalar"),
    HELP_CMD("otimes", 2, Beast::Sokko, "Computes the tensor product of two matrices.", "Matrix and Matrix", "Matrix"),

    HELP_CMD("poly", 1, Beast::Neko, "Parses a polynomial in x into a NEKO expression.", "String such as \"x^2 + 3x - 1\"", "NEKO expression"),
    HELP_CMD("sin", -1, Beast::Neko, "Computes sine or constructs sin(x).", "zero values for sin(x), or one numeric/Complex/Symbol/NEKO expression", "Decimal or Complex for numeric input, NEKO expression for symbolic input"),
    HELP_CMD("cos", -1, Beast::Neko, "Computes cosine or constructs cos(x).", "zero values for cos(x), or one numeric/Complex/Symbol/NEKO expression", "Decimal or Complex for numeric input, NEKO expression for symbolic input"),
    HELP_CMD("tan", -1, Beast::Neko, "Computes tangent or constructs tan(x).", "zero values for tan(x), or one numeric/Complex/Symbol/NEKO expression", "Decimal or Complex for numeric input, NEKO expression for symbolic input"),
    HELP_CMD("asin", -1, Beast::Neko, "Computes inverse sine or constructs asin(x).", "zero values for asin(x), or one numeric/Complex/Symbol/NEKO expression", "Decimal or Complex for numeric input, NEKO expression for symbolic input"),
    HELP_CMD("acos", -1, Beast::Neko, "Computes inverse cosine or constructs acos(x).", "zero values for acos(x), or one numeric/Complex/Symbol/NEKO expression", "Decimal or Complex for numeric input, NEKO expression for symbolic input"),
    HELP_CMD("atan", -1, Beast::Neko, "Computes inverse tangent or constructs atan(x).", "zero values for atan(x), or one numeric/Complex/Symbol/NEKO expression", "Decimal or Complex for numeric input, NEKO expression for symbolic input"),
    HELP_CMD("exp", -1, Beast::Neko, "Computes e^x or constructs exp(x).", "zero values for exp(x), or one numeric/Complex/Symbol/NEKO expression", "Decimal or Complex for numeric input, NEKO expression for symbolic input"),
    HELP_CMD("log", -1, Beast::Neko, "Computes natural logarithm or constructs log(x).", "zero values for log(x), or one numeric/Complex/Symbol/NEKO expression", "Decimal or Complex for numeric input, NEKO expression for symbolic input"),
    HELP_CMD("derivative", -1, Beast::Neko, "Differentiates a NEKO expression with respect to x by default, or with respect to an explicit variable.", "Symbol, numeric value, or NEKO expression convertible to a NEKO expression, optionally followed by a Symbol or String variable name", "NEKO expression"),
    HELP_CMD("partialDerivative", 2, Beast::Neko, "Differentiates a NEKO expression with respect to an explicit variable.", "Symbol, numeric value, or NEKO expression convertible to a NEKO expression, then a Symbol or String variable name", "NEKO expression"),
    HELP_CMD("gradient", -1, Beast::Neko, "Computes the gradient of a scalar NEKO expression.", "Symbol, numeric value, or NEKO expression convertible to a NEKO expression, optionally followed by one Symbol/String variable or a List of Symbol/String variables for coordinate order", "List of NEKO expressions"),
    HELP_CMD("hessian", -1, Beast::Neko, "Computes the Hessian matrix of a scalar NEKO expression.", "Symbol, numeric value, or NEKO expression convertible to a NEKO expression, optionally followed by one Symbol/String variable or a List of Symbol/String variables for coordinate order", "List of List of NEKO expressions"),
    HELP_CMD("laplacian", -1, Beast::Neko, "Computes the Laplacian of a scalar NEKO expression.", "Symbol, numeric value, or NEKO expression convertible to a NEKO expression, optionally followed by one Symbol/String variable or a List of Symbol/String variables", "NEKO expression"),
    HELP_CMD("int", -1, Beast::Neko, "Integrates a NEKO expression with respect to x by default, optionally over numeric bounds, or with respect to an explicit variable.", "expression alone for symbolic integration in x; expression and Symbol/String variable for symbolic integration in that variable; expression plus numeric lower and upper bounds for definite integration in x; or expression, variable, lower bound, upper bound for definite integration in that variable", "NEKO expression for symbolic integrals, Decimal for definite integrals"),
    HELP_CMD("integral", -1, Beast::Neko, "Alias for int.", "expression alone for symbolic integration in x; expression and Symbol/String variable for symbolic integration in that variable; expression plus numeric lower and upper bounds for definite integration in x; or expression, variable, lower bound, upper bound for definite integration in that variable", "NEKO expression for symbolic integrals, Decimal for definite integrals"),
    HELP_CMD("eval", -1, Beast::Neko, "Evaluates a NEKO expression at a numeric value, using x by default or an explicit variable.", "expression and numeric value, or expression, Symbol/String variable, and numeric value", "Decimal"),
    HELP_CMD("roots", 1, Beast::Neko, "Finds roots of a supported expression in x, using real factorization before complex fallback for polynomials.", "Symbol/numeric/NEKO expression", "List of Decimal or Complex roots"),
    HELP_CMD("factorPoly", 1, Beast::Neko, "Factors a polynomial in x over real roots currently found by the real factorer.", "Symbol/numeric/NEKO expression representing a polynomial in x", "NEKO expression"),
    HELP_CMD("factorPolyReal", 1, Beast::Neko, "Factors a polynomial in x over real roots.", "Symbol/numeric/NEKO expression representing a polynomial in x", "NEKO expression"),
    HELP_CMD("factorPolyComplex", 1, Beast::Neko, "Factors a polynomial in x over complex roots.", "Symbol/numeric/NEKO expression representing a polynomial in x", "Symbol containing a formatted complex factorization"),
    HELP_CMD("funcArea", -1, Beast::Neko, "Computes the signed area between a function and zero on an interval.", "NEKO expression, numeric lower bound, numeric upper bound", "Decimal"),
    HELP_CMD("funcMax", 1, Beast::Neko, "Finds a numeric maximum on the default interval [-100, 100].", "Symbol/numeric/NEKO expression", "List [x, value] of Decimals"),
    HELP_CMD("funcMin", 1, Beast::Neko, "Finds a numeric minimum on the default interval [-100, 100].", "Symbol/numeric/NEKO expression", "List [x, value] of Decimals"),
    HELP_CMD("constraint", 1, Beast::Neko, "Converts an expression into a NEKO constraint expression for optimize commands.", "Symbol/numeric/NEKO expression", "NEKO expression"),
    HELP_CMD("minimize", -1, Beast::Neko, "Minimizes an objective expression on [-100, 100], optionally with constraints.", "objective Symbol/numeric/NEKO expression, optionally one constraint expression or a List of constraint expressions", "List [x, value] of Decimals"),
    HELP_CMD("maximize", -1, Beast::Neko, "Maximizes an objective expression on [-100, 100], optionally with constraints.", "objective Symbol/numeric/NEKO expression, optionally one constraint expression or a List of constraint expressions", "List [x, value] of Decimals"),
    HELP_CMD("solveODE", 1, Beast::Neko, "Solves or evaluates a supported scalar ordinary differential equation.", "String equation such as \"y'=y\" with optional initial-condition parameters and optional target x", "NEKO expression, Symbol relation, or Decimal evaluation"),
    HELP_CMD("solveODESystem", 1, Beast::Neko, "Evaluates a supported constant-coefficient first-order ODE system.", "List of String equations like [\"x'=y\", \"y'=-x\"]", "List of Decimal values"),

    HELP_CMD("force", 3, Beast::Poni, "Constructs a force vector with a name and tail position.", "String name, Vector force, Vector tail position", "Force"),
    HELP_CMD("body", 3, Beast::Poni, "Constructs a physics body.", "numeric mass, Vector position, Vector velocity", "Body"),
    HELP_CMD("bodySystem", -1, Beast::Poni, "Constructs a shallow system of bodies for simulation commands.", "one or more Body values, or a List of Body values", "BodySystem"),
    HELP_CMD("addForce", 2, Beast::Poni, "Attaches a force to a body.", "Body and Force", "Body"),
    HELP_CMD("removeForce", 2, Beast::Poni, "Removes an attached force from a body.", "Body and Force already attached to that Body", "Body"),
    HELP_CMD("displacement", 2, Beast::Poni, "Computes final position minus initial position.", "two same-dimension position Vectors", "Vector"),
    HELP_CMD("avgVelocity", 3, Beast::Poni, "Computes average velocity from two positions and elapsed time.", "initial position Vector, final position Vector, numeric time > 0", "Vector"),
    HELP_CMD("avgAccel", 4, Beast::Poni, "Computes constant acceleration from two positions, initial velocity, and elapsed time.", "initial position Vector, final position Vector, initial velocity Vector, numeric time > 0", "Vector"),
    HELP_CMD("velocityAtTime", 3, Beast::Poni, "Computes velocity after constant acceleration for a time.", "initial velocity Vector, acceleration Vector, numeric time >= 0", "Vector"),
    HELP_CMD("positionAtTime", 4, Beast::Poni, "Computes position after constant velocity and acceleration for a time.", "initial position Vector, velocity Vector, acceleration Vector, numeric time >= 0", "Vector"),
    HELP_CMD("speedAtPosition", 4, Beast::Poni, "Computes a speed-related vector at a target position under constant acceleration.", "initial position Vector, velocity Vector, acceleration Vector, target position Vector", "Vector"),
    HELP_CMD("velocityAtPosition", 4, Beast::Poni, "Computes velocity at a reachable target position under constant acceleration.", "initial position Vector, velocity Vector, acceleration Vector, target position Vector", "Vector"),
    HELP_CMD("projectileInfo", 3, Beast::Poni, "Computes projectile range, peak height, and flight time.", "numeric initial speed, numeric launch angle in degrees, numeric initial height", "Symbol summary"),
    HELP_CMD("centripetalAcceleration", 2, Beast::Poni, "Computes v^2/r.", "numeric velocity >= 0 and numeric radius > 0", "Decimal"),
    HELP_CMD("angularVelocity", 2, Beast::Poni, "Computes v/r.", "numeric velocity >= 0 and numeric radius > 0", "Decimal"),
    HELP_CMD("netForce", 1, Beast::Poni, "Sums all forces attached to a body.", "Body", "Vector"),
    HELP_CMD("accelerationFromForce", 1, Beast::Poni, "Computes acceleration from a body's net force and mass.", "Body", "Vector"),
    HELP_CMD("gravityForce", 1, Beast::Poni, "Constructs the gravitational force acting on a body.", "Body", "Force"),
    HELP_CMD("normalForce", 2, Beast::Poni, "Constructs a normal force for a body and surface normal.", "Body and Vector surface normal", "Force"),
    HELP_CMD("frictionForce", 3, Beast::Poni, "Constructs a friction force from a normal force, coefficient, and direction.", "Force normal force, numeric mu >= 0, Vector direction", "Force"),
    HELP_CMD("springForce", -1, Beast::Poni, "Constructs a Hooke's-law spring force on a body.", "Body, Vector anchor position, numeric spring constant k >= 0, and optional numeric rest length >= 0", "Force"),
    HELP_CMD("dragForce", -1, Beast::Poni, "Constructs a linear drag force opposing a body's velocity.", "Body and numeric drag coefficient >= 0", "Force"),
    HELP_CMD("gravitationalForce", -1, Beast::Poni, "Constructs the Newtonian gravitational force on the first body due to the second.", "Body source and Body attractor with distinct positions", "Force"),
    HELP_CMD("stepBody", -1, Beast::Poni, "Advances one body by a time step using its currently attached forces.", "Body and numeric time step dt >= 0", "Body"),
    HELP_CMD("step", -1, Beast::Poni, "Advances a body system by one time step using each body's currently attached forces.", "BodySystem and numeric time step dt >= 0", "BodySystem"),
    HELP_CMD("simulate", -1, Beast::Poni, "Advances a body system for a fixed number of equal time steps.", "BodySystem, numeric time step dt >= 0, and Int steps >= 0", "BodySystem"),
    HELP_CMD("momentum", 1, Beast::Poni, "Computes momentum vector m*v.", "Body", "Vector"),
    HELP_CMD("kineticEnergy", 1, Beast::Poni, "Computes translational kinetic energy.", "Body", "Decimal"),
    HELP_CMD("totalMomentum", 1, Beast::Poni, "Computes total linear momentum of a body system.", "BodySystem", "Vector"),
    HELP_CMD("totalEnergy", 1, Beast::Poni, "Computes kinetic plus mgy energy for a body system in the default vertical field.", "BodySystem", "Decimal"),
    HELP_CMD("gravPotentialEnergy", 2, Beast::Poni, "Computes gravitational potential energy mgh.", "Body and numeric height", "Decimal"),
    HELP_CMD("springPotentialEnergy", 2, Beast::Poni, "Computes spring potential energy.", "numeric spring constant k >= 0 and numeric displacement x >= 0", "Decimal"),
    HELP_CMD("work", 2, Beast::Poni, "Computes work as force dot displacement.", "Force and compatible displacement Vector", "Decimal"),
    HELP_CMD("power", 2, Beast::Poni, "Computes power as force dot velocity.", "Force and compatible velocity Vector", "Decimal"),
    HELP_CMD("impulse", 2, Beast::Poni, "Computes impulse vector force*time.", "Force and numeric time >= 0", "Vector"),
    HELP_CMD("centerOfMass", -1, Beast::Poni, "Computes center of mass for bodies.", "one or more Body values, or a List of Body values", "Vector"),
    HELP_CMD("centerOfMassVelocity", -1, Beast::Poni, "Computes center-of-mass velocity for bodies.", "one or more Body values, or a List of Body values", "Vector"),
    HELP_CMD("elasticCollision", -1, Beast::Poni, "Applies a one-dimensional elastic collision update.", "two Body values, or one List containing two Body values", "List [Body, Body]"),
    HELP_CMD("inelasticCollision", -1, Beast::Poni, "Applies a one-dimensional inelastic collision update.", "two Body values, or one List containing two Body values", "List [Body, Body]"),
    HELP_CMD("momentOfInertiaPoint", 2, Beast::Poni, "Computes moment of inertia for a point mass.", "numeric mass >= 0 and numeric radius >= 0", "Decimal"),
    HELP_CMD("momentOfInertiaRod", 2, Beast::Poni, "Computes moment of inertia for a rod about its center.", "numeric mass >= 0 and numeric length >= 0", "Decimal"),
    HELP_CMD("momentOfInertiaDisk", 2, Beast::Poni, "Computes moment of inertia for a disk.", "numeric mass >= 0 and numeric radius >= 0", "Decimal"),
    HELP_CMD("parallelAxis", 3, Beast::Poni, "Applies the parallel-axis theorem.", "numeric I_cm >= 0, numeric mass >= 0, numeric displacement", "Decimal"),
    HELP_CMD("torque", 2, Beast::Poni, "Computes torque about a pivot.", "Force and compatible pivot Vector", "Vector"),
    HELP_CMD("angularMomentum", 2, Beast::Poni, "Computes angular momentum about a pivot.", "Body and compatible pivot Vector", "Vector"),
    HELP_CMD("rotationalKineticEnergy", 2, Beast::Poni, "Computes rotational kinetic energy.", "numeric moment of inertia I >= 0 and numeric angular velocity", "Decimal"),
    HELP_CMD("angularAccelerationFromTorque", 2, Beast::Poni, "Computes angular acceleration from torque and moment of inertia.", "numeric net torque and numeric moment of inertia I > 0", "Decimal"),

    HELP_CMD("ZnGroup", 1, Beast::Usagi, "Constructs the cyclic group Z/nZ under addition.", "Int modulus n >= 1", "Group"),
    HELP_CMD("ZnProductGroup", 1, Beast::Usagi, "Constructs a direct product of cyclic groups.", "Vector of integer moduli, each >= 1", "Group"),
    HELP_CMD("Sn", 1, Beast::Usagi, "Constructs the symmetric group S_n.", "Int degree n >= 1", "Group"),
    HELP_CMD("An", 1, Beast::Usagi, "Constructs the alternating group A_n.", "Int degree n >= 1", "Group"),
    HELP_CMD("Dn", 1, Beast::Usagi, "Constructs the dihedral group D_n.", "Int degree n >= 1", "Group"),
    HELP_CMD("ZnRing", 1, Beast::Usagi, "Constructs the ring Z/nZ.", "Int modulus n >= 1", "Ring"),
    HELP_CMD("ZnProductRing", 1, Beast::Usagi, "Constructs a direct product of modular rings.", "Vector of integer moduli, each >= 1", "Ring"),
    HELP_CMD("primeField", 1, Beast::Usagi, "Constructs the prime finite field F_p.", "prime Int p >= 2", "Ring"),
    HELP_CMD("finiteField", 2, Beast::Usagi, "Constructs a finite field F_{p^k}.", "prime Int p >= 2 and Int k >= 0", "Ring"),
    HELP_CMD("FFRing", -1, Beast::Usagi, "Constructs a finite field ring F_{p^n}.", "prime-power Int q >= 2, or prime Int p >= 2 and Int n >= 0", "Ring"),
    HELP_CMD("addGroup", 1, Beast::Usagi, "Constructs the additive group of a ring.", "Ring", "Group"),
    HELP_CMD("unitGroup", 1, Beast::Usagi, "Constructs the multiplicative unit group of a ring.", "Ring with multiplicative identity", "Group"),
    HELP_CMD("Q8", 0, Beast::Usagi, "Constructs the quaternion group Q8.", "no values", "Group"),
    HELP_CMD("listElements", 1, Beast::Usagi, "Lists the elements of a group or ring.", "Group or Ring", "List of GroupElement values for a Group, or RingElement values for a Ring"),
    HELP_CMD("numElements", 1, Beast::Usagi, "Counts the elements of a group or ring.", "Group or Ring", "Int"),
    HELP_CMD("cardinality", 1, Beast::Usagi, "Computes the order/cardinality of a finite group or ring. Also available as |G| or |R|.", "Group or Ring", "Int"),
    HELP_CMD("getElement", 2, Beast::Usagi, "Retrieves an element by its printed representation.", "Group or Ring, then String representation", "GroupElement for Group input, RingElement for Ring input"),
    HELP_CMD("groupElementConjugate", 2, Beast::Usagi, "Computes g*h*g^-1 for group elements.", "GroupElement conjugator and GroupElement target from the same Group", "GroupElement"),
    HELP_CMD("groupCommutator", 2, Beast::Usagi, "Computes the commutator of two group elements.", "two GroupElement values from the same Group", "GroupElement"),
    HELP_CMD("commutator", 2, Beast::Usagi, "Computes the commutator for group or ring elements.", "two compatible GroupElement or RingElement values", "same algebraic element family"),
    HELP_CMD("associator", 3, Beast::Usagi, "Computes the associator for group or ring elements.", "three compatible GroupElement or RingElement values", "same algebraic element family"),
    HELP_CMD("elementOrderGroup", 1, Beast::Usagi, "Computes the order of a group element.", "GroupElement", "Int"),
    HELP_CMD("additiveOrder", 1, Beast::Usagi, "Computes the additive order of a ring element.", "RingElement", "Int"),
    HELP_CMD("multiplicativeOrder", 1, Beast::Usagi, "Computes the multiplicative order of a ring element.", "RingElement", "Int"),
    HELP_CMD("isInGroup", 2, Beast::Usagi, "Tests whether a group element belongs to a group.", "Group and GroupElement", "Bool"),
    HELP_CMD("isGroupIdentity", 2, Beast::Usagi, "Tests whether a group element is the identity of a group.", "Group and GroupElement", "Bool"),
    HELP_CMD("groupIdentity", 1, Beast::Usagi, "Returns the identity element of a group.", "Group", "GroupElement"),
    HELP_CMD("isAddIdentity", 2, Beast::Usagi, "Tests whether a ring element is the additive identity of a ring.", "Ring and RingElement", "Bool"),
    HELP_CMD("isMultIdentity", 2, Beast::Usagi, "Tests whether a ring element is the multiplicative identity of a ring.", "Ring and RingElement", "Bool"),
    HELP_CMD("hasMultIdentity", 1, Beast::Usagi, "Tests whether a ring has a multiplicative identity.", "Ring", "Bool"),
    HELP_CMD("elementsCommute", 2, Beast::Usagi, "Tests whether two group elements commute.", "two GroupElement values from the same Group", "Bool"),
    HELP_CMD("isTrivialGroup", 1, Beast::Usagi, "Tests whether a group has exactly one element.", "Group", "Bool"),
    HELP_CMD("isTrivialRing", 1, Beast::Usagi, "Tests whether a ring has exactly one element.", "Ring", "Bool"),
    HELP_CMD("trivialGroup", 0, Beast::Usagi, "Constructs the one-element group.", "no values", "Group"),
    HELP_CMD("trivialRing", 0, Beast::Usagi, "Constructs the one-element ring.", "no values", "Ring"),
    HELP_CMD("groupInfo", 1, Beast::Usagi, "Formats information about a group.", "Group", "Symbol summary"),
    HELP_CMD("ringInfo", 1, Beast::Usagi, "Formats information about a ring.", "Ring", "Symbol summary"),
    HELP_CMD("groupElementInfo", 1, Beast::Usagi, "Formats information about a group element.", "GroupElement", "Symbol summary"),
    HELP_CMD("ringElementInfo", 1, Beast::Usagi, "Formats information about a ring element.", "RingElement", "Symbol summary"),
    HELP_CMD("isSubgroup", 2, Beast::Usagi, "Tests whether a subgroup is a subgroup of a group.", "Group and SubGroup", "Bool"),
    HELP_CMD("isNormalSubgroup", 2, Beast::Usagi, "Tests whether a subgroup is normal in a group.", "Group and SubGroup", "Bool"),
    HELP_CMD("trivialSubgroup", 0, Beast::Usagi, "Constructs the trivial subgroup object.", "no values", "SubGroup"),
    HELP_CMD("isTrivialSubgroup", 1, Beast::Usagi, "Tests whether a subgroup is trivial.", "SubGroup", "Bool"),
    HELP_CMD("groupCenter", 1, Beast::Usagi, "Computes the center of a group.", "Group", "SubGroup"),
    HELP_CMD("groupCentralizer", 2, Beast::Usagi, "Computes the centralizer of an element in a group.", "Group and GroupElement from that Group", "SubGroup"),
    HELP_CMD("groupNormalizer", 1, Beast::Usagi, "Computes the normalizer of a subgroup in its ambient group.", "SubGroup", "SubGroup"),
    HELP_CMD("conjugacyClass", 1, Beast::Usagi, "Formats the conjugacy class of a group element.", "GroupElement", "Symbol summary"),
    HELP_CMD("getConjClass", 1, Beast::Usagi, "Constructs the conjugacy class object for a group element.", "GroupElement", "ConjugacyClass"),
    HELP_CMD("listConjClasses", 1, Beast::Usagi, "Lists conjugacy class objects for a group or for the group of an element.", "Group or GroupElement", "List of ConjugacyClass values"),
    HELP_CMD("getConjClasses", 1, Beast::Usagi, "Lists conjugacy class objects for a group or for the group of an element.", "Group or GroupElement", "List of ConjugacyClass values"),
    HELP_CMD("numConjClasses", 1, Beast::Usagi, "Counts conjugacy classes of a group or of the group of an element.", "Group or GroupElement", "Int"),
    HELP_CMD("normalClosure", 1, Beast::Usagi, "Computes the normal closure of a subgroup.", "SubGroup", "SubGroup"),
    HELP_CMD("isInSubgroup", 2, Beast::Usagi, "Tests whether a group element is in a subgroup.", "SubGroup and GroupElement", "Bool"),
    HELP_CMD("subgroupContains", 2, Beast::Usagi, "Tests whether the first subgroup contains the second.", "SubGroup container and SubGroup candidate", "Bool"),
    HELP_CMD("subgroupIndex", 1, Beast::Usagi, "Computes the index of a subgroup in its ambient group.", "SubGroup", "Int"),
    HELP_CMD("subgroupConjugate", 2, Beast::Usagi, "Conjugates a subgroup by a group element.", "GroupElement and SubGroup from the same ambient Group", "SubGroup"),
    HELP_CMD("commutatorSubgroup", 1, Beast::Usagi, "Computes the commutator subgroup.", "Group", "SubGroup"),
    HELP_CMD("abelianization", 1, Beast::Usagi, "Computes the abelianization of a group.", "Group", "Group"),
    HELP_CMD("listNormalSubgroups", 1, Beast::Usagi, "Lists normal subgroups of a group.", "Group", "Symbol summary"),
    HELP_CMD("listMaximalSubgroups", 1, Beast::Usagi, "Lists maximal subgroups of a group.", "Group", "Symbol summary"),
    HELP_CMD("numNormalSubgroups", 1, Beast::Usagi, "Counts normal subgroups of a group.", "Group", "Int"),
    HELP_CMD("numMaximalSubgroups", 1, Beast::Usagi, "Counts maximal subgroups of a group.", "Group", "Int"),
    HELP_CMD("largestCoreFreeSubgroup", 1, Beast::Usagi, "Finds a largest core-free subgroup.", "Group", "SubGroup"),
    HELP_CMD("isCyclicGroup", 1, Beast::Usagi, "Tests whether a group is cyclic.", "Group", "Bool"),
    HELP_CMD("isCyclicSubgroup", 1, Beast::Usagi, "Tests whether a subgroup is cyclic.", "SubGroup", "Bool"),
    HELP_CMD("generatesCyclicGroup", 2, Beast::Usagi, "Tests whether an element generates a cyclic group.", "Group and GroupElement", "Bool"),
    HELP_CMD("generatesCyclicSubgroup", 2, Beast::Usagi, "Tests whether an element generates a cyclic subgroup.", "SubGroup and GroupElement", "Bool"),
    HELP_CMD("getCyclicSubgroup", 1, Beast::Usagi, "Constructs the cyclic subgroup generated by an element.", "GroupElement", "SubGroup"),
    HELP_CMD("isInCoset", 2, Beast::Usagi, "Tests whether a group element lies in a group coset.", "GroupCoset and GroupElement", "Bool"),
    HELP_CMD("leftCoset", 2, Beast::Usagi, "Constructs a left coset of a subgroup.", "SubGroup and GroupElement from its ambient Group", "GroupCoset"),
    HELP_CMD("rightCoset", 2, Beast::Usagi, "Constructs a right coset of a subgroup.", "SubGroup and GroupElement from its ambient Group", "GroupCoset"),
    HELP_CMD("listLeftCosets", 1, Beast::Usagi, "Lists all left cosets of a subgroup in its ambient group.", "SubGroup", "List of GroupCoset values"),
    HELP_CMD("listRightCosets", 1, Beast::Usagi, "Lists all right cosets of a subgroup in its ambient group.", "SubGroup", "List of GroupCoset values"),
    HELP_CMD("numLeftCosets", 1, Beast::Usagi, "Returns the number of left cosets of a subgroup in its ambient group.", "SubGroup", "Int"),
    HELP_CMD("numRightCosets", 1, Beast::Usagi, "Returns the number of right cosets of a subgroup in its ambient group.", "SubGroup", "Int"),
    HELP_CMD("times", 2, Beast::Usagi, "Computes a cross product or direct product.", "two 3D Vectors, two Groups, or two Rings", "Vector for Vectors, Group for Groups, Ring for Rings"),
    HELP_CMD("kProductGroup", 2, Beast::Usagi, "Constructs the k-fold direct product of a group.", "Group and positive Int k", "Group"),
    HELP_CMD("kProductRing", 2, Beast::Usagi, "Constructs the k-fold direct product of a ring.", "Ring and positive Int k", "Ring"),
    HELP_CMD("subring", 1, Beast::Usagi, "Constructs the subring generated by ring elements.", "RingElement or List of RingElement values from the same Ring", "SubRing"),
    HELP_CMD("leftIdeal", 1, Beast::Usagi, "Constructs the left ideal generated by ring or CD elements.", "RingElement values from the same Ring, or CD element generators from the same algebra", "Ideal or CD ideal"),
    HELP_CMD("rightIdeal", 1, Beast::Usagi, "Constructs the right ideal generated by ring or CD elements.", "RingElement values from the same Ring, or CD element generators from the same algebra", "Ideal or CD ideal"),
    HELP_CMD("isTrivialSubring", 1, Beast::Usagi, "Tests whether a subring is trivial.", "SubRing", "Bool"),
    HELP_CMD("subgroupGeneratedBy", 2, Beast::Usagi, "Constructs the subgroup generated by one or more group elements.", "Group and GroupElement or List of GroupElement values from that Group", "SubGroup"),
    HELP_CMD("subgroupAsGroup", 1, Beast::Usagi, "Converts a subgroup into a standalone group.", "SubGroup", "Group"),
    HELP_CMD("groupHomomorphism", 3, Beast::Usagi, "Constructs a group homomorphism from a complete mapping.", "domain Group, codomain Group, and List of pairs mapping every domain GroupElement to a codomain GroupElement", "GroupHomomorphism"),
    HELP_CMD("ringHomomorphism", 3, Beast::Usagi, "Constructs a ring homomorphism from a complete mapping.", "domain Ring, codomain Ring, and List of pairs mapping every domain RingElement to a codomain RingElement", "RingHomomorphism"),
    HELP_CMD("groupHomomorphismKernel", 1, Beast::Usagi, "Computes the kernel of a group homomorphism.", "GroupHomomorphism", "SubGroup"),
    HELP_CMD("ringHomomorphismKernel", 1, Beast::Usagi, "Computes the kernel of a ring homomorphism.", "RingHomomorphism", "Ideal"),
    HELP_CMD("groupHomomorphismImage", 1, Beast::Usagi, "Computes the image of a group homomorphism.", "GroupHomomorphism", "Group"),
    HELP_CMD("ringHomomorphismImage", 1, Beast::Usagi, "Computes the image of a ring homomorphism.", "RingHomomorphism", "Ring"),
    HELP_CMD("isGroupIsomorphism", 1, Beast::Usagi, "Tests whether a group homomorphism is an isomorphism.", "GroupHomomorphism", "Bool"),
    HELP_CMD("isRingIsomorphism", 1, Beast::Usagi, "Tests whether a ring homomorphism is an isomorphism.", "RingHomomorphism", "Bool"),
    HELP_CMD("listSubgroups", 1, Beast::Usagi, "Formats a list of all subgroups of a group.", "Group", "Symbol summary"),
    HELP_CMD("numSubgroups", 1, Beast::Usagi, "Counts all subgroups of a group.", "Group", "Int"),
    HELP_CMD("getSubgroup", 2, Beast::Usagi, "Retrieves a subgroup by one-based index from the subgroup list.", "Group and Int index >= 1", "SubGroup"),
    HELP_CMD("subgroupInfo", 1, Beast::Usagi, "Formats information about a subgroup and its ambient group.", "SubGroup", "Symbol summary"),
    HELP_CMD("subringInfo", 1, Beast::Usagi, "Formats information about a subring and its ambient ring.", "SubRing", "Symbol summary"),
    HELP_CMD("idealInfo", 1, Beast::Usagi, "Formats information about an ideal and its ambient ring.", "Ideal", "Symbol summary"),
    HELP_CMD("groupHomomorphismInfo", 1, Beast::Usagi, "Formats information about a group homomorphism.", "GroupHomomorphism", "Symbol summary"),
    HELP_CMD("ringHomomorphismInfo", 1, Beast::Usagi, "Formats information about a ring homomorphism.", "RingHomomorphism", "Symbol summary"),
    HELP_CMD("isCommutativeGroup", 1, Beast::Usagi, "Tests whether a group is commutative.", "Group", "Bool"),
    HELP_CMD("isCommutativeRing", 1, Beast::Usagi, "Tests whether a ring is commutative.", "Ring", "Bool"),
    HELP_CMD("isSimple", 1, Beast::Usagi, "Tests whether a group is simple.", "Group", "Bool"),
    HELP_CMD("isInverse", 2, Beast::Usagi, "Tests whether two group elements are inverses.", "two GroupElement values from the same Group", "Bool"),
    HELP_CMD("isAddInverse", 2, Beast::Usagi, "Tests whether two ring elements are additive inverses.", "two RingElement values from the same Ring", "Bool"),
    HELP_CMD("isMultInverse", 2, Beast::Usagi, "Tests whether two ring elements are multiplicative inverses.", "two RingElement values from the same Ring", "Bool"),
    HELP_CMD("hasMultInverse", 1, Beast::Usagi, "Tests whether a ring element has a multiplicative inverse in its ring.", "RingElement", "Bool"),
    HELP_CMD("isZeroDivisor", 1, Beast::Usagi, "Tests whether a ring element is a zero divisor in its ring.", "RingElement", "Bool"),
    HELP_CMD("isLeftZeroDivisor", 1, Beast::Usagi, "Tests whether a ring element is a zero divisor in its ring.", "RingElement", "Bool"),
    HELP_CMD("isRightZeroDivisor", 1, Beast::Usagi, "Tests whether a ring element is a zero divisor in its ring.", "RingElement", "Bool"),
    HELP_CMD("hasZeroDivisors", 1, Beast::Usagi, "Tests whether a ring has any zero divisors.", "Ring", "Bool"),
    HELP_CMD("isIntegralDomain", 1, Beast::Usagi, "Tests whether a ring is an integral domain.", "Ring", "Bool"),
    HELP_CMD("isDivisionRing", 1, Beast::Usagi, "Tests whether a ring is a division ring.", "Ring", "Bool"),
    HELP_CMD("isField", 1, Beast::Usagi, "Tests whether a ring is a field.", "Ring", "Bool"),

    HELP_CMD("charIP", 2, Beast::Tora, "Computes an inner product for characters, representations, or vectors.", "two Characters, two Representations, or two same-dimension Vectors", "Int, Decimal, Fraction, or Complex scalar"),
    HELP_CMD("trivialRep", -1, Beast::Tora, "Constructs the trivial representation.", "zero values for the trivial group, or one Group", "Representation"),
    HELP_CMD("regularRep", 1, Beast::Tora, "Constructs the regular representation of a group.", "Group", "Representation"),
    HELP_CMD("permutationRep", 1, Beast::Tora, "Constructs the permutation representation of a group.", "Group", "Representation"),
    HELP_CMD("standardRep", 1, Beast::Tora, "Constructs the standard representation of a group.", "Group", "Representation"),
    HELP_CMD("signRep", 1, Beast::Tora, "Constructs the sign representation of a symmetric group.", "Group that is a symmetric group", "Representation"),
    HELP_CMD("projectToAbelianization", 1, Beast::Tora, "Constructs the canonical projection from a group to its abelianization.", "Group", "GroupHomomorphism"),
    HELP_CMD("dualRep", 1, Beast::Tora, "Constructs the dual representation.", "Representation", "Representation"),
    HELP_CMD("conjRep", 1, Beast::Tora, "Constructs the complex-conjugate representation.", "Representation", "Representation"),
    HELP_CMD("prodRep", 2, Beast::Tora, "Constructs the external product representation.", "Representation and Representation", "Representation"),
    HELP_CMD("tensorRep", 2, Beast::Tora, "Constructs the tensor product of two representations of the same group.", "Representation and Representation", "Representation"),
    HELP_CMD("symRep", 1, Beast::Tora, "Constructs the symmetric square representation.", "Representation", "Representation"),
    HELP_CMD("wedgeRep", 1, Beast::Tora, "Constructs the exterior square representation.", "Representation", "Representation"),
    HELP_CMD("resRep", 2, Beast::Tora, "Restricts a representation to a subgroup.", "Representation and SubGroup", "Representation"),
    HELP_CMD("indRep", 2, Beast::Tora, "Induces a representation from a subgroup.", "Representation and SubGroup", "Representation"),
    HELP_CMD("getChar", 1, Beast::Tora, "Computes and remembers the character of a representation.", "Representation", "Symbol summary; the Character is stored for later evalChar"),
    HELP_CMD("evalChar", -1, Beast::Tora, "Evaluates a character at a group element.", "GroupElement after getChar, or Representation and GroupElement, or Character and GroupElement", "Int, Decimal, Fraction, or Complex scalar"),
    HELP_CMD("repDegree", 1, Beast::Tora, "Returns the degree of a representation.", "Representation", "Int"),
    HELP_CMD("charDegree", 1, Beast::Tora, "Returns the degree of a character, or of the character of a representation.", "Character or Representation", "Int, Decimal, or Complex scalar"),
    HELP_CMD("isIrrep", 1, Beast::Tora, "Tests whether a representation is irreducible.", "Representation", "Bool"),
    HELP_CMD("listIrreps", 1, Beast::Tora, "Lists labels for irreducible characters of a group.", "Group", "List of String labels"),
    HELP_CMD("numIrreps", 1, Beast::Tora, "Counts irreducible characters of a group.", "Group", "Int"),
    HELP_CMD("getIrrep", 2, Beast::Tora, "Retrieves an irreducible character by one-based index.", "Group and Int index >= 1", "Character"),
    HELP_CMD("decomposeRep", 1, Beast::Tora, "Decomposes a representation into irreducible multiplicities.", "Representation", "List of Int multiplicities"),
    HELP_CMD("charTable", 1, Beast::Tora, "Constructs the character table of a group.", "Group", "CharacterTable"),
    HELP_CMD("printCharTable", 1, Beast::Tora, "Prints a character table to stdout.", "CharacterTable", "None"),
    HELP_CMD("printTable", 1, Beast::Tora, "Alias for printCharTable.", "CharacterTable", "None"),
    HELP_CMD("printRep", 1, Beast::Tora, "Prints all representation matrices to stdout.", "Representation", "None"),

    HELP_CMD("cap", 2, Beast::Ookami, "Computes the intersection of two compatible objects.", "two CombSets, two CD ideals, or two CD subalgebras", "CombSet, CD ideal, or CD subalgebra"),
    HELP_CMD("intersect", 2, Beast::Ookami, "Alias for cap.", "two CombSets, two CD ideals, or two CD subalgebras", "CombSet, CD ideal, or CD subalgebra"),
    HELP_CMD("cup", 2, Beast::Ookami, "Computes the union of two finite integer sets.", "CombSet and CombSet", "CombSet"),
    HELP_CMD("isSubset", 2, Beast::Ookami, "Tests whether the first finite integer set is a subset of the second.", "CombSet candidate subset and CombSet candidate superset", "Bool"),
    HELP_CMD("cardinality", 1, Beast::Ookami, "Computes the cardinality of a finite integer set. Also available as |A|.", "CombSet", "Int"),
    HELP_CMD("diameter", 1, Beast::Ookami, "Computes max(A)-min(A) for a nonempty finite integer set.", "nonempty CombSet", "Int"),
    HELP_CMD("dconst", 1, Beast::Ookami, "Computes the additive doubling constant |A+A|/|A|.", "nonempty CombSet", "Int or Fraction"),
    HELP_CMD("density", 1, Beast::Ookami, "Computes set density inside its integer span.", "nonempty CombSet", "Int or Fraction"),
    HELP_CMD("rangeSet", -1, Beast::Ookami, "Constructs a finite integer range as a CombSet.", "Int start, Int end, and optional nonzero Int step; when step is omitted it defaults to 1 or -1 based on endpoint order", "CombSet"),
    HELP_CMD("AP", 3, Beast::Ookami, "Constructs an arithmetic progression as a finite integer set.", "Int first term, Int common difference, Int number of terms >= 0", "CombSet"),
    HELP_CMD("GP", 3, Beast::Ookami, "Constructs a geometric progression as a finite integer set.", "Int first term, Int common ratio, Int number of terms >= 0", "CombSet"),
    HELP_CMD("subsetSums", -1, Beast::Ookami, "Computes subset sums of a finite integer set.", "CombSet, optionally followed by Int subset size k >= 0", "CombSet"),
    HELP_CMD("translate", 2, Beast::Ookami, "Translates every element of a finite integer set by an integer.", "CombSet and Int translation", "CombSet"),
    HELP_CMD("dilate", 2, Beast::Ookami, "Multiplies every element of a finite integer set by an integer.", "CombSet and Int scale", "CombSet"),
    HELP_CMD("append", 2, Beast::Basic, "Appends a value to a List or adds an integer to a finite integer set.", "List and any value, or CombSet and Int element", "List or CombSet"),
    HELP_CMD("remove", 2, Beast::Basic, "Removes the first matching value from a List or removes an integer from a finite integer set.", "List and any value, or CombSet and Int element already in the set", "List or CombSet"),
    HELP_CMD("adsCard", 1, Beast::Ookami, "Counts the additive sumset A+A.", "CombSet", "Int"),
    HELP_CMD("ddsCard", 1, Beast::Ookami, "Counts the difference set A-A.", "CombSet", "Int"),
    HELP_CMD("mdsCard", 1, Beast::Ookami, "Counts the product set A*A.", "CombSet", "Int"),
    HELP_CMD("isAP", 1, Beast::Ookami, "Tests whether a finite integer set is an arithmetic progression.", "CombSet", "Bool"),
    HELP_CMD("isGP", 1, Beast::Ookami, "Tests whether a finite integer set is a geometric progression.", "CombSet", "Bool"),
    HELP_CMD("ruzsaDistance", 2, Beast::Ookami, "Computes Ruzsa distance between two finite integer sets.", "CombSet and CombSet", "Decimal"),
    HELP_CMD("ruzsaDistancePositive", 2, Beast::Ookami, "Computes positive Ruzsa distance between two finite integer sets.", "CombSet and CombSet", "Decimal"),
    HELP_CMD("repAdd", 2, Beast::Ookami, "Counts ordered additive representations a+b=x.", "CombSet A and Int x", "Int"),
    HELP_CMD("kRepAdd", 3, Beast::Ookami, "Counts ordered k-fold additive representations summing to x.", "CombSet A, Int k >= 1, and Int x", "Int"),
    HELP_CMD("repDiff", 2, Beast::Ookami, "Counts ordered difference representations a-b=x.", "CombSet A and Int x", "Int"),
    HELP_CMD("kRepDiff", 3, Beast::Ookami, "Counts ordered k-fold difference representations equal to x.", "CombSet A, Int k >= 1, and Int x", "Int"),
    HELP_CMD("repMult", 2, Beast::Ookami, "Counts ordered multiplicative representations a*b=x.", "CombSet A and Int x", "Int"),
    HELP_CMD("kRepMult", 3, Beast::Ookami, "Counts ordered k-fold multiplicative representations equal to x.", "CombSet A, Int k >= 1, and Int x", "Int"),
    HELP_CMD("energyAdd", 1, Beast::Ookami, "Computes additive energy.", "CombSet", "Int"),
    HELP_CMD("kEnergyAdd", 2, Beast::Ookami, "Computes k-fold additive energy.", "CombSet and Int k >= 1", "Int"),
    HELP_CMD("energyDiff", 1, Beast::Ookami, "Computes difference energy.", "CombSet", "Int"),
    HELP_CMD("kEnergyDiff", 2, Beast::Ookami, "Computes k-fold difference energy.", "CombSet and Int k >= 1", "Int"),
    HELP_CMD("energyMult", 1, Beast::Ookami, "Computes multiplicative energy.", "CombSet", "Int"),
    HELP_CMD("kEnergyMult", 2, Beast::Ookami, "Computes k-fold multiplicative energy.", "CombSet and Int k >= 1", "Int"),
};

#undef HELP_CMD

static constexpr size_t kSectionCount = sizeof(kSections) / sizeof(kSections[0]);
static constexpr size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

} // namespace BestiaryHelpPage

#endif
