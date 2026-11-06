# Chunk file v1

This experimental API is declared in `cheryl/core/resources/fileio/chunk-file.h`,
in namespace `CE::Files::Chunks`. It has no legacy-format compatibility.

## Wire layout

All offsets and lengths count bytes. Integers are unsigned, fixed width and
little-endian. FourCC identifiers are four literal bytes, independent of host
integer representation. The 16-byte envelope is:

| Offset | Size | Required value |
| --- | --- | --- |
| 0 | 8 | `43 48 4e 4b 0d 0a 1a 0a` (`CHNK`, CR, LF, SUB, LF) |
| 8 | 2 | Version 1 |
| 10 | 2 | Flags 0; all bits reserved |
| 12 | 4 | Envelope size 16 |

Every envelope byte must match. After the envelope, the document contains zero
or more complete top-level records. Each record has a 16-byte header:

| Relative offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | Structural FourCC: `CONT` or `DATA` |
| 4 | 4 | Application FourCC: any four bytes |
| 8 | 8 | Payload length, excluding this header |

`CONT` payloads contain ordered complete child records. `DATA` payloads are
opaque bytes, including zeros; their contents are never scanned for children.
Unknown structural categories fail. Unknown, repeated and nonprintable
application tags are accepted and retain order. Empty containers and leaves
have length zero. There is no padding or alignment. Every record must fit its
parent (or the entire input for top-level records); partial headers, partial
payloads and trailing garbage fail. A 16-byte envelope alone is valid; zero
bytes are invalid. Concatenated complete records are additional top-level chunks.

## Limits and validation

`Limits` defaults to `max_bytes = 67108864`,
`max_allocation_bytes = 134217728`, `max_chunks = 1000000`, and `max_depth = 64`.
Top-level chunks have depth 1. Every chunk, including an empty container, counts
once and consumes one depth level. Depth zero permits only an empty document.
A configured depth above the implementation ceiling of 256 is rejected even
for an empty document.

The parser checks the byte/allocation budgets before copying or reading input,
and validates memory input before copying it. File/stream input is read into a
bounded buffer before structural validation. Record lengths use checked uint64
addition, addressable vector limits and parent bounds before traversal. Parser
and writer use fixed-size traversal stacks and allocate no per-chunk index.
The writer preflights the entire supplied tree before allocating output or
opening/truncating a file. Invalid DATA-with-children, CONT-with-data and invalid
category values fail.

The allocation limit is a charged-storage budget, not a process-memory ceiling.
Encoding charges the complete output byte count. Decoding/reading charges the
input byte count plus `sizeof(Detail::Storage) + 256` for backing storage and
shared-ownership overhead. Allocator bookkeeping/capacity rounding, stack space,
I/O library buffers, exception storage and caller-owned input/value trees are
outside this accounting. Caller construction, copying and destruction of a
recursive `Chunk` tree are not bounded by codec limits. Allocation failure can
still propagate a standard allocation exception.

## API ownership and traversal

`Chunk::leaf(type, bytes)` and `Chunk::container(type, children)` construct mutable
owning values. Copies own independent vectors; moves transfer them. `encode`
returns an owning `Bytes` vector. `decode` copies its input into immutable shared
storage; the original input may then be changed or destroyed.

`Document`, `ChunkView`, `ChunkRange` and its iterators pin that shared storage.
A retained view, range or iterator survives destruction or reassignment of its
originating document. `Document::bytes()` and `ChunkView::payload()` return
borrowed spans: keep a storage-owning handle alive while using them. A span alone
does not extend storage lifetime. A container payload span includes serialized
child records. A leaf's `children()` range is empty.

`chunks()` and `children()` traverse immediate records in order. `size()`,
`at(index)` and `find(type, occurrence)` scan linearly; occurrence is zero-based
and includes duplicate application tags. Missing indices/tags and dereferencing
or advancing the end iterator throw `std::out_of_range`. Use moved-from handles
only for destruction or reassignment; moved-from documents also expose empty
`bytes()` and `chunks()`.

## Errors and I/O

`Error` derives from the engine's `Exceptions::runtime_exception` and exposes
`ErrorCode::{format, limit, io}` and a uint64 byte `offset()`. Envelope failures
identify the first mismatching byte; incomplete headers identify their start;
invalid lengths identify the length field; budget-option failures generally
report zero. Offsets are relative to the document or to the starting stream
position, not necessarily the underlying file's absolute position.

`read(stream, byte_count)` consumes exactly that many bytes and leaves subsequent
bytes unread. It neither closes the caller's stream nor accepts short reads as
normal EOF. `write(stream, chunks)` encodes, writes and flushes, but does not close
the caller's stream. Both transfer blocks of at most 1 MiB. Read errors report
bytes received; write errors report the start of the failing block because an
output stream does not expose its exact partial count. Flush errors report the
complete encoded length.

`read_file(path)` opens binary input, measures and bounds its length, reads and
validates it, checks for growth/final read failure, and explicitly checks close.
It does not promise a snapshot against concurrent same-size modification.
`write_file(path, chunks)` encodes before opening binary truncating output,
flushes, then explicitly checks close. Open failures report offset zero; close
failures report total length. Detectable failures throw. A failed write may
leave a partial or truncated file: replacement is not atomic and there is no
crash-durability guarantee.

## Independent 51-byte fixture

These literal bytes specify a reader fixture independently of `encode`. The
writer fixture independently constructs the corresponding tree and compares all
51 output bytes.

```text
43 48 4e 4b 0d 0a 1a 0a 01 00 00 00 10 00 00 00
43 4f 4e 54 52 4f 4f 54 13 00 00 00 00 00 00 00
44 41 54 41 54 45 53 54 03 00 00 00 00 00 00 00
00 41 00
```

Offset 0 is the envelope. Offset 16 is `CONT/ROOT`, payload length 19.
Offset 32 is its single `DATA/TEST` child, payload length 3. Offset 48 holds
`00 41 00`. Both records and the document end at offset 51.

## Focused build and example

Use an absolute checkout root below. The subshell preserves the caller's working
directory. A matching existing configuration can be reused. This standalone
harness compiles the production implementation and exception source, with
vendored GoogleTest; it does not validate the complete engine or optional graphics
and audio dependencies. C++23, CMake 3.28 or later and Ninja are required.

```sh
(
    cd /absolute/path/to/cheryl-checkout || exit
    cmake -S projects/engine/tests/chunk-file -B build/chunk-file -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
        -DCHERYL_CHUNK_SANITIZERS=OFF
    cmake --build build/chunk-file --target cheryl_chunk_file_tests cheryl_chunk_file_example || exit
    ctest --test-dir build/chunk-file --output-on-failure -R '^chunk_file(_example)?$'
)
```

For an available GCC/Clang sanitizer configuration, use a separate build directory
and `-DCHERYL_CHUNK_SANITIZERS=ON`; execute the same explicit targets and tests.
Agent-run builds use `--parallel 1` under their assigned resource policy; ordinary
user commands above retain available parallelism. The example creates a unique
temporary directory, writes/reloads a real file, traverses ROOT/TEST, checks exact
payload bytes and removes the directory. Its success message is
`Reloaded 51 bytes: CONT/ROOT -> DATA/TEST = 00 41 00`.

The focused tests cover literal reader/writer fixtures, ordinary ordering and
tags, malformed envelopes/lengths/truncation, limits, value/view lifetime and
injected I/O failures. Close-failure injection exercises the shared checked-close
helper rather than requiring an actual filesystem close failure. Sanitizer,
full-engine and independent-review acceptance are separate checks; the existence
of this specification does not establish them or cross-platform acceptance.
