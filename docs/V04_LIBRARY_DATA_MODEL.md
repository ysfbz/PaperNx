# PaperNX catalogue and reading statistics — updated for v0.9.1

The catalog lives in `data/catalog.txt`; reading statistics live in
`data/reading-stats.txt`. Neither file replaces or migrates `data/progress.txt`.
PAPERNX-1 through PAPERNX-6 reading locations, preferences and bookmarks continue
through the existing ProgressStore migration and backup path.

## Catalog schema

`PAPERNX-CATALOG-1` identifies the independent schema. Its storage transport is
ProgressStore's checksummed PAPERNX record envelope, flushed and verified before
replacement, with `.bak`, `.bak.old` and interrupted `.tmp` recovery. Catalog
records use distinct hashed 16-character transport IDs; original book IDs are
length-prefixed inside the record, avoiding delimiter and Unicode ambiguity.

Each book has user overrides for title, author, genre, series and tags; multiple
collection IDs; a hidden flag; and a reading status independent of exact progress:
Automatic, NotStarted, Reading or Completed. Favorite remains in ProgressStore.
Manual status changes never move the saved position or remove bookmarks.

Embedded title, author, genre and series are cached separately from overrides.
The cache fingerprint is source byte size plus modification timestamp. Opening a
document refreshes embedded metadata. Explicit user values take precedence; a
stale embedded cache is ignored. Unknown authors and uncategorized books remain
visible through fallback groups. Embedded multi-author/subject values are stored
as display strings.

In v0.9.1, natural numeric sorting (`compareNatural`) is implemented across the
catalog, correctly ordering titles, series, volume numbers, and CBZ page numbers
(e.g., "Vol 2" precedes "Vol 10").

Collections have stable IDs and names. Names compare with Turkish Unicode case
folding to avoid duplicate shelves. Renaming retains memberships; removing a
collection removes its memberships and never touches any document file. A book
can belong to up to 32 collections. Up to 256 collections can be created.

Book identity retains the existing path-plus-size hash. Same-title books do not
collide merely because their titles match. Moving or renaming files does not yet
carry metadata to the new path automatically; content-addressed identity and
moved-file reconciliation remain deferred.

## Removal and permanent deletion

Hiding a book changes only its catalog hidden flag, persists across scans and can
be reversed in Browse Library → Hidden Books. It never deletes the source.

Permanent deletion requires a separate confirmation showing the filename and
source path. Cancellation is the initial focused action. The backend refuses an
unconfirmed request and validates the selection against a fresh scanner snapshot,
current size, ID and modification time. It requires a regular EPUB, PDF, or CBZ
file inside the configured library/browser root, rejects traversal, symlinks at
every path component, directories and unsupported configuration files. Native
`sdmc:/` prefixes use the shared component inspection helper, avoiding host-style
`absolute()` prefix corruption. Filesystem errors are surfaced to the user.

After deletion, the visible catalog entry is removed; progress and metadata remain
available for recovery/reimport. There is no trash and no bulk deletion. As with
portable C++17 filesystem operations generally, external concurrent filesystem
changes between validation and removal are not an atomic directory-handle
transaction. This limitation is relevant to desktop adversarial races; it is not
a claim of protection against a privileged concurrent process.

Only intentionally disposable book copies should be used for physical deletion
acceptance. All new hardware deletion checks are **UNVERIFIED**.

## Reading statistics

`PAPERNX-STATS-1` stores per-book active milliseconds, sessions, last reading date,
measured forward fraction, evidence sample count and furthest observed fraction.
Local ISO dates index an offline reading calendar. A daily minute goal is optional
(0 disables; supported range 0–720). The statistics screen shows real recorded
time and a configurable goal.

The application supplies monotonic elapsed timestamps. Menus, background state,
no active document and pending layout are excluded. Activity expires after 120
seconds without interaction; gaps exceeding three seconds between ticks are not
credited, preventing a suspended process from accumulating time on resume.
Statistics are checkpointed periodically and at background/exit boundaries using
the verified storage transport. Up to the current checkpoint interval can be
lost if power is removed abruptly.

Book time remaining appears only after at least two active minutes, three small
forward samples and 2% measured fraction. Large jumps do not become pace evidence;
rereading earlier positions cannot repeatedly inflate forward fraction. History,
Page Flip and layout operations can suppress the next pace sample through
`reanchor()`. Estimates describe the book, not a chapter; PDF estimates remain an
approximation based on observed page progress. No estimates are invented before
the evidence threshold. Chapter estimates, weekly summary/streak UI and annual
statistics are not implemented.

## Evidence

`tests/catalog_tests.cpp` covers Unicode, same-title identity separation,
multi-membership, hiding/rescan, collection lifecycle, injected read-only write
failure, backup recovery, independent progress/bookmark preservation, unsafe-path
rejection, cancellation and deletion of disposable files. It also exercises idle,
menu and suspend time exclusion, evidence gating, daily goals and persistence.

`tests/v04_catalog_workflow.hpp` drives actual SDL book actions, touch metadata
entry, grouping and collections, restoration, confirmed/cancelled deletion, PDF
Page Flip and application screenshots. Physical Switch acceptance remains
**UNVERIFIED** until the newly built NRO is tested.
