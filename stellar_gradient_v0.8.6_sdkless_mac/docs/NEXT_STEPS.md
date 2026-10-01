# Next steps — v0.10 Validation

Code-side validation is complete for native commit
`9f3e73bc92534941db1106f521786d8c3c792347`; Mac CI run `36911997718` is 8/8 PASS.

Current standard: AE Development Rules **v4.0.0**. Cosmic is the explicit
whole-product reference; Reference Audit is **PARTIAL** and the current gate is
real-AE Validation.

Next:
1. Use exact Actions artifact **11187002476**; do not rebuild a substitute.
2. Verify on-disk identity with `diagnostics/Stellar_Mac_Check.command`.
3. Verify loaded About Build ID equals `sg-0.10.0-9f3e73bc9253-clean-bc5efe6bf48a-aarch64-apple-darwin-36911997718.1`.
4. Run `diagnostics/Stellar_AE_Diagnostics_v1_3.jsx`.
5. Execute the parity/lifecycle matrix in `docs/MAC_TEST.md`, including
   PAR-005..PAR-010 gaps from `docs/REFERENCE_SPECIFICATION_COSMIC_V4.md`.
6. Profile real AE only after correctness passes.
7. Keep PR #1 draft. No merge/release/public delivery until mandatory Validation
   evidence is PASS for the unchanged artifact.
