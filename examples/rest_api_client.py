#!/usr/bin/env python3
"""
DS REST API Client Example

This script demonstrates how to interact with the DS REST API server
from Python. It can be used for monitoring, logging, or building
dashboard applications.

Usage:
    python rest_api_client.py

Requirements:
    - Python 3.6+
    - requests library (pip install requests)
"""

import requests
import json
import time
import sys
from typing import Dict, Any, Optional

class DSApiClient:
    """Client for the DS REST API"""
    
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url.rstrip('/')
        self.session = requests.Session()
        self.session.headers.update({'Accept': 'application/json'})
    
    def _get(self, endpoint: str) -> Optional[Dict[str, Any]]:
        """Make a GET request to the API"""
        url = f"{self.base_url}{endpoint}"
        try:
            response = self.session.get(url, timeout=5.0)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            print(f"Error fetching {endpoint}: {e}", file=sys.stderr)
            return None
    
    def get_health(self) -> Optional[Dict[str, Any]]:
        """GET /health - Check server health"""
        return self._get('/health')
    
    def get_battery(self) -> Optional[Dict[str, Any]]:
        """GET /api/battery - Get battery state"""
        return self._get('/api/battery')
    
    def get_torque(self) -> Optional[Dict[str, Any]]:
        """GET /api/torque - Get torque limits"""
        return self._get('/api/torque')
    
    def get_regen(self) -> Optional[Dict[str, Any]]:
        """GET /api/regen - Get regen limits"""
        return self._get('/api/regen')
    
    def get_limits(self) -> Optional[Dict[str, Any]]:
        """GET /api/limits - Get combined limits"""
        return self._get('/api/limits')
    
    def get_diagnostics(self) -> Optional[Dict[str, Any]]:
        """GET /api/diagnostics - Get pack diagnostics"""
        return self._get('/api/diagnostics')
    
    def get_energy_cycle(self) -> Optional[Dict[str, Any]]:
        """GET /api/energy_cycle - Get energy cycle metrics"""
        return self._get('/api/energy_cycle')


def print_section(title: str, data: Dict[str, Any], indent: int = 0):
    """Pretty print a section of data"""
    prefix = "  " * indent
    print(f"{prefix}{title}:")
    for key, value in data.items():
        if isinstance(value, dict):
            print_section(key, value, indent + 1)
        elif isinstance(value, list):
            print(f"{prefix}  {key}: {value}")
        elif isinstance(value, float):
            print(f"{prefix}  {key}: {value:.4f}")
        else:
            print(f"{prefix}  {key}: {value}")


def monitor_mode(client: DSApiClient, interval: float = 1.0):
    """Continuous monitoring mode"""
    print("==========================================================")
    print("DS REST API - Monitoring Mode")
    print("==========================================================")
    print("Press Ctrl+C to stop...\n")
    
    try:
        iteration = 0
        while True:
            iteration += 1
            
            # Get battery state
            battery = client.get_battery()
            if not battery:
                print("Failed to fetch battery state")
                time.sleep(interval)
                continue
            
            # Get limits
            limits = client.get_limits()
            
            # Get diagnostics
            diagnostics = client.get_diagnostics()
            
            # Print compact status
            print(f"\n--- Iteration {iteration} ---")
            
            if battery:
                phys = battery.get('physical_state', {})
                info = battery.get('informational_state', {})
                health = battery.get('health', {})
                
                print(f"Battery:")
                print(f"  SOC:     {phys.get('state_of_charge', 0)*100:.2f}%")
                print(f"  Voltage: {phys.get('voltage_v', 0):.2f} V")
                print(f"  Current: {phys.get('current_a', 0):.2f} A")
                print(f"  Temp:    {phys.get('temperature_c', 0):.2f} °C")
                print(f"  Health:  {health.get('remaining_capacity_percent', 0):.2f}%")
                print(f"  Entropy: {info.get('entropy', 0):.4f}")
            
            if limits:
                torque = limits.get('torque', {})
                regen = limits.get('regen', {})
                
                print(f"\nLimits:")
                print(f"  Max Drive:     {torque.get('max_drive_torque_nm', 0):.2f} Nm")
                print(f"  Max Regen:     {torque.get('max_regen_torque_nm', 0):.2f} Nm")
                print(f"  Max Power:     {torque.get('max_power_kw', 0):.2f} kW")
                print(f"  Regen Fraction: {regen.get('regen_fraction', 0):.2f}")
            
            if diagnostics:
                warnings = diagnostics.get('warnings', {})
                ds = diagnostics.get('ds_metrics', {})
                
                print(f"\nDiagnostics:")
                print(f"  Pack Health:   {diagnostics.get('pack_health_percent', 0):.2f}%")
                print(f"  V Imbalance:   {diagnostics.get('voltage_imbalance_mv', 0):.2f} mV")
                print(f"  T Spread:      {diagnostics.get('temperature_spread_celsius', 0):.2f} °C")
                print(f"  DS Confidence: {ds.get('confidence', 0):.2f}")
                
                # Show active warnings
                active_warnings = [k for k, v in warnings.items() if v]
                if active_warnings:
                    print(f"  ⚠️  Warnings: {', '.join(active_warnings)}")
            
            time.sleep(interval)
            
    except KeyboardInterrupt:
        print("\n\nMonitoring stopped.")


