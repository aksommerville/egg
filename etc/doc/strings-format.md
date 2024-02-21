# Egg Strings Format

By convention, strings are stored as resources named "s/LANG/ID" where:
- LANG is two lowercase letters (ISO 631).
- ID is a decimal integer.

These should be sourced from files name "s/LANG".
Plain text file.
Blank lines, and lines beginning '#', are ignored.

Every valid line is "ID CONTENT" where:
- ID is a decimal integer 1..65535 or C identifier.
- CONTENT is a quoted JSON string or loose text.

You can use C identifiers in place of numeric IDs, if your other resources know how to search for those.
Those names only exist at build time, you can't look them up at run time.

CONTENT is usually loose UTF-8 text.
In that case, it can't contain newlines or leading or trailing space.
Use a quoted JSON string instead if needed.

Each input file contains the game's entire text in one language.

Another convention: ID 1 should be the language's name, in itself.
(I call this out because it means the various ID 1s are not translations of each other as usual).

