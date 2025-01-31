# Eggdev HTTP Server

Use `eggdev serve` to stand an HTTP server for editing data files or running ROMs.
At its most basic, it's just an HTTP server serving arbitrary files.
You should point it to the editor's or runtime's htdocs.

If you supply more than one `--htdocs`, we search in reverse order.
This is important for the editor: You can do `eggdev serve --htdocs=../egg/src/editor --htdocs=src/my-editor-overrides`
to selectively replace parts of the editor for your own data types.

Each `--htdocs` is a prefix that maps to the root of the request filesystem.
So with `--htdocs=src/www`, if one requests `GET /abc/index.html`, we serve `src/www/abc/index.html`.

You may repeat one `--htdocs` argument as `--write=PATH` to allow PUT and DELETE under that directory.
Only this one directory will accept PUT and DELETE of static files.
You should specify it as both `--htdocs` and `--write` to enable both read and write, can't imagine why you wouldn't want it readable.

`/index.html` is special only at the very root. A request for `GET /` becomes `GET /index.html`.

`GET` for a directory returns a JSON array of basenames.

The server is largely synchronous and doesn't cache anything.
Performance-wise, this would be unacceptable in a multi-user environment.
We assume that there's just one user at a time of the server, and it's fine to block while serving any request.

Similarly: We do almost nothing for security!
It's important that you not run this dev server where the public internet can reach it.
By default, we serve only on localhost, so that's cool.

## REST API

We serve a few things that are not static files, and their paths always begin `/api/`.
You can not override these.

`GET /api/make/...`

Same as `GET /...`, but we first run `make` in the working directory (where `eggdev` was launched).
If make fails, we return status 599 with make's output as the body.
This is intended for ROM files, so you can automatically build before launching via web app.
Note that we run just `make`, not specifying the file.

`GET /api/resources/...`

Read a directory recursively and return every regular file under it.
Intended for the initial load from the editor.
This could be done with individual calls but that would be wildly inefficient.
Beware that overrides will not work; once we find the root of the request, there's no more looking things up.
Returns `{ path, serial }[]`, no nesting. `serial` is encoded base64. `path` does not include the common root directory.

`POST /api/sound?position=ms&repeat=0|1`

Request body is a MIDI or WAV file, or empty to silence playback.
Fails 501 if the server launched without an audio driver. You can call with an empty body to test availability.
Add `?unavailableStatus=299` or any 3-digit number for a different status (so browsers don't log it as an error).
Only one thing can play at a time.
TODO Can we get playhead feedback over a WebSocket?

`POST /api/synth?rate=HZ`

Request body is any audio resource.
Compile the resource, stand a synthesizer, print PCM, and return a WAV file.
This does not require a server-side audio driver.

`POST /api/compile?srcfmt=NAME&dstfmt=NAME`

Run one resource through the standard compiler or uncompiler.

`GET /api/gamehtml`

Returns the path for the game's final HTML output, as provided via `--gamehtml=PATH`.
For launching game in the editor.

`GET /api/symbols`

Returns the entire digested content of namespaces declared with `--schema=PATH` arguments.
This lets the editor share symbols with your C code.
```
{
  ns: {
    name: string;
    mode: "NS" | "CMD";
    sym: {
      name: string;
      id: number;
      args?: string;
    }[];
  }[];
}
```
