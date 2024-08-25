# Egg Strings Format

One "strings" resource contains multiple strings keyed by index, expected to be all in one language.
Strings should be encoded UTF-8. For the most part, tooling is agnostic toward text encoding.
Length of individual strings must be in 0..32767.

You are expected to encode the strings' language in their rid.
For input files, put the language code in front of rid, and restrict rid to 1..63: `en-1-my_strings.txt`.
After encoding, the rid can be understood as `((lang[0]-'a')<<11)|((lang[1]-'a')<<6)|subrid`.

There's no definite limit to index, and they're allowed to be sparse, but be reasonable.
Decoders might restrict index, and might balk at excessive empty runs.
Decoders must at a minimum allow index up to 1023, and empty runs up to 100.
Our compiler will reject index above 32767.

## Binary Format

Starts with a 4-byte signature: "\0ES\xff"

Followed by any count of strings, starting with index zero.
Each begins with their length:
```
0ccccccc           Length in 0..127.
1mmmmmmm llllllll  Length in 0..32767 (NB no bias).
```
Followed by raw text.

## Text Format

Plain line-oriented text.
'#' begins a line comment, start of line only.
Empty lines are ignored.

Each line must begin with the decimal index.
Indices must be sorted.

After index, either loose text or a quoted JSON string.
Leading and trailing whitespace will be trimmed from loose text, and it can't start with a quote.
We make no provision for continued lines.
