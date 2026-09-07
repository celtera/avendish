# Avendish examples

This folder contains multiple examples:

- [Tutorial](Tutorial) is a set of step-by-step tutorials to learn how to write simple audio, data and texture processors with the library, detailed below.
- [Demos](Demos) contains example programs.
- [Ports](Ports) contains more complete examples ported from other libraries.
- [Helpers](Helpers) contains various examples mainly used for testing the helpers library.
- [Raw](Raw) contains various examples mainly used for testing raw plug-ins.

## Simple examples

These examples showcase the basic usage of the API for the most common cases of processors found in
multimedia systems.

- [1 - Hello World](Tutorial/EmptyExample.hpp)
- [2 - Trivial value generator](Tutorial/TrivialGeneratorExample.hpp)
- [3 - Trivial value filter](Tutorial/TrivialFilterExample.hpp)
- [4 - Simple audio effect](Tutorial/AudioEffectExample.hpp)
- [5 - Audio effect with side-chains](Tutorial/AudioSidechainExample.hpp)
- [6 - MIDI Synthesizer](Tutorial/Synth.hpp)
- [7 - Texture Generator](Tutorial/TextureGeneratorExample.hpp)
- [8 - Texture Filter](Tutorial/TextureFilterExample.hpp)

## Advanced features

These examples showcase some more advanced features:

* Sample-accurate control values:
- [9 - Sample-accurate generator](Tutorial/SampleAccurateGeneratorExample.hpp)
- [10 - Sample-accurate filter](Tutorial/SampleAccurateFilterExample.hpp)
- [11 - Porting an effect from Max/MSP: CCC](Ports/LitterPower/CCC.hpp)

* Dynamic multichannel handling:
- [12 - Multichannel distortion](Tutorial/Distortion.hpp)

* All the GUI controls that can currently be used
- [13 - Control Gallery](Tutorial/ControlGallery.hpp)

## Going deeper

* How to write a plug-in without having any dependency at all:
- [14 - Zero-dependency audio effect](Tutorial/ZeroDependencyAudioEffect.hpp)

The content of the "Raw" folder contains a set of examples without using helpers to show how 
every feature of the library works underneath - they are all valid Avendish plug-ins !

## Data utilities

Score registers these alongside its other data-processing objects. Generic
string/file helpers live in the examples; host-value codecs and routing/timing
utilities live in score's plugins.

### Strings

[StringTool.hpp](Advanced/Utilities/StringTool.hpp) provides `ao::StringTool`,
`ao::StringSplit`, and `ao::StringJoin`. Their message inlet triggers processing;
changing a control affects the next message, without replaying old text.

- **String tool:** ASCII trim, optional substring, optional literal replacement,
  ASCII case conversion, reverse, rotation, repeat, left/right padding, then
  Max length truncation and Min length padding, in that order. Lengths, substring
  and rotation indices count Unicode codepoints. Negative substring starts
  count from the end; substring length `-1` takes the remainder. Min length `0`
  disables minimum padding; Max length `-1` disables truncation and `0` allows
  only empty output. Min pad is a separate right-padding codepoint, independent
  of the earlier Pad character. An active maximum below the minimum is an error.
- **Split string:** the delimiter is a complete literal string, not a set of
  separator characters. An empty delimiter splits into codepoints. Keep empty
  preserves boundary and consecutive empty fields; this is not a CSV parser.
- **Join strings:** joins a list of strings with a literal separator.

Text must be valid UTF-8; embedded NULs are preserved. Codepoints are not grapheme
clusters: combining marks and multi-codepoint emoji may separate when reversed.
Case conversion and trimming are ASCII-only; no normalization is performed.
The configurable byte limit is at most 1 MiB, and lists are limited to 65,536
parts. Failures emit Error without partial output; success clears Error before
emitting the result.

**String to Bytes** and **Bytes to String** convert between raw strings and
lists of integer bytes, not Unicode codepoints. For example, `"é"` becomes
`[195, 169]` in UTF-8. Every byte from 0 through 255 round-trips, including NUL
and invalid UTF-8. The reverse conversion rejects floats, booleans, nested lists
and out-of-range integers rather than truncating or coercing them. Max bytes
bounds both input bytes and list elements, up to 1 MiB. Errors produce no
partial output; changing the limit does not replay the previous input.

### Files

[FileIO.hpp](Advanced/Utilities/FileIO.hpp) provides `examples::FileRead`,
`examples::FileReadLine`, and `examples::FileWrite`.

- **Read File:** Read publishes binary-safe Data, Bytes, Lines and EOF. Mode
  selects Whole, Range, Stream or Autostream. Whole reads the complete bounded
  file; Range seeks to Offset and reads up to Count bytes without loading the rest.
