"""Keep active Level-2 handoff tools pinned to the exact v0.10 validation artifact."""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).parent
TARGET = json.loads((ROOT / "validation_target_v010.json").read_text())

class TargetIdentityTests(unittest.TestCase):
    def test_active_tools_embed_exact_target(self):
        for name in (
            "Stellar_AE_Diagnostics_v1_3.jsx",
            "Stellar_Mac_Check.command",
            "Stellar_Cosmic_Compare.jsx",
        ):
            text = (ROOT / name).read_text()
            self.assertIn(TARGET["native_commit"], text, name)
            self.assertIn(TARGET["build_id"], text, name)

    def test_target_is_validation_not_release(self):
        self.assertEqual(TARGET["release_status"], "NOT_APPROVED_AE_GATES_PENDING")
        self.assertEqual(TARGET["version"], "0.10.0")
        self.assertEqual(TARGET["artifact_id"], 11187002476)
        self.assertEqual(TARGET["rules_version"], "4.0.0")
        self.assertEqual(TARGET["reference_audit_status"], "PARTIAL")

if __name__ == "__main__":
    unittest.main(verbosity=2)
