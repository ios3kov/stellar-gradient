"""Explicit report-write failure regression; imports the existing fixture setup."""
import subprocess
import unittest
from test_mac_check import CollectorTests, HARNESS


class ReportWriteTest(unittest.TestCase):
    def test_no_success_message_after_write_failure(self):
        fixture = CollectorTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        harness = HARNESS.replace('\nsg_main\n', '\nsg_report() { return 1; }\nsg_main\n')
        result = subprocess.run(['/bin/bash', '-c', harness], env=fixture.env,
                                capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 4)
        self.assertNotIn('Report created', result.stdout)


if __name__ == '__main__':
    unittest.main(defaultTest='ReportWriteTest', verbosity=2)
