# Release smoke fixtures

`smoke.sqfs` — minimal SquashFS image used by the release pipeline's
capability smoke (`.github/workflows/release.yml`): every shipped POSIX
`tebakofs` binary must list it successfully, and builds without the
SquashFS backend (Windows) must report the missing capability loudly.

Contents: `test.txt` ("Hello from SquashFS!") at the image root.

Provenance: `mksquashfs <dir-with-test.txt> smoke.sqfs -noappend -quiet`
(mksquashfs from squashfs-tools). Committed to the repo because the
release smoke must be hermetic — several release legs have no mksquashfs
and the test fixtures under `tests/fixtures/` are gitignored/regenerated.
