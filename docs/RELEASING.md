# Releasing

The Windows packages are built by GitHub Actions. A local release build is not
required.

The numeric version in the root `CMakeLists.txt` is the source of truth for the
current release line. CMake's `KW_LEGION_VERSION_PRERELEASE` cache variable adds
the SemVer pre-release suffix and defaults to `snapshot` for ordinary builds.
The release workflow clears that suffix to produce the final version.

## Final releases

To cut a release, first ensure the intended version matches `project(VERSION)`
in the root `CMakeLists.txt`. Then push a tag for the commit to release:

```console
git tag v0.4.0
git push origin v0.4.0
```

The release job runs only for tags beginning with `v` and accepts exactly a
numeric `vX.Y.Z` tag. It aborts before building release packages if the tag is
malformed or its version does not match the CMake project version. After the
debug test suite succeeds, it builds the Release preset and uses CPack to
produce an NSIS installer and portable ZIP. It also publishes
`SHA256SUMS.txt` with the GitHub release.

Tags and releases are immutable by convention. Corrections should use the next
patch version rather than replacing a published package.
