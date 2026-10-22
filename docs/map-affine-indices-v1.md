# MAP affine index contract

H2-B extends the legacy LLVM producer and the C++ reader with mathematical
integer affine expressions. The JSON structure remains MAP schema version 2.
R2 strict region extraction shares the pointer and iteration-counter extensions
below while retaining its other index restrictions.

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
`(d/s)*IV + C-(d/s)*I0`; this representation requires an integral ratio.
When a constant-endpoint, nonunit-step loop has a secondary PHI requiring a
nonintegral ratio or an affine pointer PHI, the producer instead emits a unit
iteration counter `t` with `start: 0`, `step: 1`, and `bound: trip_count`.
Every recurrence in that loop, including the original controlling IV, then
uses `C+d*t`. Original integer-width, endpoint and overflow proofs remain in
force. Outer-affine endpoints are not normalized this way. Integral loops
without such a PHI retain their existing representation.
Nested recurrence starts may contain outer IVs. Scalar formals must have a
proved binding and value-preserving arithmetic/casts. Runtime coefficients,
nonlinear expressions, unproved wrap and scope violations are rejected.
Every recurrence's use site is checked before term merging; identically named
sibling loops cannot hide an escaped IV through normalization.
Only integral casts (`trunc`, `zext`, `sext`) participate in this proof;
pointer-to-integer SCEVs are rejected with an unsupported-index diagnostic.

Header-tested `!=` loops are accepted when their endpoints are constant and
the nonzero step reaches the endpoint exactly without signed wrapping. A
zero-trip loop is permitted; unreachable endpoints and nonconstant endpoint
ranges are rejected.

Pointer PHIs such as `*p++` may reference one registered homogeneous scalar
array or scalar object (including an inline pointer formal). The producer
separates the storage base from the cumulative byte offset, proves that offset
as an affine expression, and converts it exactly into scalar element indices.
Initial offsets, negative strides and GEP offsets after the PHI are preserved.
Nested pointer carry is also supported when an outer header PHI receives the
final header PHI of its immediate child loop. Both loops must have a single
header exit, and the child's constant pointer stride and exact constant trip
count must be proved. The checked product gives the outer byte stride; existing
affine and layout checks still apply. Conditional carries, extra exits and
pointer-index overflow are rejected. This does not cover arbitrary nested
pointer updates, sibling-loop carry or floating-point loop counters.
An outer nonunit loop whose pointer carry is only discovered by nested rewriting
may still require a nonintegral emitted-index coefficient; that case is rejected
rather than normalized speculatively.
Ambiguous bases, data-dependent strides and incompatible element layouts are
rejected. ELF consumers still require the final object to bind to a global
symbol; this does not assign synthetic addresses to stack objects or pointers.
The strict region builder shares the unit-counter coordinate when selected,
while retaining its existing restrictions on other scaled index expressions.
When a secondary PHI selects that coordinate, ordinary scaled GEPs in the same
loop also use the normalized affine proof. Acceptance can therefore depend on
whether that loop needs normalization, not solely on the GEP's spelling.
Pointer recurrences have their own affine byte-offset proof: `*p; p += 2` may
therefore pass strict extraction even where an ordinary scaled GEP is rejected.
The strict pointer-cast ban also applies throughout a recurrence's GEP/PHI chain.
Cast validation precedes index validation, so a cast combined with a scaled
index reports the pointer-cast diagnostic first.

Scalar nodes serialize the memory instruction's actual store size as
`elem_size`, independent of the declared object or pointer-formal type. Field-free
Array nodes serialize their element stride; it must also
equal the memory instruction's store size. Padded element types for which these
sizes differ are rejected. Nodes containing field paths omit `elem_size`:
object metadata provides root layout and field descriptors provide leaf widths.
The structured reader ignores any legacy node-level `elem_size`. Ordinary GEP chains containing
pointer casts on identifiable storage are rejected because casts may alter row
strides independently of scalar element size. Runtime allocations keep the
unresolved-storage path for either cast/GEP ordering. A unique call/load base
in a pointer recurrence also uses that path after its affine offset is proved;
ambiguous PHIs and data-dependent strides are still rejected. Legacy extraction
preserves each unaddressable access without an object ID, using a Scalar
placeholder when no valid Array layout can be represented. Its display name
and memory width do not prove a relative offset or physical address. Strict
regions reject these addresses; ELF consumers reject them before display-name
lookup, including when the name happens to match a global symbol.

Reader normalization and inline substitution share the same missing-only width
fallback. Inline Array binding requires compatible known element sizes and row
dimensions (the trailing dimensions used by the indices); structured paths
instead validate the root type/layout. Compatible actual shapes replace formal
extents, while explicit access widths remain unchanged. Legacy inputs without
formal layout metadata retain their existing object-metadata fallback.
Known Array binding incompatibilities are structural errors checked eagerly
during call expansion, including calls inside zero-trip loops. Their diagnostics
include the formal canonical ID (and therefore callee) and actual object ID.
This check is distinct from the deferred expression evaluation below.

## Evaluation and failures

Exact lexical variable names retain priority over parsing for legacy MAP,
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
not rename external MAP variables to prevent capture. Arbitrary legacy exact
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
| MAP v2 constants and IV offsets | Supported | Preserved |
| MAP v2 affine strings | Symbolic fallback in string traces; linked addresses rejected | Evaluated |
| Old MAP v2 Scalar nodes without explicit access width | Declared metadata fallback | Regenerate before relying on inline Scalar addresses/widths |
| Explicit unknown or malformed schema version | Version ignored | Rejected at normalization |

The old reader was tested with `i+1`, `2*i`, `4*i`, and `8*i+j` using both
version 2 and version 3 roots. Incrementing the version does not make that
reader reject symbolic string traces. New affine MAP therefore requires an
H2-B-capable reader; old string/profile output is not a valid evaluation of
these expressions. The frontend and backend must be upgraded together for
new inputs. MAP generated before H2-A may have already lost coefficients and
must be regenerated from the original C/IR; a reader cannot reconstruct them.
The same requirement applies to older v2 Scalar nodes lacking `elem_size`:
declared formal metadata cannot recover the width of a cast load/store. The
legacy fallback remains for compatibility, but is not evidence of the actual
memory instruction width. Regenerate such MAP from C/IR with the current
frontend before using inline Scalar address results.
