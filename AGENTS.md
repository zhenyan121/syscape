# Syscape Repository Instructions

## Project Mission

Syscape is an MIT-licensed, zero-dependency, header-only C++ library for
querying information exposed by the execution platform. The Freestanding
Minimal foundation targets strict C++11, while Hosted Full targets strict
C++17. The long-term goal is to cover every useful category that each declared
compatibility profile can honestly expose, from hosted operating systems to
constrained RTOS and bare-metal targets.

These instructions apply to the entire repository. Follow them for every
change unless a more specific `AGENTS.md` in a subdirectory adds stricter
requirements.

## Non-Negotiable Constraints

- Target strict C++11 for Freestanding Minimal and strict C++17 for Hosted Full
  as defined in `docs/support-matrix.md`. RTOS/Constrained headers must declare
  their exact minimum language and library requirements, never below C++11.
- Use only the language and standard-library facilities guaranteed by a
  header's declared profile and documented APIs supplied by the target
  operating system, RTOS, board support package, or platform SDK.
- Do not add third-party library dependencies anywhere, including tests,
  examples, documentation tooling, and the build process.
- Do not use compiler language extensions, compiler builtins, non-standard
  language modes, or unnecessary vendor pragmas.
- Keep all shipped library implementation code header-only. Do not add `.cpp`
  files to the library itself.
- Write all repository documentation, code comments, API documentation,
  examples, diagnostics, and test descriptions in English.
- Preserve the MIT license and do not add incompatible licensed material.

Implementation-defined preprocessor macros may be used only to identify a
compiler, operating system, architecture, standard-library facility, or SDK
availability. Their use does not permit compiler-specific language features.
Platform APIs must be included and called only in the branch for the platform
that provides them.

## Repository and Header Layout

- Place public library headers under `include/syscape/`.
- Every library source file must use the `.hpp` extension. Tests may use
  `.cpp`; build files and documentation may use their conventional names and
  extensions.
- Split public headers by coherent information domain, such as CPU, memory,
  operating system, process, network, user, storage, power, display, or
  environment information.
- Keep platform-specific helpers and other implementation details under
  `include/syscape/detail/`. Applications must not depend on anything in that
  directory.
- Prefer focused headers with one clear responsibility. Do not accumulate
  unrelated queries in a monolithic header.
- Do not provide an umbrella `syscape.hpp` or another header that includes all
  modules. Users must be able to include only the domains they use.
- Every public header must be self-contained: it must include everything it
  needs and compile when it is the first project header in a translation unit
  that satisfies its declared compatibility profile.
- Every public header must document its minimum language version,
  compatibility profile, and any required standard-library, RTOS, SDK,
  permission, or hardware facilities.
- Use conventional, unique include guards. Do not use `#pragma once`.
- Include only what a header uses and avoid exposing platform SDK headers or
  macros through the public interface when an internal boundary can contain
  them.

## Public API Design

- Put every public symbol in `namespace syscape` or a domain namespace nested
  directly beneath it, for example `syscape::cpu` or `syscape::memory`.
- Follow standard-library conventions: lowercase `snake_case` identifiers,
  value semantics, explicit ownership, predictable overloads, and the smallest
  interface that completely represents the information.
- Do not expose operating-system handles, SDK structures, platform-sized text
  types, platform macros, or other native implementation details in public
  signatures.
- In Hosted Full interfaces, prefer standard C++ types and containers. In
  Freestanding Minimal interfaces, use allocation-free enums, scalar values,
  compile-time constants, or explicit caller-owned storage. Return structured
  values instead of encoded delimiter-separated strings or loosely related
  output parameters.
- State the unit in the name, type, or API documentation for every numeric
  measurement whose unit is not inherent in its type.
- Hosted Full public query operations must not use exceptions to report
  expected lookup or platform failures. They must return
  `syscape::result<T>`.
- `syscape::result<T>` carries either a value or a `std::error_code`. It must
  provide an expected-like, standard-library-style interface suitable for
  explicit error handling in C++17.
- Define library-wide portable conditions in `syscape::errc` and expose them
  through a custom `std::error_category`. Preserve a native system error code
  and category when that provides more accurate diagnostic information.
- Do not silently replace failed, missing, or unsupported information with a
  plausible value. In particular, do not treat zero, an empty string, or an
  empty collection as an error sentinel when it can also be valid data.
