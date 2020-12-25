```yaml
string ::= [^\n\t][^\n]*
name ::= string
value(i) ::= string | \n table(i + 1) | \n array(i + 1)
table(i) ::= (\t{i}name: value(i)\n)+
array(i) ::= (\t{i}string\n)+
file ::= table(0) | array(0)
```

Where i can be any natural number. Parsers should parse `file` rules.
The space between `\n` and `table`/`array` is only for notation and not 
there in the grammar. The grammar should already show why names and string
values are not allowed to start with `\t` and not at all allowed to
contain newlines. Otherwise (except the natural space after ':' in a table
assignment), white-space is simply treated as part of the identifier or
string value.

Nothing else is specified, really.
How data is interpreted is up to whoever interprets it.
But whoever writes a file can make sure that the data can be interpreted
the way they intend it. For instance, for numbers, usual number formats
should be used: Something like '2.234' or '7' should be parse as an int
everywhere, notations like '-3.5e-7' or '+034.23' are common enough as well.
But one cannot expect, for instance, readers to be able to parse expressions
like 'e^-3' or binary such as '011010' or '0b0110' as the correct number.

Schema verification is outside of the scope of the file format itself
as well. The expressiveness of the language is powerful enough to express
something like this though.

Problems:

- just using ":" in an array string will per spec just be part of the string.
  The cb and value-parsing implementations will instead return the
  "mixed array/table" error. The "serialize.hpp" implementation
  just accepts it. The problem here is that it looks weird (e.g. with
  syntax highlighting). But then again, it will look weird anyways
  if array and table are mixed. We should stick with the way the spec
  defined it and fix that in implementations.
  But: when a string containing ":" is the first value of an array,
  it will be interpreted as table. Extra rules for this are insane,
  just add the advise to manually escape colons in strings
  (only really needed for the first-value-in-array case though)
  via \:, and then just manually replace it in the application/parser.


Extended syntax with array-nesting:
```yaml
string ::= [^\n\t][^\n]*
name ::= string
value(i) ::= string | \n table(i + 1) | \n array(i + 1)
table(i) ::= (\t{i}name: value(i)\n)+
array(i) ::= (\t{i}string\n)+
arraynest(i) ::= \t{i}-\n(\t{i}value(i)\n)+
file ::= value(0) | table(0) | array(0)
```
