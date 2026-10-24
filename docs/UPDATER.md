# PaperNX updater

The updater checks releases from `ysfbz/PaperNx`. TLS host and peer verification stay enabled; a missing or unreadable CA bundle is an error. A release candidate must have the exact `PaperNX.nro` asset, a valid SHA-256, the PaperNX NRO/NACP identity, the expected version and a valid byte count before it can be staged. Downgrades are not offered as updates.

## Manual update flow

1. Run a release check and review the displayed version and release notes.
2. Choose the download. The updater writes to a partial file, verifies it, then stages the candidate beside `PaperNX.nro`.
3. `Ready` means downloaded, verified and staged; it does not mean installed.
4. Back up `books/` and `data/`, close PaperNX, then use a file manager or computer to replace `sdmc:/switch/PaperNX/PaperNX.nro` with the staged NRO. Keep the previous NRO until the replacement starts successfully.

PaperNX never replaces or renames the running canonical NRO and has no in-app restart/install action. The external file-manager replacement is a user action; power-loss-safe replacement by a particular tool or filesystem is not claimed.

## Staging and recovery

There are two independent verified candidate slots: `PaperNX.nro.new` with `.ready`, and `PaperNX.nro.new.1` with `.ready.1`. Each record carries its own hash, size, version, slot, generation and ready state. The old three-line `.ready` format is read as slot 0. At startup candidates are validated independently; the highest valid SemVer is selected, with slot 0 as the deterministic tie-break.

A new download uses the inactive slot and does not remove the selected verified candidate until the new payload and metadata have passed validation and staging. Cancellation, invalid content, insufficient space, write/rename/flush failure, or an incomplete transfer leaves the prior valid candidate available. Partial files, temporary records and unpaired slot files can be cleaned on a later updater check. User books, settings, progress, bookmarks and Reading Journal files are outside the updater's write set.

Legacy `.boot`, `.boot.started`, `.boot.ok` and `.swap` markers block a new download until recovery is resolved. An unmarked `PaperNX.nro.old` is treated as a user backup and left alone. If the canonical NRO is missing, restore a known-good copy with another file manager or from a computer; PaperNX cannot run its own recovery while its executable is absent.

## Security and known limits

The updater fails closed on TLS, JSON schema, URL origin, SHA-256, NRO/NACP identity and version failures. It does not disable certificate verification. Unit and local HTTPS tests exercise cancellation, interrupted transfers, invalid payloads, disk-full and staging faults; these host tests do not establish real Switch SD behavior.

Automatic launcher/chainload installation is not part of v0.9.3. HBL `envSetNextLoad()` argument/RomFS/reboot recovery behavior and FAT power-loss replacement guarantees have not been proven on hardware. Updates therefore remain verified downloads followed by manual NRO replacement. Back up `books/` and `data/` before upgrading.
