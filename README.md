<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/pedantjson-logo-dark.svg">
    <img src="assets/pedantjson-logo.svg" alt="pedantjson" width="340">
  </picture>
</h1>

pedantjson is a strict JSON reader for C++23. It reads a text into an immutable tree that
remembers where everything came from: every key and every value carries the byte offset where it
begins, which the document turns into a line and a column, an object's members keep the order the
text gives them, every duplicate name included, and a number keeps the text it was written as. A
text that isn't JSON gives an error that says what is wrong, on which line and in which column.

It is meant for formats written by hand, whose readers have to point at each mistake precisely: a
parser that quietly keeps the last of two duplicate keys, or forgets where a value came from, can't
do that.

It also writes: any value of a document, or a draft, a value built in memory, as compact or
indented text that it reads back as the same values, as [Writing](#writing) describes.

pedantjson is at an early stage: until 1.0.0, a minor release may break the interface, and
`CHANGELOG.md` lists what each release changes. It reads the grammar of
[RFC 8259](https://www.rfc-editor.org/rfc/rfc8259) strictly, rejecting every text the RFC forbids
and deciding each case the RFC leaves open, as [Strictness](#strictness) describes. It reads a
number into the type a caller asks for, catching a value beyond that type's range, and it builds
and looks up [RFC 6901](https://www.rfc-editor.org/rfc/rfc6901) JSON pointers, finding for a value
the text never wrote the nearest value it did.

## Example

```cpp
#include <pedantjson/pedantjson.hpp>

#include <iostream>

int main() {
    const auto document = pedantjson::parse(R"({
  "width": 1,
  "width": 2,
  "margins": {"top": 4}
})");
    if (!document) {
        const auto& error = document.error();
        std::cerr << error.position.line << ':' << error.position.column << ": "
                  << error.message() << '\n';
        return 1;
    }
    // Every member is kept, a repeated name's too, with the position of its key.
    for (const auto [key, value] : *document->root().as_object()) {
        const auto position = document->position(key.offset());
        std::cout << position.line << ':' << position.column << ": " << key.name() << '\n';
    }
    // A member the text leaves out takes the position of the nearest value the text has.
    const auto bottom = pedantjson::Pointer{} / "margins" / "bottom";
    const auto position = document->position(document->nearest(bottom).offset());
    std::cout << position.line << ':' << position.column << ": no " << bottom.text() << '\n';
}
```

This prints:

```text
2:3: width
3:3: width
4:3: margins
4:14: no /margins/bottom
```

## The interface

Everything is in namespace `pedantjson`, in the one header `<pedantjson/pedantjson.hpp>`.

- `parse(std::string text, ParseOptions = {})` returns a `std::expected<Document, SyntaxError>`.
  `ParseOptions::max_depth` limits how deeply arrays and objects may nest.
- A `Document` owns the text and the tree read from it. It can be moved but not copied.
  `position(offset)` gives the `Position` of any offset in the text: its `line` and its `column`,
  each counting from 1. A line ends at a line feed, a carriage return, or both together. A column
  counts code points rather than bytes, a tab being one, and a byte-order mark at the start of the
  text takes none, as an editor shows it.
- A `Value` is a cheap handle into a document, valid while the document lives, through any move of
  it. `kind()` says which of `null`, `boolean`, `number`, `string`, `array` or `object` it is, and
  `offset()` where it begins. Its accessors `as_bool()`, `as_number()`, `as_string()`,
  `as_array()` and `as_object()` each return a `std::optional`, empty when the value is of another
  kind, so nothing asserts or throws on a value of the wrong kind.
- An `Array` is a range of values, and an `Object` a range of members, each a `Key` and a `Value`,
  in the order of the text. `Object::find(name)` answers the first member of a name, and a caller
  that must treat duplicates walks the members.
- A `Number` keeps its text, exactly as written. `to<T>()` reads it into any standard integer
  type, `float` or `double`, and returns a `std::expected<T, NumberError>`. `is_integer()` says
  whether its value is an integer.
- A `Pointer` is a JSON pointer, as RFC 6901 defines one, that owns its text. `/` appends a
  member's name or an element's index, escaping a name's `~` and `/`, so `Pointer{} / "a/b" / 0`
  is `/a~1b/0`. `Pointer::parse(text)` reads one from its text, returning a
  `std::expected<Pointer, PointerError>`, and `text()` gives the text back.
- `Document::find(pointer)` answers the value a pointer names, if the document has it, following a
  repeated name to its first member. `Document::nearest(pointer)` answers the deepest value on the
  pointer's way that the document has, the root at worst, whose position a value the text never
  wrote can take.
- A `NumberError` has a `code`, `out_of_range` or `not_an_integer`, and a `message()`.
- A `PointerError` has a `code`, `missing_slash` or `invalid_escape`, the `offset` where the text
  stops being a pointer, and a `message()`.
- A `SyntaxError` has a `code`, the `offset` where the text stops being JSON, the `position` of
  that offset, and a `message()`.
- `write(value, WriteOptions = {})` writes a value of a document as text, returning a
  `std::string`. `WriteOptions::indent` lays the text out with that many spaces to a level, and
  leaves it compact when it is empty, as it is by default.
- A `Draft` is a value built in memory to be written. It is made from `nullptr`, a `bool`, any
  number type that `to<T>()` reads, or a string, or as `Draft::array()` or `Draft::object()`,
  which `append` adds elements or members to. An explicit `Draft(value)` copies a value of a
  document. A draft is an ordinary value, copied and moved like any other, and has nothing to read
  back with.
- `write(draft, WriteOptions = {})` returns a `std::expected<std::string, WriteError>`. A
  `WriteError` has a `code`, `not_finite` or `invalid_utf8`, the `pointer` to the first value that
  can't be written, and a `message()`.

## Strictness

pedantjson reads only what RFC 8259 calls JSON, and decides the cases the RFC leaves to the parser
as follows:

- The text is UTF-8, the only encoding the RFC allows between systems. Bytes that aren't UTF-8,
  wherever they stand, are a syntax error of their own, `invalid_utf8`: a byte that begins no
  sequence, a sequence cut short, a character written in more bytes than it needs, an encoded
  surrogate, or a code point beyond U+10FFFF. A text in UTF-16 or UTF-32 is rejected.
- A byte-order mark at the start of the text is ignored, though offsets still count its three
  bytes. Anywhere else it is a character, allowed only in a string.
- A `\u` escape of a surrogate has to be half of a pair: a high surrogate followed at once by a
  `\u` escape of a low one. Any other surrogate escape is `unpaired_surrogate`, at its backslash,
  since it stands for no character. Escapes of noncharacters such as U+FFFF, and of U+0000, are
  read, since they stand for characters.
- Arrays and objects may nest as deeply as `ParseOptions::max_depth`, 256 by default. The bracket
  that goes deeper is `too_deep`. The parser keeps its own stack rather than recursing, so the
  limit isn't for its sake but for a program that walks the document by recursion.
- A number of any size or precision is read, and keeps its text, so a value too large for a type
  is caught when it is read into that type rather than when the text is read.
- A number whose value is an integer is an integer, however it is written: `1.0`, `1e2` and
  `150e-1` read into an `int` as 1, 100 and 15, exactly, at any size the type holds. A number whose
  value has a fraction, such as `1.5` or `1e-1`, is `not_an_integer` for an integer type, and an
  integer the type can't hold is `out_of_range`. Negative zero reads as zero.
- A floating-point type reads the value nearest the number. A number that rounds beyond the type's
  largest finite value is `out_of_range`, and so is one that isn't zero but rounds to zero, rather
  than becoming an infinity or a zero the text never wrote. Negative zero keeps its sign.
- An object keeps every member, duplicate names included, and leaves it to the caller whether a
  duplicate is an error.

[JSONTestSuite](https://github.com/nst/JSONTestSuite)'s parsing tests prove these rules: every
text it says is JSON is read, every text it says isn't is rejected, and each of the texts whose
outcome it leaves to the parser has a row in a table in `tests/json_test_suite_tests.cpp`, giving
pedantjson's outcome and the reason for it.

## Writing

`write` writes JSON that `parse` reads back as the same values: the same kinds, members in the same
order, duplicates included, the same strings, and, read with `to<T>()` into the type each number
was made from, the same numbers. That holds when the text is read with a `ParseOptions::max_depth`
at least as deep as the arrays and objects written, since `write` writes any depth.

```cpp
#include <pedantjson/pedantjson.hpp>

#include <iostream>
#include <limits>

int main() {
    auto frame = pedantjson::Draft::object();
    frame.append("frame", 12).append("time", 0.1F).append("name", "start\n");
    auto samples = pedantjson::Draft::array();
    samples.append(-0.0).append(1e-7);
    frame.append("samples", std::move(samples));

    if (const auto text = pedantjson::write(frame, {.indent = 2})) {
        std::cout << *text << '\n';
    }

    // What JSON can't hold is refused when the draft is written, with the pointer to it.
    frame.append("rate", std::numeric_limits<double>::infinity());
    if (const auto text = pedantjson::write(frame); !text) {
        std::cerr << text.error().pointer.text() << ": " << text.error().message() << '\n';
    }
}
```

This prints:

```text
{
  "frame": 12,
  "time": 0.1,
  "name": "start\n",
  "samples": [
    -0,
    1e-07
  ]
}
/rate: number not finite
```

- Compact text has no whitespace at all. Indented text has one element or member to a line, a
  space after each colon, and each closing bracket on a line of its own at its parent's indent; an
  empty array or object stays `[]` or `{}`. An indent of 0 is a broken precondition, checked by
  `assert`. Either way, the text ends at the value's last character, with no final line feed, and
  never begins with a byte-order mark, even for a document whose text did.
- A number from a document is written with the text it was read with, so nothing is lost however
  large or precise it is. A `float` or `double` in a draft is written in the shortest form that
  `to<T>()` reads back as the same bits, negative zero and subnormals included, and a `float` is
  shortest as a `float`. An integer is written exactly.
- A string or a name is written from its characters, escaped again whatever the text it came from
  wrote: `\"` and `\\`, `\b`, `\f`, `\n`, `\r` and `\t`, any other character below U+0020 as a
  `\u` escape in lower-case hexadecimal, and every other character as itself in UTF-8, `/`, DEL,
  U+2028 and U+2029 included.
- A draft holds whatever it is given, so a `float` or `double` that isn't finite is refused when
  the draft is written, as `not_finite`, and a string or a name that isn't UTF-8 as `invalid_utf8`.
  The error points at the first such value in the order of the text, a name at its member, before
  the member's value, and a failed write gives no text. A value of a document holds nothing JSON
  can't, so writing one can't fail.

## Requirements

- A C++23 compiler and standard library with `std::expected`, and with `std::from_chars` and
  `std::to_chars` for floating-point numbers. pedantjson supports these, each from the oldest
  version CI builds and tests it with, through to the newest release, which CI builds too:

  | Compiler | Oldest version |
  |---|---|
  | GCC | 12 |
  | Clang, with libstdc++ | 19, with the libstdc++ of GCC 12 or newer |
  | Clang, with libc++ | 20 |
  | MSVC | 14.44, from Visual Studio 2022 17.14 |
  | Apple Clang | from Xcode 26.4 |

  An oldest version rises only when CI can't build with it any more, and a release that raises one
  is a breaking change. Until MSVC 14.52, CMake builds C++23 with MSVC in its preview standard
  mode.
- CMake 3.28 or newer.
- Nothing else: pedantjson uses the standard library alone. It never throws, and builds with
  exceptions enabled or disabled.

## Using pedantjson

The build only finds its dependencies with `find_package`, and never downloads anything. The
library itself needs none; the tests need [Catch2](https://github.com/catchorg/Catch2) 3.

As an installed package:

```cmake
find_package(pedantjson CONFIG REQUIRED)
target_link_libraries(app PRIVATE pedantjson::pedantjson)
```

The install also has a pkg-config file, `pedantjson.pc`, for Meson, Makefiles and other builds:

```sh
c++ -std=c++23 app.cpp $(pkg-config --cflags --libs pedantjson)
```

Through [vcpkg](https://vcpkg.io), with `ports/pedantjson/` as an overlay port until pedantjson is
in vcpkg's registry. The port downloads the release it names and installs it as a package, which
`find_package` finds as above. Take it from the `develop` branch: the port can name a release only
once the release is tagged, so a release's own copy names the release before it.

As a subproject, through a git submodule or `FetchContent`:

```cmake
add_subdirectory(external/pedantjson)
target_link_libraries(app PRIVATE pedantjson::pedantjson)
```

As a subproject, pedantjson builds the library alone, without its tests, fuzzer or install rules.
These options change what it builds:

| Option | Default | Effect |
|---|---|---|
| `PEDANTJSON_BUILD_TESTS` | on as the top-level project | Build the tests. |
| `PEDANTJSON_BUILD_FUZZER` | on as the top-level project | Build the fuzzer, where the compiler can link libFuzzer. |
| `PEDANTJSON_INSTALL` | on as the top-level project | Install the header, the library and the package files. |
| `PEDANTJSON_EXCEPTIONS` | on | Build the library with exceptions enabled. |

## Working on pedantjson

Every configuration the project builds is a preset in `CMakePresets.json`, and a workflow preset
configures, builds and tests in one step:

```sh
cmake --workflow --preset gcc-debug
```

| Preset | Configuration |
|---|---|
| `gcc-debug`, `gcc-release` | GCC |
| `gcc-sanitize` | GCC, debug, with AddressSanitizer and UndefinedBehaviorSanitizer |
| `gcc-noexcept` | GCC, release, with the library built without exceptions |
| `clang-debug`, `clang-release`, `clang-sanitize` | Clang with libstdc++, as for GCC |
| `libcxx-debug`, `libcxx-release`, `libcxx-sanitize` | Clang with libc++, as for GCC |
| `msvc-debug`, `msvc-release` | MSVC, from a developer command prompt |
| `msvc-asan` | MSVC, optimised with assertions kept on, with AddressSanitizer |
| `msvc-noexcept` | MSVC, release, the library alone, built without exceptions |
| `apple-clang-debug`, `apple-clang-release`, `apple-clang-noexcept` | Apple Clang, as for GCC |
| `library` | The library alone, release, built without vcpkg and installed into `build/library/prefix` |
| `tidy` | The library and the tests, with clang-tidy |
| `format` | The formatting, checked with clang-format |
| `docs` | The documentation of the public header, checked with Doxygen |

The presets take Catch2 from the vcpkg manifest, with `VCPKG_ROOT` naming a clone of
[vcpkg](https://vcpkg.io), and build with Ninja. Warnings are errors in every preset.
Once the `format` preset is configured, `cmake --build --preset format --target format` formats
every source in place.

`fuzz/` holds a [libFuzzer](https://llvm.org/docs/LibFuzzer.html) target, which feeds the parser
texts it makes up and checks what the header promises of each. It is built where the compiler is a
Clang that can link libFuzzer's runtime, and its `fuzz` target runs it for a minute, starting from
the texts of JSONTestSuite and of `fuzz/corpus/`:

```sh
cmake --workflow --preset clang-sanitize
cmake --build --preset clang-sanitize --target fuzz
```

`PEDANTJSON_FUZZ_SECONDS` sets how long it runs. A text that crashes it is written into
`build/clang-sanitize/fuzz/crashes/`, and is then committed to `fuzz/corpus/` with a test of its
own, as `CODING_STANDARDS.md` describes.

- `CODING_STANDARDS.md` says how the code is written, tested and committed.
- `CHANGELOG.md` lists the changes in each release, and those not yet released, and
  `RELEASING.md` says how a release is made.
- `tests/consumers/` builds a small program against the installed package, through pkg-config, and
  as a subproject. Where the other of GCC and Clang is found beside the build's compiler on
  libstdc++, it builds the program with that one too, against the library this build made.
- `ports/pedantjson/` is the vcpkg port, which downloads and builds the release it names. CI
  builds the same program through it, and through its portfile run on the checkout by
  `ci/checkout-port/`, so that each change is built with vcpkg before it is released.
- `.circleci/config.yml` builds and tests every pull request with each compiler above, at its
  oldest version and its newest release, and fuzzes the parser for a minute.

## Acknowledgements

The tests include the parsing tests of [JSONTestSuite](https://github.com/nst/JSONTestSuite), by
Nicolas Seriot, under the MIT licence. They are copied unchanged into `tests/JSONTestSuite/`, with
that licence beside them.

The logo's wordmark and the social preview's tagline are drawn from
[IBM Plex Mono](https://github.com/IBM/plex), under the SIL Open Font License.

## Licence

pedantjson is released under the zlib licence. See `LICENSE`.
