# LAT affine index contract

H2-B extends the legacy LLVM producer and the C++ reader with mathematical
integer affine expressions. The JSON structure remains LAT schema version 2.
R2 strict region extraction retains its existing supported index domain.

## Representation

Each GEP index corresponds to one string in `indices`, and the matching
`access_path` index contains the same string. A flattened `8*i+j` is one index;
the two dimensions of `A[i][j]` remain two indices.

The wire grammar is a nonempty sum/difference of decimal integer constants,
identifiers (`[A-Za-z_][A-Za-z0-9_]*`), and `decimal*identifier` terms. A leading
sign is permitted. Whitespace, parentheses, division, remainders and products
of variables are outside this grammar. The producer folds constant products:
`2*i*2`, `2*(i*2)` and `i*(2*2)` in C all serialize as `4*i`.

Canonical output sorts identifiers lexicographically, merges duplicate terms,
removes zero coefficients, omits coefficient 1, and writes the constant last.
Negative unit coefficients use `-i`; the zero expression is `0`.
No LLVM dependency is added to the backend expression module.

Coefficients, constants, slot values and final indices are signed int64.
Reader normalization and substitution check each coefficient/constant multiply and
addition against int64. Evaluation starts with the constant and adds products
in canonical identifier order using checked signed 128-bit intermediates,
then requires an int64 result. Exceeding either domain is unresolved, even if
an alternative evaluation order could cancel the overflow.

Frontend proofs use the original IR integer widths and extension semantics.
An SCEV recurrence `C+d*t` expressed in the emitted IV `I0+s*t` becomes
`(d/s)*IV + C-(d/s)*I0`; the ratio must be integral and all ranges proved.
Nested recurrence starts may contain outer IVs. Scalar formals must have a
proved binding and value-preserving arithmetic/casts. Runtime coefficients,
nonlinear expressions, unproved wrap and scope violations are rejected.
Every recurrence's use site is checked before term merging; identically named
sibling loops cannot hide an escaped IV through normalization.
Only integral casts (`trunc`, `zext`, `sext`) participate in this proof;
pointer-to-integer SCEVs are rejected with an unsupported-index diagnostic.

## Evaluation and failures

Exact lexical variable names retain priority over parsing for legacy LAT,
including unusual names such as `i+1`. Legacy integer spelling and single-IV
offset parsing retain their existing behavior. New expressions bind every
nonzero variable term to the nearest lexical slot at the first reached access.
No body preparation or new expression failure is observed in zero-trip loops.

Inline actuals are substituted simultaneously as expressions, not by textual
concatenation: `2*x+1` with `x=i+1` is `2*i+3`. Failed expression composition
is deferred as parenthesized original text, without raising a new eager error
in a skipped body. Producer-generated names keep this failure unresolved at
the eventual access.

The producer separates loop names across inline functions. The reader does
not rename external LAT variables to prevent capture. Arbitrary legacy exact
names can collide with unmapped identifiers or deferred failure text such as
`(2*x+1)`; capture prevention does not cover those external inputs.

String traces retain original fallback spelling for unsupported/unresolved
expressions. Unsupported expressions inside display-name brackets also retain
the original callee spelling during inline substitution: `A[x/2]` stays
`A[x/2]` for both `x=i` and `x=j`. This can merge previously distinct symbolic
element keys; these unresolved traces are not valid address evaluations.
Linked-address consumers reject unresolved indices using the existing source
charge, ordinal, coverage and diagnostic ordering. Every index is checked on
every access, even when the layout has already been prepared. Existing object,
offset, address, work-budget and callback contracts remain applicable.

## Reader compatibility

| Input | H2-A C++ reader | H2-B C++ reader |
| --- | --- | --- |
| Legacy function arrays / versionless object roots | Existing behavior | Preserved |
| LAT v2 constants and IV offsets | Supported | Preserved |
| LAT v2 affine strings | Symbolic fallback in string traces; linked addresses rejected | Evaluated |
| Explicit unknown or malformed schema version | Version ignored | Rejected at normalization |

The old reader was tested with `i+1`, `2*i`, `4*i`, and `8*i+j` using both
version 2 and version 3 roots. Incrementing the version does not make that
reader reject symbolic string traces. New affine LAT therefore requires an
H2-B-capable reader; old string/profile output is not a valid evaluation of
these expressions. The frontend and backend must be upgraded together for
new inputs. LAT generated before H2-A may have already lost coefficients and
must be regenerated from the original C/IR; a reader cannot reconstruct them.