- **Read File Line:** Open selects the current Path; Next pulls one line;
  Rewind restarts that same file; Close clears the session. Line excludes LF or
  paired CRLF, but preserves blank lines, bare CR and the final unterminated
  line. EOF is separate from Line: an empty file has no phantom blank line.
- **Write File:** each incoming Data event performs the selected Mode:
  **Write** replaces the entire file, **WriteRange** writes at most Count bytes
  at Offset in an existing regular file, **Append** adds bytes at the end, and
  **Ignore** performs no I/O. Ignore is the default. Range writes preserve bytes
  outside their range and can extend the file. Data is binary-safe; Line ending
  selects None, LF or CRLF before range clipping.

Read and line-reader actions are patchable impulse controls. Autostream needs
no Read trigger. Write File has one event-driven Data inlet, not separate write
triggers: even equal-valued arrivals are new writes. Changing Mode, Path or
other settings never writes or replays old Data. Paths are UTF-8 filesystem
paths, relative to the host's working directory when not absolute; these
objects do not expand score library macros.

Write replaces the file on **every** message. For a stream of chunks use Append,
or assemble a complete message before Write. This data-plus-action design is
also used by [Node-RED's Write File node](https://flowfuse.com/node-red/core-nodes/write-file/).
[A reported serial/XML pitfall](https://discourse.nodered.org/t/reading-serial-xml-data-and-using-the-write-file-node/98777)
illustrates why incoming message boundaries matter.

Offset is a checked nonnegative decimal string representing a 64-bit byte
position, so offsets above the host's int32 range remain exact. Count is
0–64 MiB, defaulting to 65,536 bytes. A zero-count range/stream operation
performs no I/O and reports EOF false. Read ranges may return fewer bytes at
EOF. WriteRange retains Maximum bytes preflight validation of Data plus ending.

On POSIX systems, Stream performs one bounded, non-seeking pull per Read, with a
1–60,000 ms timeout (default 1,000 ms). It supports regular files and nonterminal
character devices such as `/dev/urandom`; FIFOs, sockets and terminals are
rejected. Each request opens and closes its own descriptor: this is not a
persistent stream session. A timeout publishes any received Data and metadata,
sets Timed out, and reports Error without Success. Other platforms report that
stream mode is unsupported rather than falling back to an unbounded read.

Autostream continuously delivers one Count-byte chunk per processing tick when
data is available, without Read impulses. For example, select Autostream,
Path `/dev/urandom`, and Count `6`. Regular files advance rather than replay
their first bytes; EOF publishes any final shorter chunk once and stops.
Character-device pulls remain independent reopen operations.

In Async execution, bounded prefetch fetches up to 32 chunks per batch, at most
64 MiB, with one worker request in flight. Processing consumes one chunk per
tick and refills ahead of exhaustion. Startup or a slow source can leave a tick
without Data; no bytes are invented and this is not a hard timing guarantee.
Sync reads on each tick and may block. Path, Mode, Execution, Count, Timeout and
stop/start changes discard buffered data and invalidate old completions.
Timeouts retain partial bytes for the next complete chunk and retry; other
errors halt Autostream until settings or lifecycle reset.

Execution selects Async or Sync. Async is the default and requires the host's
worker system. All filesystem operations, including status, open, seek and
close, run in `worker.work`.
Its returned completion only stages a result on the processor; `operator()`
publishes callbacks during a processing tick, after the host clears graph ports.
Sync explicitly performs blocking I/O on the caller.

For explicitly triggered operations, Busy admits only one request until its
result is published. Overlapping requests, including writer Data events, are
rejected through Error; they are not queued or silently dropped. Pace further
requests with Success. Data/Line is published before Busy clears and Success
fires. Autostream schedules its own bounded refills instead of reporting idle
ticks as busy errors. A missing async worker reports Error, never a synchronous
fallback. Apart from explicit partial stream results, failures do not publish
successful data; counters retain the last successful values.

Whole-file reads/writes default to 1 MiB, with a 64 MiB hard maximum. Lines default
to 64 KiB, with a 1 MiB hard maximum. Limits reject overflow rather than truncate;
an oversized line leaves its cursor unchanged. The line reader reopens and seeks
for each request, so it never preloads the file or retains an open handle across
ticks or destruction. Keep the file unchanged during a line-reading session.
Whole/range/line operations require regular files. Write success includes closing
the stream, not `fsync` durability; a failed write may have partially modified it.
Already-started async work may finish after object deletion, but cannot call
back into the deleted object.

### Score value serialization

Score registers one **Serialize** / **Deserialize** pair in
`score-plugin-avnd/AvndProcesses/ValueSerialization.hpp`. Format selects JSON,
CBOR, Text or Binary and selects the matching settings pane without a duplicate
tab bar. Preset restoration, incoming Format changes and undo update the pane.
Reusable Avendish tab groups can opt into this with `halp_flag(hide_tabs)` and
`model = &ins::format`; ordinary tab groups keep their visible selector.

Conversions run on incoming value updates, not on settings changes. Serialize
accepts a Value and emits Bytes; Deserialize accepts Bytes and emits a Value.
Wire bytes use binary-safe strings, including embedded NULs.

#### JSON and CBOR

RapidJSON handles strict UTF-8 JSON; Qt Core streams CBOR without an intermediate
DOM. Serialize's JSON pane provides Pretty print. Integers must fit int32;
decimals round to float32, rejecting overflow, non-finite values and
nonzero-to-zero underflow. Null maps to impulse; vectors encode as arrays and
decode as lists. Nested maps/lists and embedded NULs in text are supported.
Duplicate map keys and unset values are rejected.

CBOR uses the same JSON-compatible value subset: byte-string values, tags,
undefined and nontext map keys are not supported. The Bytes port itself still
carries arbitrary binary CBOR wire data.

#### Text

Serialization infers the input shape; there is no String/Scalar/List mode to
choose. Plain text uses:

- Root strings: their UTF-8 bytes unchanged.
- Numbers and booleans: JSON scalar spelling, preserving float32 precision.
- Nonempty flat lists and float vectors: JSON scalar fields joined by Delimiter.
  String fields are quoted and JSON-escaped, so delimiters inside them are safe.
- Empty lists, nested lists and maps: complete JSON syntax.

For example, a `vec2f` containing `1.25` and `2.5` becomes `1.25,2.5` with the
default delimiter. Decode with Auto to obtain a numeric list; plain text has no
tag distinguishing a list from `vec2f`, `vec3f` or `vec4f`.

Deserialize's Interpretation resolves ambiguity: Auto detects JSON, scalar
tokens and delimited lists; String preserves raw UTF-8 bytes, including
numeric-looking strings and line endings; List forces a one-field list.
Integer, Float and Boolean require that scalar type. Line ending selects None,
LF or CRLF for serialization and removes one matching suffix during non-String
decoding. Delimiter is a nonempty literal string; quotes, backslashes and
brackets are reserved. Numeric fields containing or overlapping the delimiter
are rejected. This is not CSV.

Text style **Pretty** uses the same formatter as
`ossia::value_to_pretty_string`: for example `vec2f: [1.25, 2.50]`.
It is a display format, not a lossless archive: floats are printed to two decimal
places. Decoding accepts canonical output supported by ossia's pretty parser;
maps and ambiguous escaped strings are display-only. Use JSON or CBOR when
numeric precision and arbitrary nested structures must round-trip.

#### Binary

Free-flow encoding concatenates int32, float32, one-byte booleans and raw string
bytes, recursively flattening lists/vectors. Maps and impulses are rejected.
Free-flow decoding reads repeated elements of a selected u8/i8/u16/i16/u32/i32/
f32/f64 type into an ordered list. Without a layout, field boundaries and original
mixed types are not recoverable.

Layout mode describes one exact protocol record. It uses a strict subset of
[Python's `struct` format strings](https://docs.python.org/3/library/struct.html),
not a new binary description language. Use the same layout on both ends:
encoding consumes fields in order; decoding returns an ordered list of fields.

| Code | Wire field | Consumes a value |
| --- | --- | --- |
| `b B` | Signed/unsigned 8-bit integer | Yes |
| `h H` | Signed/unsigned 16-bit integer | Yes |
| `i I` | Signed/unsigned 32-bit integer | Yes |
| `q Q` | Signed/unsigned 64-bit integer | Yes |
| `f d` | IEEE float32 / float64 | Yes |
| `?` | Boolean byte | Yes |
| `Ns` | One exact-N-byte raw string | One string, not N values |
| `Nx` | N padding bytes | No |

A count immediately before a numeric or boolean code repeats the field:
`3H` is three unsigned 16-bit integers. Counts are at most 65,536. Strings and
padding require a positive length. Spaces separate fields, not counts from
their codes. Unsupported Python codes are errors.

The optional prefix controls byte order and alignment:

| Prefix | Byte order | Alignment |
| --- | --- | --- |
| `<` | Little-endian | Packed; no implicit padding |
| `>` or `!` | Big-endian / network | Packed; no implicit padding |
| `=` | Native | Packed; no implicit padding |
| `@` | Native | Native C alignment before each field |
| None | Byte order control | Packed; no implicit padding |

Unlike Python's default, omitting the prefix does **not** enable native
alignment. Prefer `<` or `>` for portable files and protocols. `@` is for
interoperability with the current machine's C layout; it is not portable across
ABIs. No automatic padding is added at the end. A zero-repeat numeric code,
such as `0I`, aligns the current offset in native mode without consuming a value.
`x` inserts explicit zero bytes on encode and skips their contents on decode.

For example, on an x86-64 host:

| Layout | Offsets | Total bytes |
| --- | --- | --- |
| `<BI` | Byte at 0, integer at 1 | 5 |
| `@BI` | Byte at 0, integer at 4 | 8 |
| `<B3xI` | Byte at 0, explicit padding at 1–3, integer at 4 | 8 |
| `@IB` | Integer at 0, byte at 4; no tail padding | 5 |
| `@IB0I` | Same fields, then align the end for an integer | 8 |

A complete portable example:

```text
<B H I f 4s 2x
```

encodes `[1, 4660, 16909060, 1.5, "ABCD"]` as exactly 17 bytes:

```text
01 34 12 04 03 02 01 00 00 c0 3f 41 42 43 44 00 00
```

Its offsets are 0 for `B`, 1 for `H`, 3 for `I`, 7 for `f`, 11 for the
four-byte string, and 15 for the two padding bytes. This also matches
`struct.pack("<B H I f 4s 2x", 1, 4660, 16909060, 1.5, b"ABCD")`.

The layout requires exact field counts, string lengths and input byte length.
Unlike Python's `Ns`, strings are never silently truncated or NUL-padded;
provide exactly N bytes. Truncated or trailing input bytes are errors.
Integer wire widths do not widen `ossia::value`: even `q`/`Q` values must fit
int32. Decoded floating fields must fit finite float32 without nonzero-to-zero
underflow. Thus a float64 field is a wire representation, not double-precision
host storage. Layout parsing is cached until Layout or Byte order changes.

Limits are 8 MiB of wire data, 65,536 values/keys and 64 nested containers.
Success/Error accompanies each result; failures clear the result rather than
replay stale data. Parsing and string transformations allocate bounded storage;
they are not allocation-free or hard-real-time DSP operations.

### Fresh-input rendezvous

**Rendezvous** emits an ordered list only after every configured input has
received a fresh event. **Keep first**, immediately after Input count, chooses
the retention policy: enabled keeps the first fresh value on each port; disabled
keeps the latest, which is the default. This includes multiple arrivals within
one processing tick, even at identical timestamps. Equal values still count
as fresh arrivals. Changing retention affects future arrivals without clearing
the partial cycle.

Emitting starts a new cycle, so cached values cannot satisfy the next rendezvous.
Waiting reports the number of missing inputs. Input count ranges from 0 to
1,024. Clear and count changes discard an incomplete cycle; Clear takes
precedence over same-tick arrivals. Zero inputs never emit.

### Switch and editable rows

**Switch** has Input first and Cases second. It routes every incoming event,
preserving timestamps, to the first matching case or Unmatched. Cases are
strict JSON scalars: use `"text"` for a string, an unquoted number or boolean,
and `null` for impulse. Numeric int/float
values compare numerically. Invalid rows and arrays/objects never match;
duplicate cases use the first row.

Switch's Cases and **Prompt composer**'s Keywords share the same editable list
widget in the canvas and inspector. Each row has its own up/down arrows and
delete icon; the bottom **+** adds a row. Actions operate on the clicked row,
without requiring prior selection. Double-click or F2 edits text, Enter commits,
and Escape cancels. Rows carry stable IDs, so renaming and
reordering preserve their dynamic ports, values and cables. Edits, presets and
undo preserve those identities; deleting a row removes its port.

Prompt composer accepts legacy newline-separated keyword values when loading
old data, migrating them to editable rows without losing associated weights.
The widget supports up to 512 rows.

### Value display

Format selects Ordinary, Pretty or Hex and immediately redraws retained log
entries. Hex follows the existing binary value editor: lowercase two-digit bytes,
spaces between bytes, and 16 bytes per line. Strings expose raw bytes, including
NUL and high-bit bytes. Exact numeric values from 0 through 255, booleans, and
flat numeric lists/vectors display one byte per element. Empty strings/lists
show `[empty]`; non-byte values show `[not byte data]` and ordinary text rather
than silently wrapping, truncating or reinterpreting memory.

### Rate limiting and debounce

**Rate Limiter** adds a Mode control. Limit retains the existing rate-limiting
and quantization behavior. Debounce retains the latest incoming value and emits
it after Duration milliseconds of silence, including during ticks with no new
input. Equal-valued arrivals restart the timer, and an arrival exactly at the deadline supersedes
the pending value.

Zero delay forwards each arrival. Debounce ignores quantization and emits on
the first eligible sample. Mode changes, stop/reset and transport discontinuities
discard pending values rather than replaying stale events.