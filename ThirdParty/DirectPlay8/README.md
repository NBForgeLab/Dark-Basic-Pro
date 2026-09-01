# DirectPlay8 headers (vendored)

Authentic DirectX 8.0 SDK headers `dplay8.h` and `dpaddr.h`, vendored because
DirectPlay was removed from the modern Windows SDK and the legacy June 2010
DirectX SDK is not installed on build machines.

Provenance: copied from the MinGW dx80 SDK mirror
(https://github.com/google/sagetv/tree/master/third_party/mingw/dx80/include),
which carries the original Microsoft headers. Only `dplay8.h` and `dpaddr.h`
are needed by the MultiplayerPlus plugin; they are self-contained (require
only `ole2.h` plus each other).

No local modifications to the header contents. `dvoice.h`/`dxerr8.h` from the
same mirror were deliberately not vendored: the voice code path is commented
out upstream and `dxerr9` had no remaining usage.

At runtime DirectPlay8 is provided by `dpnet.dll`, which still ships with
Windows 10/11, so no extra runtime component is required.
