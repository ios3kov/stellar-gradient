# Third-party host/build components

The render implementation remains Stellar Gradient clean-room code.

The SDK-less After Effects host uses:

- `after-effects` 0.4.x — Apache-2.0 OR BSD-3-Clause OR MIT OR Zlib
- `pipl` 0.1.x — MIT OR Apache-2.0
- `cc` — MIT OR Apache-2.0

These packages provide Adobe host ABI/build plumbing and do not contain Cosmic implementation code.
For distribution, retain the notices/licenses required by the chosen license option for each dependency and freeze the exact resolved set in `Cargo.lock` after the first successful Mac build.
