# Coding standards

How code in pedantjson is written. Where a tool can hold a rule, it does: `.clang-format`,
`.clang-tidy` and the warning flags in `cmake/PedantjsonConventions.cmake` are the record of those
rules. This document covers the rest, and says why. Anything it leaves open follows the
[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines); where it
departs from them, it says so.

## Language and build

- C++23 with no compiler extensions, on the standard library alone. pedantjson is one header,
  `<pedantjson/pedantjson.hpp>`, and one static library, so that the parser is compiled once
  rather than in every translation unit that reads JSON.
- The README lists the compilers CI proves, each at the oldest version CI builds it with. Code stays
  within those floors: a feature the oldest supported compiler lacks waits until the floor rises. A
  floor rises only when no runner CI uses carries it any more, and raising one is a breaking
  change.
- CMake 3.28 or newer, which Ubuntu 24.04 and Visual Studio 2022 carry. Every configuration the
  project builds is a preset in `CMakePresets.json`, with a workflow preset, so that a developer
  and CI build alike.
- The build only finds its dependencies with `find_package`, and never downloads anything. The
  library needs none; the tests take theirs from the vcpkg manifest's `tests` feature.
- Anything the tests use, such as Catch2 or vendored test files, must be under one of these
  licences: Zlib, MIT, BSD-2-Clause, BSD-3-Clause, BSL-1.0, ISC, Apache-2.0 with or without the
  LLVM exception, CC0-1.0 or Unlicense.

## Naming and spelling

- `PascalCase` for types, concepts, type aliases and template parameters.
- `snake_case` for functions, variables, parameters, namespaces, enumerators and `constexpr`
  constants, with no `k` prefix.
- Private and protected data members end in an underscore (`storage_`). A plain struct's fields
  are bare.
- Enumerations are `enum class`.
- Macros only where nothing else will do, named `PEDANTJSON_UPPER_CASE`.
- Everything is in one namespace, `pedantjson`, with internals in `pedantjson::detail`.
- A name the standard library's concepts look for, such as an iterator's `value_type`, keeps the
  standard's spelling, with a `NOLINT` for the naming check.
- Prose is British: comments, documentation, the README and commit messages. Every identifier is
  American, so a comment "normalises the text" sits above `normalize_text`. One rule for every
  identifier means no word needs a judgement call.
- The words of RFC 8259 and of the interface are used exactly, and never the words they replace:

  | Say | Not |
  |---|---|
  | text, for the JSON being read | source, buffer |
  | document, for what `parse` returns | DOM |
  | value | node, which is the storage behind a value |
  | member | property, field, pair, entry |
  | key, for the quoted string in the text, and name, for what it decodes to | |
  | element | item, entry |
  | offset, for a count of bytes from the start of the text | position, index |
  | position, for a line and a column | location, offset |
  | syntax error | parse error |

## Files and formatting

- The one public header is `include/pedantjson/pedantjson.hpp`. Sources and private headers sit in
  `src/`, as `snake_case.cpp` and `snake_case.hpp`. Headers use `#pragma once`.
- The public header is included as `<pedantjson/pedantjson.hpp>`, and a private header as `"..."`.
- `clang-format` decides the layout, and groups includes as: the file's own header, this project's
  headers, third-party headers, then the standard library. It takes a test file's own header to be
  the one named like it without `_tests`, so a test file isn't named after a standard header:
  `string_tests.cpp` would put `<string>` first.

## Errors and assertions

- A failure that correct code can meet, such as a text that isn't JSON, is returned as a value: the
  function returns `[[nodiscard]] std::expected<T, E>`. This departs from the Core Guidelines' E.2.
- A value of another kind than the caller expects isn't an error: each checked accessor returns a
  `std::optional`. There are no asserting accessors, so pedantjson needs no violation handler, which
  a general library shouldn't impose.
- Constructors never fail. Anything that can fail to come into being is made by a factory
  function, as a `Document` is made by `parse`.
- Each failure domain has an error type of its own, which carries an `enum class` code for programs
  to branch on and a fixed message for each code. A syntax error describes the first mistake only,
  since nothing after it can be read with any certainty.
- A broken precondition is a bug, not an error. The preconditions are few, such as the in-range
  index of `operator[]`, and are checked with `assert`, as the standard library checks its own.
