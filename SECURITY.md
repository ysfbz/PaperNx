# Security Policy

## Supported Versions

Security updates and bug fixes are provided for the following releases:

| Version | Supported          |
| ------- | ------------------ |
| 0.9.x   | :white_check_mark: |
| < 0.9.0 | :x:                |

---

## Threat Model & Security Posture

**PaperNX** is designed as a **completely offline** application for the Nintendo Switch:
- **No Network Connectivity:** PaperNX does not open TCP/UDP sockets, perform HTTP/HTTPS requests, or communicate with external servers.
- **No Cloud Services or Accounts:** No personal identifiers, credentials, or telemetry data are collected or transmitted.
- **MicroSD Scope:** Application state is stored under `sdmc:/switch/PaperNX/data/`. The built-in browser can open user-selected documents from other permitted directories on the SD card; PaperNX does not claim strict OS-level filesystem sandboxing.
- **Memory-safety testing:** The v0.9.0 historical release-preparation report and the v0.9.1 repository test report record AddressSanitizer and UndefinedBehaviorSanitizer checks. Tests reduce risk but cannot certify that all malformed documents or device-specific execution paths are vulnerability-free.

---

## Reporting a Vulnerability

If you discover a security issue or memory corruption bug (e.g., a maliciously crafted EPUB or PDF file triggering an unchecked buffer overflow or denial of service), please report it responsibly:

1. **GitHub Security Advisory:** Submit a confidential report via the **Security** tab of the GitHub repository:  
   [https://github.com/ysfbz/PaperNx/security/advisories](https://github.com/ysfbz/PaperNx/security/advisories)
2. **Direct Contact:** Alternatively, reach out directly to the maintainer:  
   **Yusuf Buzrul** via GitHub: [@ysfbz](https://github.com/ysfbz)

Please include:
- A description of the vulnerability.
- A minimal proof-of-concept (sample document triggering the bug).
- The PaperNX version tested.

We will review reports promptly, reproduce the issue in an isolated environment, and publish a fix alongside an appropriate advisory.
