# APIs

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

## Related projects

- [inih](https://github.com/benhoyt/inih) for INI files, extremely lightweight.
  For more lightweight configuration it's probably more stable and better
  tested than this. Probably also even more lightweight.
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

Is this a classic case of ['not invented here' (NIH)](https://en.wikipedia.org/wiki/Not_invented_here)?
Yeah, probably, that's exactly what this is. But it was fun to see
if it's as easy as I'd thought it would be to write a configuration file
format, representation and parser.
