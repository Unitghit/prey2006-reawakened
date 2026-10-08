# Release checklist

Version 1.0.2. Build the archives with `tools/package-release.ps1 -Build` from a
clean, committed tree; it produces the portable zip, a source zip of the same
commit and SHA-256 checksums in `output/release`, and fails if the package
contains retail data, data derived from it, imported content or user files.

## Repository preparation

- [x] Separate source snapshot from the active playtest installation.
- [x] Preserve upstream notices and record reference revisions.
- [x] Document features, testing limits, and provenance questions.
- [x] Exclude retail archives, keys, personal saves, and build outputs.
- [ ] Complete licensing and asset provenance review before public distribution (owner).

## Packaging

- [x] Verify clean Windows x64 engine build from this snapshot.
- [x] Verify native launcher settings, layout, arguments, and Save & Play process behavior with an isolated probe.
- [x] Verify portable runtime gameplay with imported retail data (full campaign playthrough).
- [x] Import and validate a retail installation without copying keys or executable files.
- [x] Guided Setup in the launcher with automatic discovery plus Browse.
- [x] Build files derived from retail Prey at Setup instead of shipping them.
- [x] Produce the portable zip, source archive, dependency notices, and SHA-256 checksums.
- [x] Version shown in the console, `si_version` and the executables' file properties.
- [x] Test installation and an in-place update with preserved settings and saves.
- [x] Test missing, damaged, incomplete and wrong-game retail data errors (launcher `--verify`).
- [x] Test launch from paths containing spaces and non-ASCII characters.
- [x] Test a clean folder from the release zip; Proton remains untested.

## Publication

- [x] Player README, changelog and known issues.
- [ ] Final audit of the exact release archive and source commit.
- [ ] Owner approval to make the repository public and publish the release.
