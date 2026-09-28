# Native host dependency lock

2026-09-28. Cargo.toml is byte-identical to the manifest on `feature/ae-hot-loader-shell` at `aef5b9b3270bc9132492acb7cb2754d1e8f2dd75` (blob `f70f74ae06d1ae3fe56062a76ccd095af98b9c63`). Reuse that branch's existing Cargo.lock (blob `e5bacba6640f714120760a6a1ad3f04ab1734f6f`) rather than resolve fresh versions or hand-edit registry checksums. No Hot Loader code is imported.

The next build-identity change will enforce `--locked` and synchronize the package version. Validation of this lock in the Stellar-only CI is required; the other branch's passing CI is provenance, not a substitute for current validation.

Preceding stage completed: Mac CI #35, run 36449152224, commit 058d8f7103a7dd2140beb5257ca741855daa188d: all eight jobs PASS. This confirms code-side checks and signing/packaging only. Real After Effects, Metal execution parity and release gates remain unverified.
