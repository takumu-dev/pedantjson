# pedantjson

pedantjson is a strict JSON reader for C++23. It reads a text into an immutable tree that
remembers where everything came from: every key and every value carries the byte offset where it
begins, an object's members keep the order the text gives them, every duplicate name included, and
a number keeps the text it was written as. A text that isn't JSON gives an error that says what is
wrong and where.

It is meant for formats written by hand, whose readers have to point at each mistake precisely: a
parser that quietly keeps the last of two duplicate keys, or forgets where a value came from, can't
do that.

pedantjson is at an early stage, and has no release yet. Today it reads the grammar of
[RFC 8259](https://www.rfc-editor.org/rfc/rfc8259) strictly, rejecting every text the RFC forbids
and deciding each case the RFC leaves open, as [Strictness](#strictness) describes. Still to come
are numbers read into the type a caller asks for, lines and columns, and JSON pointers.

## Example

```cpp
#include <pedantjson/pedantjson.hpp>

#include <iostream>

int main() {
    const auto document = pedantjson::parse(R"({"width": 1, "width": 2})");
    if (!document) {
        std::cerr << "offset " << document.error().offset << ": " << document.error().message()
                  << '\n';
        return 1;
    }
    for (const auto [key, value] : *document->root().as_object()) {
        std::cout << key.name() << " at offset " << key.offset() << " is "
                  << value.as_number()->text() << '\n';
    }
}
```

This prints:

```text
width at offset 1 is 1
width at offset 13 is 2
```

## The interface

Everything is in namespace `pedantjson`, in the one header `<pedantjson/pedantjson.hpp>`.

- `parse(std::string text, ParseOptions = {})` returns a `std::expected<Document, SyntaxError>`.
  `ParseOptions::max_depth` limits how deeply arrays and objects may nest.
- A `Document` owns the text and the tree read from it. It can be moved but not copied.
- A `Value` is a cheap handle into a document, valid while the document lives, through any move of
  it. `kind()` says which of `null`, `boolean`, `number`, `string`, `array` or `object` it is, and
  `offset()` where it begins. Its accessors `as_bool()`, `as_number()`, `as_string()`,
  `as_array()` and `as_object()` each return a `std::optional`, empty when the value is of another
  kind, so nothing asserts or throws on a value of the wrong kind.
- An `Array` is a range of values, and an `Object` a range of members, each a `Key` and a `Value`,
  in the order of the text. `Object::find(name)` answers the first member of a name, and a caller
  that must treat duplicates walks the members.
- A `Number` keeps its text, exactly as written.
- A `SyntaxError` has a `code`, the `offset` where the text stops being JSON, and a `message()`.

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
- An object keeps every member, duplicate names included, and leaves it to the caller whether a
  duplicate is an error.

[JSONTestSuite](https://github.com/nst/JSONTestSuite)'s parsing tests prove these rules: every
text it says is JSON is read, every text it says isn't is rejected, and each of the texts whose
outcome it leaves to the parser has a row in a table in `tests/json_test_suite_tests.cpp`, giving
pedantjson's outcome and the reason for it.

## Requirements

- A C++23 compiler and standard library with `std::expected`. CI builds with GCC 16 so far; the
  oldest compilers pedantjson supports will be listed here once CI proves them.
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

As a subproject, through a git submodule or `FetchContent`:

```cmake
add_subdirectory(external/pedantjson)
target_link_libraries(app PRIVATE pedantjson::pedantjson)
```

As a subproject, pedantjson builds the library alone, without its tests or install rules. These
options change what it builds:

| Option | Default | Effect |
|---|---|---|
| `PEDANTJSON_BUILD_TESTS` | on as the top-level project | Build the tests. |
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
| `gcc-noexcept` | GCC, release, with the library built without exceptions |
| `clang-debug`, `clang-release` | Clang |

The presets take Catch2 from the vcpkg manifest, with `VCPKG_ROOT` naming a clone of
[vcpkg](https://vcpkg.io), and build with Ninja. Warnings are errors in every preset.

- `CODING_STANDARDS.md` says how the code is written, tested and committed.
- `tests/consumers/` builds a small program against the installed package, through pkg-config, and
  as a subproject.

## Acknowledgements

The tests include the parsing tests of [JSONTestSuite](https://github.com/nst/JSONTestSuite), by
Nicolas Seriot, under the MIT licence. They are copied unchanged into `tests/JSONTestSuite/`, with
that licence beside them.

## Licence

pedantjson is released under the zlib licence. See `LICENSE`.