def test_all_endpoints(client: DSApiClient):
    """Test all API endpoints"""
    print("==========================================================")
    print("DS REST API - Testing All Endpoints")
    print("==========================================================\n")
    
    # Test health endpoint
    print("Testing GET /health...")
    health = client.get_health()
    if health:
        print_section("Health", health)
        print("✓ Success\n")
    else:
        print("✗ Failed\n")
        return
    
    # Test battery endpoint
    print("Testing GET /api/battery...")
    battery = client.get_battery()
    if battery:
        print_section("Battery State", battery)
        print("✓ Success\n")
    else:
        print("✗ Failed\n")
    
    # Test torque endpoint
    print("Testing GET /api/torque...")
    torque = client.get_torque()
    if torque:
        print_section("Torque Limits", torque)
        print("✓ Success\n")
    else:
        print("✗ Failed\n")
    
    # Test regen endpoint
    print("Testing GET /api/regen...")
    regen = client.get_regen()
    if regen:
        print_section("Regen Limits", regen)
        print("✓ Success\n")
    else:
        print("✗ Failed\n")
    
    # Test limits endpoint
    print("Testing GET /api/limits...")
    limits = client.get_limits()
    if limits:
        print_section("Combined Limits", limits)
        print("✓ Success\n")
    else:
        print("✗ Failed\n")
    
    # Test diagnostics endpoint
    print("Testing GET /api/diagnostics...")
    diagnostics = client.get_diagnostics()
    if diagnostics:
        print_section("Pack Diagnostics", diagnostics)
        print("✓ Success\n")
    else:
        print("✗ Failed\n")
    
    # Test energy cycle endpoint
    print("Testing GET /api/energy_cycle...")
    energy = client.get_energy_cycle()
    if energy:
        print_section("Energy Cycle", energy)
        print("✓ Success\n")
    else:
        print("✗ Failed\n")
    
    print("==========================================================")
    print("All endpoint tests completed!")
    print("==========================================================\n")


def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(description='DS REST API Client')
    parser.add_argument('--url', default='http://localhost:8080',
                        help='Base URL of the API server (default: http://localhost:8080)')
    parser.add_argument('--monitor', action='store_true',
                        help='Run in continuous monitoring mode')
    parser.add_argument('--interval', type=float, default=1.0,
                        help='Monitoring interval in seconds (default: 1.0)')
    
    args = parser.parse_args()
    
    # Create client
    client = DSApiClient(args.url)
    
    # Check server is reachable
    print(f"Connecting to {args.url}...")
    health = client.get_health()
    if not health:
        print(f"ERROR: Cannot connect to server at {args.url}")
        print("Make sure the server is running (./rest_api_server)")
        sys.exit(1)
    
    print(f"✓ Connected to DS REST API v{health.get('version', 'unknown')}\n")
    
    # Run mode
    if args.monitor:
        monitor_mode(client, args.interval)
    else:
        test_all_endpoints(client)


if __name__ == '__main__':
    main()
