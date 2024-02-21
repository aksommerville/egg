# Egg Game Metadata Format

We allow the developer to embed lots of loosely structured data to aid indexing and review.
You might have some organized games database that could auto-import this data.

A few things are also relevant technically at runtime.

## Input format.

Our tooling should generate the details file from:

- `src/data/details`: Key=value text, like a Java "properties" file.
- `src/data/icon.ico`: Icons, to copy verbatim.

In the text file, lines beginning '#' are ignored, as are blanks.

## Binary format.

A loose collection of game metadata is stored in resource "details".

Composed of binary key=value pairs:

```
u8  Key length
... Key
vlq Value length
... Value
```

vlq should work as it does in MIDI:
- Sequence runs to the first byte with its high bit clear, limit 4 bytes.
- Strip the high byte and combine each 7-bit word big-endianly.
- Maximum value 0x0fffffff.

Keys defined by Egg will always begin with a lowercase letter, and will always be C identifiers.
I recommend *breaking* that convention for custom keys, to avoid collision.
(but why are you defining custom keys? I don't imagine there will be a use for them).

Text values for all standard keys should be UTF-8.

| KEY              | VALUE |
|------------------|-------|
| title            | Game title for display. |
| author           | Author's name. |
| copyright        | Copyright notice for display, eg "(c) 2024 AK Sommerville" |
| website          | URL of the project page or author's home page or whatever. |
| contact          | Email, phone, or whatever, may show to user as a support contact. |
| version          | Version of this game, for display. Recommend the common "vA.B.C". |
| date             | ISO 8601, publication date. "YYYY-MM-DD" |
| source           | URL of source code repo. |
| language         | Comma-delimited list of ISO 631 language names. Could know this from reading the TOC, but be a pal and say it up front too. |
| rating           | If you are rated by some agency like ESRB, call it out here. |
| contentAdvisory  | Official or not, if the game has nudity, profanity, excessive violence, call it out here. Freeform text. |
| genre            | Short answer, what kind of game is this? (see below) |
| multiplayer      | Recommend: "Single player", "Local multiplayer", "Online multiplayer", "Online and local multiplayer". |
| tags             | Extra loose tags for indexing. Comma-delimited. (see below) |
|--------------------------|
| required         | Space-delimited feature list that the game can't work without (see below). |
| optional         | Same as `required`, but game is able to run without if necessary. |
| framebuffer      | "WxD", decimal integers. Preferred window size. Platform might use only the aspect ratio, and guarantees nothing. |
| storageSize      | Bytes, decimal. How much persistent storage you expect to need, in case the user imposes a quota or something. |
| thumbnail        | Path of an image resource in this archive, which is intended for previews. Can include multiple, eg different sizes. |
|--------------------------|
| icon             | Binary, icon for window manager etc. (see below) |

## Tokens for required and optional

These do not do anything at runtime, like enabling or disabling features.
But a host may decline to launch or may deprioritize in searches, if a feature is not available.
Be nice to your users, tell them what you expect technically.

- `store`: Persistent storage.
- `http`
- `websocket`
- `text`: Requires text input (EGG_EVENT_TYPE_TEXT). All hosts should emulate if needed.
- `keyboard`: Requires real-time on and off events from a keyboard. Only real keyboards will work.
- `touch`: Requires point-and-click input (EGG_EVENT_TYPE_MBUTTON). Touchscreens can emulate neatly.
- `mouse`: Requires passive mouse position. Joysticks might emulate badly, and touchscreens not at all.
- `joystick`: A keyboard can emulate, and touchscreens maybe.
- `accelerometer`: Most hosts won't have one, and we can't emulate.
- `audio`: Rhythm games should call it required; game can't be played without.

## Icon format

Should be in MS Windows Icon format, and should contain 16x16 and 32x32, and whatever other sizes you feel like.
We might use this as favicon on the web, or as the window manager icon on a desktop.

## Genre

I recommend using strings from Itch.io, just so we're all on the same page.

On 2024-02-15, Itch's quickie genre list:

- Action
- Adventure
- Card Game
- Educational
- Fighting
- Interactive Fiction
- Platformer
- Puzzle
- Racing
- Rhythm
- Role Playing
- Shooter
- Simulation
- Sports
- Strategy
- Survival
- Visual Novel
- Other

## Tags

Like genre, try to use tags that are already popular on Itch.io.
Nothing special about Itch here, just we need some kind of source of truth to keep in sync.
And of course, you can make things up here.

Any tooling that reads tags should trim leading and trailing space, and treat letters case-insensitively.
