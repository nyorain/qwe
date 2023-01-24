# A simple file format for hierarchical data

This contains a specification and various implementations/utilities for a 
very simple, lightweight, flexible data format:

```yaml
# This is a comment
bottom: 6360000.0
mie:
  g: 0.8
  scale_height: 1200
  scattering:
    rgb:
      5.e-5
      5.e-5
      5.e-5
rayleigh:
  scattering:
    rgb:
      6.95e-6
      1.18e-5
      2.44e-5
  scale_height: 1200
```

Goals: easy to write, easy to read, easy and efficient to parse, as little
restrictions as possible.

The main difference to most other markup languages: The full specification is
like 4 lines long and does __not care about semantics__ at all.
It only specifies a way of having __structured text__, there is no
complicated distinction between string, number, date, whatever on the 
language level.
Which I found to be much more convenient to work with in many cases as
it allows to flexibly fit the semantics to your usecase and means
that language and parser are incredibly simple.

In terms of C++, the entire data structure for a `qwe` files could look like 
this (even though this specific representation is inefficient):

```cpp
struct Table : std::vector<std::pair<std::string, Table>> {};
```

## APIs

This repository provides multiple APIs. All of this is rather WIP and not in a final/clean state.

- [parse_cb.h](parse_cb.h) is a standalone implementation of a non-allocating C parser
  that simply forwards the parsed data directly to a supplied callback.
  Most low-level interface but probably the most simple and small implementation.
- `data.h`, `parse.h`, `print.h` implement a C data representation,
  parser and printer for it. All the headers are small and can easily
  combined into a single one. To keep it simple, tables are represented
  as (dynamically sized) linear arrays as well instead of (hash)maps.
- `common.hpp`, `data.hpp`, `util.hpp`, `parse.hpp`, `print.hpp` implement a 
  high-level C++17 data representation, utilities for easy interaction with it,
  a parser and a printer.
- The [s2](s2) folder implements the WIP second iteration of the language,
  which is even simpler. [s2/parse2.hpp](s2/parse2.hpp) implements a lightning
  fast, single-pass, allocation-less, <200loc parser that does not depend on
  any non-trivial stl features and just forwards the extracted 
  `std::string_view` values to user-provided callbacks.

## Related projects

- [inih](https://github.com/benhoyt/inih) for INI files, extremely lightweight.
  For more lightweight configuration it's probably more stable and better
  tested than this. But it does not allow arbitrary nesting.
- [toml](https://github.com/toml-lang/toml)
- [strictyaml](https://github.com/crdoconnor/strictyaml)

Why did I write my own one? All of them were either too big of a dependency
for something that seemed quite easy to me or not powerful enough.
This is basically a combination of the extremely simple and lightweight
approach of 'inih' combined with a more 'strictyaml'-like language, i.e.
more scalable configuration, allowing nested structures.
An argument against 'strictyaml' is that it currently lacks a real
specification and is still not as lightweight as I imagined my configuration
system to be.
TOML is really promising but implementations of it are **not** trivial due
to all its features (that I don't consider worth it and wouldn't ever 
need anyways).

### Does the world really need this?

Nope. But it was fun to make!
Which is the main justification for this to exist.

Is this a classic case of ['not invented here' (NIH)](https://en.wikipedia.org/wiki/Not_invented_here)?
Yeah, probably, that's exactly what this is. But it was fun to see
if it's as easy as I'd thought it would be to write a simple configuration file
format, representation and parser.

I'd also argue that experiments aiming at making software simpler, more
efficient and intuitive are very well justified.

