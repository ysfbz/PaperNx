RAR-only subset of selmf/unarr, commit 29084afa8d507b34558f2936ec789edcd116e306 (1.2.0).
Source archive SHA-256: e65612f22de473677ed63261a0436739140c79f6ed3dd5e61c94bc36752126f0
Source: https://github.com/selmf/unarr/tree/29084afa8d507b34558f2936ec789edcd116e306
LGPLv3; PPMd files carry public-domain notices. See COPYING and AUTHORS.

Local changes: generated unarr.h version macros; custom allocator state is C11 thread-local so library thumbnails and the reader can have separate memory budgets. The custom calloc checks zero count before division. Only RAR sources and their PPMd dependencies are compiled; no archive-writing support is included.
Encrypted or split RAR4 entries are rejected during indexing, before exposing an incomplete book.
