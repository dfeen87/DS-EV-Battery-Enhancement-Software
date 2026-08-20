# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.

import os
import json
import unittest
from ds_core.python.ds_ev_enhancer import (
    compute_mechanical_hp,
    compute_electrical_hp,
    hp_consistency_score,
    evaluate_governance,
    DSEVEnhancerOrchestrator,
)
from ds_core.python.ailee.core_min import (
    AileeTrustPipeline,
    AileeConfig,
    GovernanceLevel,
    GovernanceDecision,
)
from ds_core.python.ailee.domains.automotive.ailee_automotive_domain import (
    AileeAutomotiveDomain,
    AutonomyLevel,
)


class TestAileeAutomotiveDomain(unittest.TestCase):

    def test_hp_calculations(self):
        # 400 Nm @ 4000 RPM -> 224.68 HP
        hp_mech = compute_mechanical_hp(400.0, 4000.0)
        self.assertAlmostEqual(hp_mech, 224.681, places=2)

        # 400V, 400A -> 160 kW -> 214.56 HP
        hp_elec = compute_electrical_hp(400.0, 400.0)
        self.assertAlmostEqual(hp_elec, 214.563, places=2)

        score = hp_consistency_score(hp_mech, hp_elec)
        self.assertGreater(score, 0.90)

    def test_governance_normal_mode(self):
        res = evaluate_governance(
            torque_nm=300.0,
            rpm=4000.0,
            v_batt=380.0,
            i_batt=350.0,
            soc=85.0,
            soh=95.0,
            temp_c=30.0,
            sensor_valid=True,
        )
        self.assertEqual(res["level"], 0)
        self.assertGreaterEqual(res["trust_score"], 0.85)
        self.assertFalse(res["used_fallback"])

    def test_governance_thermal_derating(self):
        res = evaluate_governance(
            torque_nm=300.0,
            rpm=4000.0,
            v_batt=380.0,
            i_batt=350.0,
            soc=85.0,
            soh=95.0,
            temp_c=52.0,  # High temperature
            sensor_valid=True,
        )
        self.assertGreaterEqual(res["level"], 1)
        self.assertLess(res["governed_hp"], 200.0)

    def test_governance_sensor_fault_fallback(self):
        res = evaluate_governance(
            torque_nm=300.0,
            rpm=4000.0,
            v_batt=380.0,
            i_batt=350.0,
            soc=85.0,
            soh=95.0,
            temp_c=30.0,
            sensor_valid=False,  # Sensor fault
        )
        self.assertEqual(res["level"], 3)
        self.assertTrue(res["used_fallback"])

    def test_audit_logging(self):
        test_log_path = os.path.join(os.path.dirname(__file__), "../logs/test_audit.log")
        if os.path.exists(test_log_path):
            os.remove(test_log_path)

        orchestrator = DSEVEnhancerOrchestrator(log_path=test_log_path)
        res = orchestrator.evaluate_signals(
            torque_nm=350.0,
            rpm=4200.0,
            v_batt=390.0,
            i_batt=380.0,
            soc=80.0,
            soh=90.0,
            temp_c=28.0,
        )

        self.assertTrue(os.path.exists(test_log_path))
        with open(test_log_path, "r", encoding="utf-8") as f:
            lines = f.readlines()
            self.assertGreater(len(lines), 0)
            log_data = json.loads(lines[-1])
            self.assertEqual(log_data["level"], res["level"])
            self.assertIn("timestamp", log_data)

        if os.path.exists(test_log_path):
            os.remove(test_log_path)


if __name__ == "__main__":
    unittest.main()
