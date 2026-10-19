# Changelog

Every change to pedantjson that a project using it would notice is listed here, newest first, in
the form of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions follow
[semantic versioning](https://semver.org/spec/v2.0.0.html), under which a minor release before
1.0.0 may break the interface.

## [Unreleased]

## [0.1.0] - 2026-10-19

### Added

- `parse`, which reads a text strictly as [RFC 8259](https://www.rfc-editor.org/rfc/rfc8259)
  defines JSON, rejecting every text the RFC forbids, into a `Document` that owns the text and an
  immutable tree read from it. A text that isn't JSON gives a `SyntaxError` with a code, the offset
  where the text stops being JSON, the position of that offset, and a message.
- `ParseOptions::max_depth`, which limits how deeply arrays and objects may nest, 256 by default.
  The parser keeps a stack of its own, so no text can overflow the call stack.
- `Value`, a handle into a document that survives a move of it, with its kind, its offset, and
  accessors that answer in `std::optional`. `Array` and `Object` are ranges in the order of the
  text, and an object keeps every member, duplicate names included, each with its `Key`.
- `Number`, which keeps the text it was written as and reads it with `to<T>()` into any standard
  integer type, `float` or `double`, giving a `NumberError` for a value out of the type's range
  or, for an integer type, one that isn't an integer. A number whose value is an integer, such as
  `1.0` or `1e2`, is an integer at any size.
- `Document::position`, which turns an offset into a line and a column, the column counting code
  points.
- `Pointer`, an [RFC 6901](https://www.rfc-editor.org/rfc/rfc6901) JSON pointer, built with `/` or
  read with `Pointer::parse`, and `Document::find` and `Document::nearest`, which look one up, the
  latter answering the deepest value on its way that the text wrote.
- Rules for what RFC 8259 leaves open: the text must be UTF-8, a byte-order mark at its start is
  ignored, and an escaped surrogate must be half of a pair.
- A CMake package that gives the target `pedantjson::pedantjson`, and a pkg-config file. The
  library builds with exceptions enabled or disabled, with GCC 12, Clang 19 with libstdc++,
  Clang 20 with libc++, MSVC 14.44, Apple Clang from Xcode 26.4, and newer.

[Unreleased]: https://github.com/takumu-dev/pedantjson/compare/0.1.0...HEAD
[0.1.0]: https://github.com/takumu-dev/pedantjson/releases/tag/0.1.0
