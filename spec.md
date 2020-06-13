```yaml
string ::= [^\n\t][^\n]*
name ::= string
value(i) ::= string | \n table(i + 1) | \n array(i + 1)
table(i) ::= (\t{i}name: value(i)\n)+
array(i) ::= (\t{i}string\n)+
file ::= value(0) | table(0) | array(0)
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