- No `throw`, `try` or `catch`, in the library or in its tests. The library builds with exceptions
  disabled, and stays exception-neutral: RAII throughout, so that an exception from the standard
  library, such as `std::bad_alloc`, unwinds cleanly. Nothing catches it.
- `noexcept` goes on moves, `swap`, and functions that have no precondition and can't fail. It never
  goes on a function with a precondition (the Lakos rule), so that a throwing assertion stays
  possible.
- Nothing the text controls decides how deep the parser recurses. Arrays and objects are read with
  a stack of their own, so that no nesting can overflow the call stack.

## Ownership and types

- Values first, and `std::unique_ptr` for a single owner. No `std::shared_ptr` without a comment
  saying why, and no bare `new` or `delete`.
- `T*` and `T&` never own. A handle into a document holds a pointer to the document's storage
  rather than a reference, so that the handle can be assigned; null means the handle belongs to no
  document, as a default `Array` does.
- A `Document` owns its text and every node read from it, on the heap, so that every handle into it
  survives a move of the document. Handles are cheap to copy and valid while their document lives,
  which is the one lifetime rule a caller learns.
- Counts, offsets and container indices are `std::size_t`, departing from the Core Guidelines'
  ES.107. Signed types are for arithmetic that can go negative.

## Warnings and clang-tidy

- The compiler warnings in `cmake/PedantjsonConventions.cmake` are errors in every preset, through
  `CMAKE_COMPILE_WARNING_AS_ERROR`. The library gets its settings with
  `pedantjson_target_conventions`, and a test target with `pedantjson_target_warnings`. The
  settings are private to each target, so that none of them reaches the installed package's
  interface or a user's build; a consumer test checks this.
- clang-tidy runs the groups in `.clang-tidy`, with every warning an error. A finding that has to
  stand is suppressed with `NOLINT(<check>): <reason>` or `NOLINTNEXTLINE(<check>): <reason>`, never
  a bare `NOLINT`. Keep the reason on that line: `NOLINTNEXTLINE` covers only the line after its
  own, and any longer explanation goes in a comment above it.

## Documentation

- Every declaration in the public header has a `///` Doxygen comment, those in `detail` and the
  private members included, and the header begins with a `\file` comment. Clang's `-Wdocumentation`
  checks each comment against its declaration.
- A comment says why, not what.

## Tests

- Tests come first: a test that fails, then the code that passes it. A new test that passes at once
  is checked by breaking the code it covers, since a test that can't fail proves nothing.
- A test uses the public header only, and survives any refactoring that keeps the behaviour. Its
  name is a sentence saying what holds, such as "a default array is empty". A test case's name must
  not begin with `-`, which Catch2 would read as one of its options.
- A test reaches into what it reads through `support::required`, `support::element` and
  `support::member`, which end the test when an optional is empty or an index is past the end,
  rather than dereferencing or indexing unchecked.
- Each test file covers one part of the format or the interface, and its test cases carry that
  part's Catch2 tag, such as `[strings]`. A test of a syntax error checks its code and its offset.
- A test's JSON text is written in C++, as a raw string where it can be. Check the bytes of any test
  text that holds a `\u` escape, since a tool that decodes it on the way into the file leaves a
  test that can't fail.
- `tests/consumers/` builds a small program against the installed package, through pkg-config, and
  against the source tree as a subproject. Each run starts from an empty prefix and empty build
  trees, so that nothing an earlier run left can stand in for what this one should make.

## Versions, commits and branches

- Versions follow [semantic versioning](https://semver.org). Before 1.0, a minor release may break
  the interface. The version lives in `project(pedantjson VERSION x.y.z)`, and in the vcpkg port's
  manifest, which vcpkg needs and a test keeps in step with it.
- Commit subjects follow Conventional Commits, `type(scope)!: description`, with the type one of
  `build`, `chore`, `ci`, `docs`, `feat`, `fix`, `perf`, `refactor`, `revert`, `style` or `test`.
  The scope and the `!` of a breaking change are optional. A message describes the change and why.
- Branches follow git flow, with no prefix on version tags. `main` holds released versions only,
  and each merge into it is tagged with its version. `develop` is where work comes together. A
  change is made on `feature/<slug>` from `develop`, its slug naming the change; a release on
  `release/<version>`; and a fix to a release on `hotfix/<version>` from `main`. Every merge is
  `--no-ff`.
- Every branch gets a pull request, which must pass the `ci` check before the branch is finished
  with `git flow`.