- Distinguish at least these conditions when the platform can distinguish
  them: unsupported capability, permission denied, data not found, temporarily
  unavailable data, malformed platform data, and native I/O or system failure.
- Hosted Full public text is UTF-8 stored in `std::string`. Convert from native
  encodings at the platform boundary, including conversion from UTF-16 on
  Windows. Report a conversion failure; do not return corrupted or silently
  substituted text. Freestanding Minimal headers must not require
  `std::string`, `std::error_code`, dynamic allocation, exceptions, or RTTI.
- Compile-target facts in the minimal profile use explicit enum values such as
  `unknown` when no recognized value can be established. An explicit unknown
  category is data, not a fabricated platform value or error sentinel.
- Public interfaces must remain source-compatible when support for another
  platform is added. Do not redesign a portable interface around one platform's
  representation.

Do not introduce a public API merely because one platform exposes a field.
First define portable semantics, ownership, units, error behavior, and lifetime.
If no honest portable abstraction exists, keep the functionality in a clearly
named platform-specific domain beneath `syscape` without leaking native types.

## Header-Only and Runtime Behavior

- Every non-template function and variable definition in a shipped header must
  be ODR-safe. Use `inline`, `constexpr`, templates, or another mechanism from
  the header's declared C++ standard as appropriate.
- Do not rely on a separately compiled library, generated source file, or
  hidden link dependency.
- Avoid mutable global state, global constructors, implicit background work,
  telemetry, and process-wide configuration changes.
- Do not cache platform information unless the API documents the cache
  lifetime, invalidation behavior, and thread-safety contract. Prefer querying
  on demand when correctness can change during the process lifetime.
- Public queries must be safe for concurrent calls unless unavoidable platform
  behavior is explicitly documented.
- Release every native resource on every path. Wrap owned resources in small
  internal RAII types where practical.
- Handle partial reads, interrupted calls, size races, integer conversion,
  arithmetic overflow, truncation, and changing system state explicitly.

## Compatibility Profiles and Platform Backends

Portability is defined within an explicit profile. It does not mean requiring a
bare-metal target to provide operating-system information or weakening the
Hosted Full API to the smallest freestanding environment.

- **Hosted Full:** provides the complete hosted C++17 standard library and the
  full portable query API based on `result<T>`, `std::error_code`, and UTF-8
  `std::string` values.
- **Sandboxed/Restricted:** provides hosted or hosted-like C++17 but exposes
  only information allowed by public APIs, permissions, entitlements, and the
  application sandbox.
- **RTOS/Constrained:** provides a documented module subset backed by public
  RTOS APIs. Each header declares its language and library requirements, with
  strict C++11 as the lowest supported language version. Board-specific facts
  come from an explicit provider or adapter; the library must not guess them.
- **Freestanding Minimal:** requires strict C++11 and provides allocation-free
  compile-target, architecture, byte-order, toolchain, execution-environment,
  and explicit board-capability information. It does not promise Hosted Full
  query headers.
- **SDK Restricted:** catalogs a proprietary target without promising a
  backend until lawful SDK access and permitted real-hardware verification are
  available.

- Keep the portable contract separate from platform backends.
- Select a known backend with narrowly scoped compile-time detection.
- Provide a fallback for every public header within its declared profile. An
  unknown Hosted Full platform must compile and return the portable
  unsupported-capability error. An unknown Freestanding Minimal target must
  compile and report explicit unknown target facts rather than fail
  preprocessing, compilation, or linking.
- Account for runtime capability differences within a known platform. A known
  operating system is not proof that a particular API, kernel facility,
  permission, device, or data source is available.
- Prefer documented, stable system APIs. Use pseudo-files, command execution,
  registries, environment variables, or implementation-specific file formats
  only when they are documented platform interfaces or no stronger documented
  API exists. Document the limitation in either case.
- Never invoke external commands to obtain information when an in-process
  system API is available. Do not make network requests as an implicit part of
  a local information query.
- Keep platform conditionals out of the portable public contract. Confine them
  to the smallest practical internal header or function.
- Prefer stable official RTOS APIs for constrained backends. Use an explicit
  provider or adapter for board-specific clocks, memory maps, peripherals,
  identifiers, and capabilities that the runtime cannot discover portably.
- A compiler backend, target macro, cross-compilation result, emulator run, or
  related platform is not proof that a Syscape backend works on the target.
