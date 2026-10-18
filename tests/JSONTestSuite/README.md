# JSONTestSuite

`test_parsing/` is the parsing test set of [JSONTestSuite](https://github.com/nst/JSONTestSuite)
by Nicolas Seriot, copied unchanged from commit `1ef36fa01286573e846ac449e8683f8833c5b26a`
(22 November 2024). It is under the MIT licence, in `LICENSE` beside this file.

Each file's name says what a parser must do with it:

- `y_`: the text is JSON, and must be read.
- `n_`: the text isn't JSON, and must be rejected.
- `i_`: RFC 8259 leaves the outcome to the parser. `tests/json_test_suite_tests.cpp` holds a table
  with a row for each, giving pedantjson's outcome and the reason for it.

The files are kept byte for byte, which `.gitattributes` protects from line-ending conversion: some
are deliberately not UTF-8, and three are UTF-16.

To update the copy, replace `test_parsing/` and `LICENSE` with those of a newer commit, change the
commit above and the counts of files the tests expect, and give every new `i_` file a row in the
table.
