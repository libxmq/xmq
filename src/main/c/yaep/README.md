# YAEP -- Yet Another Earley Parser

  This is a fork of https://github.com/vnmakarov/yaep

  * **YAEP** is an abbreviation of Yet Another Earley Parser.
  * This standalone library is created for convenience.
  * The parser development is actually done as a part of the [*Dino* language
    project](https://github.com/dino-lang/dino).
  * YAEP is licensed under the MIT license.

# YAEP features:
  * It is sufficiently fast and does not require much memory.
    This is the **fastest** implementation of the Earley parser which I
    know of. If you know a faster one, please send me a message. It can parse
    **300K lines of C program per second** on modern computers
    and allocates about **5MB memory for 10K line C program**.
  * YAEP does simple syntax directed translation, producing an **abstract
    syntax tree** as its output.
  * It can parse input described by an **ambiguous** grammar.  In
    this case the parse result can be a single abstract tree or all
    possible abstract trees. YAEP produces a compact
    representation of all possible parse trees by using DAG instead
    of real trees.
  * YAEP can parse input described by an ambiguous grammar
    according to **abstract node costs**.  In this case the parse
    result can be a **minimal cost** abstract tree or all possible
    minimal cost abstract trees.  This feature can be used to code
    selection task in compilers.
  * It can perform **syntax error recovery**.  Moreover its error
    recovery algorithm finds error recovery with a **minimal** number of
    ignored tokens.  This permits implementing parsers with very good
    error recovery and reporting.
  * It has **fast startup**.  There is only a tiny and insignificant delay
    between processing grammar and the start of parsing.
