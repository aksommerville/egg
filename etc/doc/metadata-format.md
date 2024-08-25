# Egg Metadata Format

Every ROM file must contain a resource metadata:1.
Because metadata is type 1 and ROM contents are sorted, it must be the first resource.
That's very much by design.

Metadata consists of key=value pairs, all strings limited to 255 bytes of ASCII.
The empty key is not allowed.

## Binary Format

Starts with 4-byte signature: "\0EM\xff"
Followed by zero or more of:
```
  u8 Key length.
  u8 Value length.
  ... Key.
  ... Value.
```
Followed by a single zero byte to terminate.

## Text Format

Line-oriented text.
'#' starts a line comment, start of line only.
'=' separates key and value.
Leading and trailing whitespace are trimmed from both key and value.

## Standard Fields

All fields meaningful to Egg will begin with a lowercase letter and will never contain a dot.
Games are free to add their own fields for whatever reason, but please start with an uppercase letter or use reverse-DNS.

Plain-text fields may have an alternate version with '$' appended.
The value of the '$' field is the index of a string in `strings:1` to substitute for display, in the user's language.
This is for translation, long text, and text outside ASCII.
eg metadata may contain "title=MyGame" and also "title$=3", then `strings:en:1` at index 3 also contains "MyGame" and `strings:fr:1` at index 3 contains "MonJeu", etc.

If you don't provide `fb`, you'll get a default of "640x360".

| Key           | Description |
|---------------|-------------|
| fb            | `WIDTHxHEIGHT`. Dimensions of texture 1, the main video output. |
| audioRate     | Preferred audio rate in Hz, if you're using a custom synthesizer. 200..200000 |
| audioChanc    | Preferred channel count, if you're using a custom synthesizer. 1..8 |
| title         | Full title of this game. |
| author        | Your name. |
| desc          | Brief description, for prospective users' consumption. |
| lang          | Comma-delimited list of ISO 639 language codes, in order of your preference. |
| freedom       | See below. |
| required      | Comma-delimited list of features, see below. Runtime fails if it can't satisfy one. |
| optional      | Same as `required` but not fatal. |
| players       | `N` or `N..N`, how many can play at once. |
| copyright     | Short legal text eg "(c) 2024 AK Sommerville" |
| advisory      | Brief message detailing potentially objectionable content. |
| rating        | Machine-readable declaration of ratings from official agencies. TODO format |
| genre         | Single answer, what kind of game is it? Recommend using strings popular on Itch.io, so we have a common reference. |
| tags          | Comma-delimited list of arbitrary tags. Like `genre`, try to use ones that have been seen on Itch.io. |
| time          | When was this build released? ISO 8601, ie "YYYY-MM-DDTHH:MM:SS.fff" but stop anywhere, usually after year or day. |
| version       | Version of your game. Recommend "vN.N.N" |
| persistKey    | If present, we'll require it to match for the key-value store. Use some random nonce, or the version string. |
| iconImage     | ID of image resource. Recommend PNG 16x16. |
| posterImage   | ID of image resource. Recommend PNG with aspect 2:1 with no text. |

AK: If you change this list, remember to update src/editor/js/MetadataEditor.js

`freedom`: Declare whether users are allowed to copy and modify this game.
- `restricted`: Assume we're not even allowed to play it, unless the license says so.
- `limited` (default): Assume we're allowed to play but not redistribute.
- `intact`: OK to redistribute unmodified. Assume you're not allowed to borrow assets.
- `free`: Assume assets may be borrowed with no more than attribution to the author.

`required`, `optional`
- `gl`: Require direct rendering; Egg's video API will not be available.
- `keyboard`
- `mouse`
- `touch`
- `accelerometer`
- `gamepad`
- `gamepad(BTNMASK)`: BTNMASK is one bit per button, see egg.h:EGG_BTN_*
- `audio`: eg for rhythm games, don't let it start up when audio is unavailable

`rating`
TODO describe format
