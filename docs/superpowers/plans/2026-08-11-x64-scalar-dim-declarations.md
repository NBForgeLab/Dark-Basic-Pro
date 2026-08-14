# Wave 10 — Scalar typed DIM declarations: plan

- [x] Explore: `dim d as float` fails (ERR_SYNTAX+43 in pre-scan); array
      form and `global`/`local` forms work; isolated probes confirmed
      `dim dd` (plain) also fails — scalar DIM was entirely broken.
- [x] Root cause: `DoDeclaration`'s `Token::Dim` branch always requires
      array brackets; `ProduceNextArrayToken` returns NULL (pointer
      untouched) for scalar names → `SeperateValueFromArrayString` fails.
- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-scalar-dim-declarations-design.md`
- [x] RED: `tests/test_x64_scalar_declarations.cpp` — 8 tests: scalar typed
      DIM compiles end to end (float/int/string, with init, plain `dim d`,
      multi-declaration, array regression via `FF D3` CALL RBX, emitted
      inline `d=1.5` store with no DLL call). 7/7 scalar tests failed red.
- [x] GREEN: fall back to the plain-variable-token path in the
      `Token::Dim` branch of `DoDeclaration` when bracket separation fails.
      8/8 green; full suite 1012/0/1.
- [x] Full build clean + ctest 100% + docs/17 + design doc TDD notes.
