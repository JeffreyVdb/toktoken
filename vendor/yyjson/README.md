# yyjson

- **Version:** 0.12.0
- **Source:** <https://github.com/ibireme/yyjson> (commit ad58f21)
- **License:** MIT (see file LICENSE in this directory)
- **Downloaded:** 2026-03-16

## Files

- `yyjson.c` -- yyjson amalgamation source
- `yyjson.h` -- yyjson public API header
- `LICENSE` -- MIT license text

## Build Flags

Compiled with:

- `-DYYJSON_DISABLE_WRITER=1` -- disables JSON writer (~30% binary reduction)
- `-DYYJSON_DISABLE_UTILS=1` -- disables JSON Pointer/Patch (~10% binary reduction)
- `-DYYJSON_DISABLE_INCR_READER=1` -- disables incremental reader

See `CMakeLists.txt` for the complete list of compile definitions.

## Modifications

None. Files are unmodified from the upstream source.
