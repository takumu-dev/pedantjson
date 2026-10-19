# Releasing

How a version of pedantjson is released. A release crosses three places: this repository, where
git flow makes the release and tags it; GitHub Releases, where the tag gets its section of the
changelog as notes; and vcpkg, whose port in `ports/pedantjson/` downloads and builds the release.
Below, `<version>` stands for the new version, such as `0.2.0`.

## Choosing the version

Versions follow [semantic versioning](https://semver.org), and tags carry the version alone, with
no `v`. Before 1.0.0, a release that breaks the interface raises the minor version, and any other
release the patch. Raising the oldest version of a compiler that pedantjson supports breaks the
interface.

## 1. Make the release

From an up-to-date `develop` that passes `ci`, start the release:

```sh
git flow release start <version>
```

On the release branch, make one commit, `chore(release): <version>`, that:

- sets `project(pedantjson VERSION <version>)` in `CMakeLists.txt`;
- renames the `## [Unreleased]` heading of `CHANGELOG.md` to `## [<version>] - <date>`, the date
  as `YYYY-MM-DD`, and puts a new, empty `## [Unreleased]` above it;
- at the foot of `CHANGELOG.md`, points `[Unreleased]` at
  `https://github.com/takumu-dev/pedantjson/compare/<version>...HEAD`, and adds
  `[<version>]: https://github.com/takumu-dev/pedantjson/releases/tag/<version>` below it.

The tests check that the changelog's newest release is the project's version. The vcpkg port stays
as it is, since it can't name a release that isn't tagged yet; step 3 moves it.

Push the release branch and open a pull request into `main`. Once it passes `ci`, finish the
release, which merges it into `main`, tags that merge `<version>`, and merges `main` back into
`develop`, and push the result:

```sh
git flow release finish --keepremote -m "<version>" <version>
git push origin main develop <version>
git push origin --delete release/<version>
```

`--keepremote` keeps the release branch on GitHub until `main` is pushed, so that GitHub sees the
pull request merged rather than closed.

A fix to a release is made the same way, on a branch that `git flow hotfix start <version>` starts
from `main`, and finished with `git flow hotfix finish --keepremote -m "<version>" <version>`, then
published and given its port as in steps 2 and 3. Step 3 moves the port on `develop`, so the
port on `main` still names the release before the one being fixed. The hotfix's commit therefore
also takes the port from `develop`, where it names the release being fixed:

```sh
git checkout develop -- ports/pedantjson
```

Finishing the hotfix merges it into `develop` too, where its section of the changelog meets the
entries under `develop`'s `Unreleased`. Keep both, with the hotfix's section below `Unreleased`.

## 2. Publish it on GitHub

Each tag gets a GitHub Release whose notes are its section of the changelog:

```sh
awk -v heading="## [<version>]" '
    index($0, "## ") == 1 { printing = index($0, heading) == 1; next }
    /^\[[^]]+\]: / { printing = 0 }
    printing
' CHANGELOG.md | gh release create <version> --verify-tag --title <version> --notes-file -
```

A release carries no binaries. pedantjson is built from source by the project that uses it, with
that project's compiler and flags, and GitHub adds the archives of the source to every release.

## 3. Move the vcpkg port to the release

`ports/pedantjson/` is written to go into vcpkg's registry unchanged. It downloads the release that
its manifest names from GitHub, checks it against the hash in its portfile, and builds the library
without its tests or fuzzer. Once the release is tagged, a feature moves the port onto it:

```sh
git flow feature start vcpkg-port-<version>
```

- Set `"version"` in `ports/pedantjson/vcpkg.json` to `<version>`.
- Set `SHA512` in `ports/pedantjson/portfile.cmake` to the hash of the archive GitHub makes of the
  tag:

  ```sh
  curl --fail --location --output pedantjson.tar.gz \
      https://github.com/takumu-dev/pedantjson/archive/<version>.tar.gz
  "$VCPKG_ROOT/vcpkg" hash pedantjson.tar.gz
  ```

Commit both as `build(vcpkg): build <version> through the port`, push the branch and open a pull
request into `develop`. Its vcpkg consumer job builds a project through the port, from the release
on GitHub. Once it passes `ci`, finish the feature in the same way as the release:

```sh
git flow feature finish --keepremote vcpkg-port-<version>
git push origin develop
git push origin --delete feature/vcpkg-port-<version>
```

The tests let the port name the project's version or the release before it, and nothing older, so
a release whose port was never moved stops the next release's `ci`.

## 4. Submit the port to vcpkg's registry

pedantjson goes into [vcpkg's registry](https://github.com/microsoft/vcpkg) at 1.0.0. Until then, a
project using vcpkg takes `ports/pedantjson/` as an overlay port.

From 1.0.0, every release is also submitted to the registry once step 3 is done, following vcpkg's
[maintainer guide](https://learn.microsoft.com/vcpkg/contributing/maintainer-guide). In a clone of
a fork of microsoft/vcpkg, on a branch from its `master`:

```sh
cp -R <pedantjson>/ports/pedantjson ports/
./vcpkg format-manifest ports/pedantjson/vcpkg.json
git add ports/pedantjson
git commit -m "[pedantjson] Update to <version>"
./vcpkg x-add-version pedantjson
git add versions
git commit --amend --no-edit
```

`format-manifest` should change nothing, and the port's files go in as they are, so that the port
in the registry and the one CI builds stay the same. The first submission's message is
`[pedantjson] New port`. Open a pull request into microsoft/vcpkg, whose CI builds the port on
each of vcpkg's own CI triplets that its `"supports"` allows.
