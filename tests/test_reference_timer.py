"""Integration checks for the real reference timer and harness failure handling."""
import contextlib
import hashlib
import io
from pathlib import Path
import platform
import subprocess
import sys
import unittest

import run_test_harness


ROOT = Path(__file__).resolve().parent.parent
WRAPPER = ROOT / "tests/run_reference.py"


@unittest.skipUnless(platform.system() == "Linux" and platform.machine() == "x86_64",
                     "Linux x86-64 timer repair")
class ReferenceTimerTests(unittest.TestCase):
    def test_actual_reference_timers_against_wall_time(self):
        for part in ("part_a", "part_b"):
            with self.subTest(part=part):
                binary = ROOT / part / "runtasks_ref_linux"
                before = hashlib.sha256(binary.read_bytes()).digest()
                result = subprocess.run([sys.executable, str(WRAPPER), "--check", str(binary)],
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("independent wall-time error", result.stderr)
                self.assertIn("; OK", result.stderr)
                self.assertEqual(before, hashlib.sha256(binary.read_bytes()).digest())

    def test_incompatible_binary_is_rejected(self):
        result = subprocess.run([sys.executable, str(WRAPPER), "/bin/true"],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 125)
        self.assertIn("old timer symbols are missing", result.stderr)


class HarnessFailureTests(unittest.TestCase):
    def test_missing_results_cannot_pass(self):
        with self.assertRaises(SystemExit):
            run_test_harness.run_test([sys.executable, "-c",
                                      "print('[Serial]: [1.000] ms')"], True)

    def test_failed_process_cannot_pass(self):
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit):
            run_test_harness.run_test([sys.executable, "-c", "raise SystemExit(125)"], True)


if __name__ == "__main__":
    unittest.main()
