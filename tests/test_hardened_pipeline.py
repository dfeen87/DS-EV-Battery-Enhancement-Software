#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DS EV Battery Enhancement - Hardened Integration and Pipeline Test Suite
========================================================================

Verifies safety boundaries, limp/derate modes, installer safety gates,
and update/rollback workflows under simulated/hardware-agnostic conditions.
"""

import os
import sys
import unittest
import subprocess
import shutil
import json

# Add ds_core/lib to path so we can import pybind11 module if needed
sys.path.insert(0, os.path.abspath("ds_core/lib"))

class TestHardenedPipeline(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        # Build standard output first
        subprocess.run(["cmake", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"], stdout=subprocess.DEVNULL)
        subprocess.run(["cmake", "--build", "build"], stdout=subprocess.DEVNULL)

    def run_diag(self, args):
        """Runs the diagnostics script with given arguments and returns returncode and output."""
        cmd = [sys.executable, "scripts/diagnostics.py"] + args
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return res.returncode, res.stdout, res.stderr

    # ========================================================================
    # 1. Safety Boundaries
    # ========================================================================

    def test_soh_safety_boundaries(self):
        # SOH below threshold (79.9%) -> must fail/exit 4
        code, out, err = self.run_diag(["--soh", "79.9"])
        self.assertEqual(code, 4)
        self.assertIn("below safe enhancement threshold", out)

        # SOH at/above threshold (80.0%) -> should pass
        code, out, err = self.run_diag(["--soh", "80.0"])
        self.assertEqual(code, 0)

        # SOH clearly above (90.0%) -> should pass
        code, out, err = self.run_diag(["--soh", "90.0"])
        self.assertEqual(code, 0)

    def test_temperature_safety_boundaries(self):
        # Temperature just above limit (45.1 C) -> must fail/exit 4
        code, out, err = self.run_diag(["--temp", "45.1"])
        self.assertEqual(code, 4)
        self.assertIn("outside safe range", out)

        # Temperature just below limit (0.0 C) -> should pass
        code, out, err = self.run_diag(["--temp", "0.0"])
        self.assertEqual(code, 0)

        # Temperature just above limit (45.0 C) -> should pass
        code, out, err = self.run_diag(["--temp", "45.0"])
        self.assertEqual(code, 0)

        # Temperature clearly cold (-0.1 C) -> must fail/exit 4
        code, out, err = self.run_diag(["--temp", "-0.1"])
        self.assertEqual(code, 4)
        self.assertIn("outside safe range", out)

    def test_soc_safety_boundaries(self):
        # SOC just above upper limit (90.1%) -> must fail/exit 4
        code, out, err = self.run_diag(["--soc", "90.1"])
        self.assertEqual(code, 4)
        self.assertIn("outside ideal installation range", out)

        # SOC just below lower limit (19.9%) -> must fail/exit 4
        code, out, err = self.run_diag(["--soc", "19.9"])
        self.assertEqual(code, 4)
        self.assertIn("outside ideal installation range", out)

        # SOC at limits (20.0% and 90.0%) -> should pass
        code, out, err = self.run_diag(["--soc", "20.0"])
        self.assertEqual(code, 0)

        code, out, err = self.run_diag(["--soc", "90.0"])
        self.assertEqual(code, 0)

    def test_voltage_out_of_range(self):
        # For Tesla_Model_3_LR, nominal voltage range is 300V to 410V
        # Voltage below range (299.0V) -> must fail
        code, out, err = self.run_diag(["--model", "Tesla_Model_3_LR", "--voltage", "299.0"])
        self.assertEqual(code, 4)
        self.assertIn("outside nominal profile bounds", out)

        # Voltage above range (411.0V) -> must fail
        code, out, err = self.run_diag(["--model", "Tesla_Model_3_LR", "--voltage", "411.0"])
        self.assertEqual(code, 4)
        self.assertIn("outside nominal profile bounds", out)

        # Voltage in-range (355.0V) -> should pass
        code, out, err = self.run_diag(["--model", "Tesla_Model_3_LR", "--voltage", "355.0"])
        self.assertEqual(code, 0)

    # ========================================================================
    # 2. Limp / Derate Modes
    # ========================================================================

    def test_limp_and_derate_modes(self):
        try:
            import ds_enhancer_pybind as h
        except ImportError:
            self.skipTest("pybind11 module not found on path")

        # Create state with low SOH/degradation
        middleware = h.DSBMSMiddleware()
        middleware.init(75.0, 400.0)

        # A. Critical SOC (Limp Mode)
        # With SOC at 3% (below 5%), overall scaling should be aggressive derating (limp mode)
        enhanced = middleware.enhance_cycle(360.0, 50.0, 25.0, 0.03, 1.0)
        config = h.TorqueConfig()
        tm = h.DSTorqueManager(config)
        result = tm.compute_torque_limit(enhanced, 4000.0, 0.01)
        self.assertTrue(result.limp_mode_active)
        self.assertTrue(result.soc_derate_active)
        self.assertLess(result.overall_scaling, 0.4)

        # B. High Temperature (Thermal Derating)
        # With Temperature at 48 C (above 45 C soft limit), thermal derating must kick in
        enhanced_hot = middleware.enhance_cycle(360.0, 50.0, 48.0, 0.6, 1.0)
        result_hot = tm.compute_torque_limit(enhanced_hot, 4000.0, 0.01)
        self.assertTrue(result_hot.thermal_derate_active)
        self.assertLess(result_hot.thermal_scaling, 1.0)

        # C. Weak Cell Detection
        # Pass a diagnostic report containing a weak cell count and ID
        diag_report = h.DiagnosticReport()
        diag_report.weak_cell_warning = True
        diag_report.weak_cell_ids = [12, 45]
        diag_report.voltage_imbalance_mv = 75.0 # mV imbalance > 50.0mV triggers limiting

        result_weak = tm.compute_torque_limit(enhanced, 4000.0, 0.01, diag_report)
        self.assertTrue(result_weak.weak_cell_derate_active)
        self.assertLess(result_weak.cell_balancing_scaling, 1.0)

    # ========================================================================
    # 3. Installer safety-gating Behavior
    # ========================================================================

    def test_installer_safety_gating(self):
        # We can run Makefile install with env overrides under sudo
        # If SOH is set to 75% via env, install MUST be rejected
        env = os.environ.copy()
        env["DS_SOH"] = "75.0"

        res = subprocess.run(["sudo", "-E", "make", "install"], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("diagnostics failed", res.stdout + res.stderr)

        # If metrics are nominal (e.g. SOH 92.5%), install should succeed
        env_ok = os.environ.copy()
        env_ok["DS_SOH"] = "92.5"
        res_ok = subprocess.run(["sudo", "-E", "make", "install"], env=env_ok, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res_ok.returncode, 0)
        self.assertIn("DS Software Suite successfully deployed", res_ok.stdout)

    # ========================================================================
    # 4. Backup & Rollback behavior
    # ========================================================================

    def test_backup_and_rollback_flow(self):
        # 1. Take a safe install
        subprocess.run(["sudo", "make", "install"], stdout=subprocess.DEVNULL)

        # 2. Write a dummy config file inside /opt/ds_enhancement/config/dummy.json
        dummy_file = "/opt/ds_enhancement/config/dummy.json"
        with open(dummy_file, "w") as f:
            f.write('{"test": "rollback"}')

        # 3. Trigger backup
        subprocess.run(["sudo", "./scripts/rollback.sh", "backup"], stdout=subprocess.DEVNULL)

        # 4. Modify the file
        with open(dummy_file, "w") as f:
            f.write('{"test": "modified"}')

        # 5. Revert/Rollback
        subprocess.run(["sudo", "make", "rollback"], stdout=subprocess.DEVNULL)

        # 6. Read file - must be original '{"test": "rollback"}'
        with open(dummy_file, "r") as f:
            content = f.read()
        self.assertEqual(content, '{"test": "rollback"}')

        # Clean up dummy file
        subprocess.run(["sudo", "rm", "-f", dummy_file])

if __name__ == "__main__":
    unittest.main()
