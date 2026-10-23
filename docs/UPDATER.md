# PaperNX self-updater

The updater checks releases only from `ysfbz/PaperNx` and accepts only the exact `PaperNX.nro` asset with a matching SHA-256 and valid PaperNX NRO/NACP identity and version. TLS peer and host verification stay enabled. A missing or unreadable RomFS trust bundle is an error; the updater never disables verification or uses an unverified fallback.

Automatic launch checks default to off and are stored in app settings. A manual check remains available. When enabled, the launch check starts only after the first rendered frame and performs network work on the updater worker. Release notes and the NRO are shown before download; the user must choose Update, then Restart to install. Download cancellation removes the partial `.part` file. The updater stages only files adjacent to `PaperNX.nro`; progress, catalog, settings, books and Reading Journal are not written by the update flow.

## CA trust bundle

`romfs/cacert.pem` is curl's PEM conversion of Mozilla's public root store, downloaded from [curl.se/ca/cacert.pem](https://curl.se/ca/cacert.pem). The source documents that the converted bundle retains Mozilla's **MPL-2.0** license: [CA extract and license details](https://curl.se/docs/caextract.html). The bundled revision was updated by Mozilla on **2026-10-20** and contains **118 certificates**; its SHA-256 is `796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe` and its size is **200,116 bytes**. `scripts/license-notices/curl-ca-bundle.md` preserves this attribution in generated license archives.

`make all` parses the PEM before compiling, and `scripts/verify_artifacts.py` confirms the same file is embedded in the NRO RomFS. The current trust set is intentionally a Mozilla general-purpose bundle; update it from curl's maintained extract and revise this checksum/size record when refreshing it. The native Switch build produced an AArch64 ELF and NRO containing this exact 200,116-byte source bundle. The NRO's RomFS is 1,962,916 bytes; that includes fonts and other RomFS entries, so it is not a before/after size comparison.

## Install, startup confirmation and recovery

Before install, the verified old executable is copied durably to `PaperNX.nro.old`; a small `.boot` record stores the new file's hash, size and version. The candidate replaces the executable only after it has been downloaded, fully validated and staged. At the next launch `prepareBoot()` validates the installed candidate and writes `.boot.started`. The application keeps `.old` until it renders its first frame and writes `.boot.ok`; a later launch with `.boot.started` but no confirmation restores `.old`. This is a first-frame startup check, not proof that every reader feature works on the new build.

The install uses two renames because Switch FAT-backed storage may reject renaming over an existing target. There is a short interruption window after the old path is renamed to `.swap` and before `.new` becomes `PaperNX.nro`. `.old` remains as a second copy, but if power is lost in that window the canonical NRO path may be absent, so PaperNX cannot run its own recovery code. Recovery then requires restoring `PaperNX.nro.old` to `PaperNX.nro` with another homebrew file manager or from a computer. This manual recovery path is not automated or physically tested. Do not describe the installer as providing power-loss-proof rollback.

Local HTTPS integration tests exercise valid and invalid certificates, source/asset restrictions, cancellation, truncated bodies, wrong hashes, wrong app identity/version, staging cleanup, interrupted startup and restore behavior. These tests pass on the desktop host. A real Switch TLS request, SD-card interruption and boot recovery have not been exercised on hardware.