- Adding support for a platform must not weaken behavior on existing platforms
  or remove the fallback path.

## Documentation Rules

- Treat `docs/support-matrix.md` as the source of truth for planned and actual
  information coverage and `docs/platform-catalog.md` as the source of truth
  for platform, architecture, runtime, and toolchain classification. Update
  both as applicable whenever a module, query, backend, target, limitation, or
  verification status changes.
- Document every public type, function, enum, and observable behavior in
  English with Doxygen-compatible comments.
- For each query, document what is measured, the unit, whether the value can
  change, relevant platform availability, and the meaningful error conditions.
- Mark implementation limitations precisely. Do not claim universal support
  from testing a single operating-system version or architecture.
- Keep examples minimal and compilable under the documented minimum language
  version with no dependency beyond Syscape and the platform SDK.
- Update documentation and examples in the same change as any public behavior
  change.

## Testing and Validation

Use strict standard-conformance settings. At minimum:

- Compile Freestanding Minimal checks with GCC and Clang in C++11, C++14, and
  C++17 modes, including C++11 with `-pedantic-errors` and `-ffreestanding`.
- Compile Hosted Full checks with GCC and Clang in C++17 mode and
  `-pedantic-errors`, together with appropriate high-signal warnings.
- Compile MSVC checks at each header's declared minimum standard with its
  standard-conformance mode enabled, such as `/permissive-`.
- Compile every public header in isolation.
- Compile each header against its minimum declared compatibility profile. Do
  not include Hosted Full facilities in Freestanding Minimal compile tests.
- Compile a translation unit that includes each public header twice.
- Link more than one translation unit that includes and uses the same public
  headers to detect ODR violations.
- Exercise successful queries without assuming that optional hardware or
  permissions exist on the test host.
- Exercise unsupported capability, permission failure, missing data,
  temporarily unavailable data, malformed input, UTF conversion failure, and
  native system failure wherever those paths can be controlled.
- Maintain a test configuration that forces the generic fallback backend even
  on a supported development platform.
- Cross-compile architecture and constrained-target checks when an eligible
  toolchain is available, but record cross-compilation separately from real
  target verification.
- Verify boundary values, units, empty-but-valid results, integer conversions,
  and resource cleanup.
- Do not download, generate, link, or execute a third-party dependency as part
  of configuration, compilation, testing, examples, or documentation.

Tests that cannot run on the current host must still be made structurally
testable through narrow internal boundaries. Never weaken or delete a
platform-specific test merely to make another platform pass.

## Workflow for New Information Modules

Before implementing a new module:

1. Define the minimum language version, compatibility profile, portable
   meaning, data types, units, encoding, allocation behavior, and error
   behavior.
2. Identify documented platform sources and the runtime capabilities they
   require.
3. Add the focused public header and internal backend boundaries.
4. Implement known platform backends without exposing native types. For RTOS
   and board-specific data, define an explicit provider boundary where needed.
5. Implement and test the unknown-platform fallback.
6. Add standalone-header, ODR, success, failure, and boundary tests.
7. Add or update English API documentation and examples.

A module is complete only when its public header is self-contained, its
definitions are ODR-safe, its minimum language and profile are documented,
unavailable capabilities produce an honest result for that profile, resources
and text encodings are handled correctly, and all applicable strict-standard
tests pass.

## Change Discipline

- Before completing any code change, stage the intended C++ changes and run
  `pre-commit run clang-format`. Review and stage any resulting formatting
  changes, then run the command again and require it to pass. If `pre-commit`
  or `clang-format` is unavailable, report that explicitly in the change
  summary.
- Install the repository's pre-commit hook in each local clone with
  `pre-commit install`. The hook formats staged C++ headers and sources with
  the repository's `.clang-format` configuration before a commit is created.
- Inspect existing abstractions before adding another one. Reuse the shared
  result, error, encoding, resource, and capability machinery.
- Keep changes focused and do not reformat or rename unrelated code.
- Do not commit generated build artifacts, local caches, or machine-specific
  data.
- Treat new compiler warnings as defects; do not suppress them globally to
  accommodate one backend.
- When a platform cannot be tested, say so explicitly in the change summary
  and still verify the fallback and all host-independent behavior.
- Never describe an information source as portable unless the public semantics
  and fallback behavior are both defined.
