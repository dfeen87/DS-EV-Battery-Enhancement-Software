# DS REST API Documentation

## Overview

The DS REST API provides read-only HTTP/JSON access to the DS Battery Management, Torque Enhancement, and Regenerative Braking system state. The API is designed for observability and monitoring without any control surfaces, making it safe for automotive use.

## Key Features

- **Read-Only**: All endpoints are GET requests; no state modification possible
- **Thread-Safe**: All data access is mutex-protected
- **Non-Blocking**: Runs in dedicated thread; does not interfere with real-time control
- **Lightweight**: Minimal dependencies; uses POSIX sockets
- **LAN-Wide Access**: Binds to 0.0.0.0:8080 by default

## Server Configuration

**Default Bind Address**: `0.0.0.0:8080`

The server can be configured to bind to a different address/port during initialization:

```cpp
ds::api::RestApiServer server("0.0.0.0", 8080);
server.start();
```

## API Endpoints

### GET /health

Health check endpoint for monitoring server status.

**Response Example**:
```json
{
  "status": "ok",
  "version": "1.0.0",
  "timestamp_ms": 1676342964123.45,
  "total_requests": 1234,
  "successful_requests": 1230,
  "failed_requests": 4
}
```

**Fields**:
- `status`: Server status (always "ok" if responding)
- `version`: API server version
- `timestamp_ms`: Current server timestamp in milliseconds
- `total_requests`: Total number of requests received
- `successful_requests`: Number of successful (200) responses
- `failed_requests`: Number of failed (4xx/5xx) responses

---

### GET /api/battery

Returns the complete battery state including physical (Ψ), informational (Φ), health, and energy metrics.

**Response Example**:
```json
{
  "timestamp_ms": 1676342964123.45,
  "physical_state": {
    "voltage_v": 385.5,
    "current_a": -150.3,
    "temperature_c": 32.5,
    "state_of_charge": 0.72
  },
  "informational_state": {
    "entropy": 0.0234,
    "cycle_count": 123.45,
    "degradation": 0.05,
    "phi_magnitude": 0.0156
  },
  "health": {
    "remaining_capacity_percent": 95.2,
    "cycles_to_80_percent": 1850.5,
    "estimated_eol_cycles": 2500.0,
    "confidence": 0.92,
    "warning_triggered": false
  },
  "energy": {
    "energy_psi_j": 270000000.0,
    "energy_phi_j": 125.5,
    "energy_metric_j": 45.2,
    "energy_total_j": 270000170.7
  }
}
```

**Physical State (Ψ)** - Directly measurable quantities:
- `voltage_v`: Pack voltage in volts
- `current_a`: Pack current in amperes (negative = discharge, positive = charge)
- `temperature_c`: Average pack temperature in Celsius
- `state_of_charge`: State of charge (0.0 = empty, 1.0 = full)

**Informational State (Φ)** - DS-computed informational metrics:
- `entropy`: Normalized entropy measure (information content)
- `cycle_count`: Equivalent full charge/discharge cycles
- `degradation`: Battery degradation factor (0.0 = new, 1.0 = end-of-life)
- `phi_magnitude`: Magnitude of informational state vector

**Health** - Predictive health metrics:
- `remaining_capacity_percent`: Estimated remaining capacity vs. new
- `cycles_to_80_percent`: Estimated cycles until 80% capacity
- `estimated_eol_cycles`: Estimated total lifecycle
- `confidence`: Confidence in prediction (0.0 to 1.0)
- `warning_triggered`: Early degradation warning flag

**Energy** - Energy conservation tracking (Thermodynamic principle):
- `energy_psi_j`: Energy in physical state (Joules)
- `energy_phi_j`: Energy in informational state (Joules)
- `energy_metric_j`: Energy in metric coupling (Joules)
- `energy_total_j`: Total energy (should be conserved)

---

### GET /api/torque

Returns torque limits and derating information from the DS Torque Enhancement module.

**Response Example**:
```json
{
  "timestamp_ms": 1676342964123.45,
  "max_drive_torque_nm": 380.5,
  "max_regen_torque_nm": 220.0,
  "max_power_kw": 245.8,
  "scaling_factors": {
    "base_motor": 1.0,
    "ds": 0.98,
    "thermal": 0.95,
    "soc": 1.0,
    "health": 0.99,
    "cell_balancing": 1.0,
    "overall": 0.92
  },
  "derate_active": {
    "health": false,
    "thermal": true,
    "entropy": false,
    "metric": false,
    "soc": false,
    "weak_cell": false
  },
  "limiting_factor": "THERMAL",
  "confidence_level": 0.95
}
```

**Fields**:
- `max_drive_torque_nm`: Maximum drive torque (propulsion) in Newton-meters
- `max_regen_torque_nm`: Maximum regenerative braking torque in Newton-meters
- `max_power_kw`: Maximum electrical power in kilowatts
- `scaling_factors`: Individual derating factors (0.0 to 1.0)
  - `base_motor`: Base motor curve scaling
  - `ds`: DS-based reduction
  - `thermal`: Temperature derating
  - `soc`: State-of-charge protection
  - `health`: Long-term health preservation
  - `cell_balancing`: Weak cell protection
  - `overall`: Combined scaling factor (product of all)
- `derate_active`: Boolean flags indicating which derating mechanisms are active
- `limiting_factor`: Description of primary limiting factor (string)
- `confidence_level`: Confidence in DS prediction (0.0 to 1.0)

---

### GET /api/regen

Returns regenerative braking limits and charge acceptance constraints.

**Response Example**:
```json
{
  "timestamp_ms": 1676342964123.45,
  "max_regen_torque_nm": 220.0,
  "regen_fraction": 0.85,
  "limiting_factor": "SOC",
  "derate_factors": {
    "f_speed": 1.0,
    "f_soc": 0.85,
    "f_voltage": 1.0,
    "f_temp": 1.0,
    "f_cell": 1.0,
    "f_ds": 0.98,
    "f_stability": 1.0
  },
  "events": {
    "abs_events": 0,
    "slip_events": 0,
    "safety_blocks": 0
  }
}
```

**Fields**:
- `max_regen_torque_nm`: Maximum regenerative braking torque in Newton-meters
- `regen_fraction`: Recommended fraction of braking to assign to regen (0.0 to 1.0)
- `limiting_factor`: Description of primary limiting factor (string)
- `derate_factors`: Individual derating factors (0.0 to 1.0)
  - `f_speed`: Speed-dependent derating
  - `f_soc`: State-of-charge headroom constraint
  - `f_voltage`: Pack voltage headroom constraint
  - `f_temp`: Temperature-based derating
  - `f_cell`: Cell-level balance/health derating
  - `f_ds`: DS stress indicator derating
  - `f_stability`: ABS/ESC stability reduction
- `events`: Counters for safety-related events
  - `abs_events`: Number of ABS activation events
  - `slip_events`: Number of wheel slip events
  - `safety_blocks`: Number of hard safety gate blocks

---

### GET /api/limits

Returns combined limits from battery, torque, and regen systems in a compact format.

**Response Example**:
```json
{
  "timestamp_ms": 1676342964123.45,
  "battery": {
    "soc": 0.72,
    "max_charge_current_a": 200.0,
    "max_discharge_current_a": 400.0
  },
  "torque": {
    "max_drive_torque_nm": 380.5,
    "max_regen_torque_nm": 220.0,
    "max_power_kw": 245.8
  },
  "regen": {
    "max_regen_torque_nm": 220.0,
    "regen_fraction": 0.85
  }
}
```

**Fields**:
- `battery`: Battery-level limits
  - `soc`: Current state of charge (0.0 to 1.0)
  - `max_charge_current_a`: Maximum charging current (amperes)
  - `max_discharge_current_a`: Maximum discharge current (amperes)
- `torque`: Torque limits
  - `max_drive_torque_nm`: Maximum drive torque (Newton-meters)
  - `max_regen_torque_nm`: Maximum regen torque (Newton-meters)
  - `max_power_kw`: Maximum power (kilowatts)
- `regen`: Regenerative braking limits
  - `max_regen_torque_nm`: Maximum regen torque (Newton-meters)
  - `regen_fraction`: Recommended regen fraction (0.0 to 1.0)

---

### GET /api/diagnostics

Returns pack diagnostics including weak cells, imbalance, thermal spread, and DS metrics.

**Response Example**:
```json
{
  "timestamp_ms": 1676342964123.45,
  "pack_health_percent": 95.2,
  "estimated_remaining_cycles": 1850.5,
  "degradation_rate_per_cycle": 0.00028,
  "weak_cell_ids": [12, 45],
  "voltage_imbalance_mv": 35.5,
  "temperature_spread_celsius": 8.2,
  "warnings": {
    "degradation": false,
    "thermal": false,
    "imbalance": false,
    "weak_cell": true
  },
  "ds_metrics": {
    "metric_trace": 4.0234,
    "phi_magnitude": 0.0156,
    "entropy_level": 0.0234,
    "confidence": 0.92
  },
  "last_update_time_ms": 0.85,
  "average_update_time_ms": 0.72
}
```

**Fields**:
- `pack_health_percent`: Overall pack health percentage
- `estimated_remaining_cycles`: Estimated remaining charge cycles
- `degradation_rate_per_cycle`: Per-cycle degradation rate
- `weak_cell_ids`: Array of cell IDs identified as weak (empty for single-cell mode)
- `voltage_imbalance_mv`: Maximum cell voltage imbalance in millivolts
- `temperature_spread_celsius`: Temperature spread across pack in Celsius
- `warnings`: Boolean flags for active warnings
  - `degradation`: Early degradation detected
  - `thermal`: Thermal issue detected
  - `imbalance`: Cell imbalance issue
  - `weak_cell`: Weak cell detected
- `ds_metrics`: DS-specific metrics
  - `metric_trace`: Trace of effective metric tensor
  - `phi_magnitude`: Magnitude of informational state
  - `entropy_level`: Normalized entropy level
  - `confidence`: Overall DS confidence (0.0 to 1.0)
- `last_update_time_ms`: Most recent update computation time (milliseconds)
- `average_update_time_ms`: Average update computation time (milliseconds)

---

### GET /api/energy_cycle

Returns energy cycle metrics including drive, regen, losses, and efficiency.

**Response Example**:
```json
{
  "timestamp_ms": 1676342964123.45,
  "drive_energy_kwh": 12.45,
  "regen_energy_kwh": 2.34,
  "loss_energy_kwh": 1.23,
  "delta_info_energy_j": 125.5,
  "total_cycles": 123.45,
  "efficiency_percent": 18.8,
  "safety": {
    "thermal_ok": true,
    "soc_ok": true,
    "cell_protection_ok": true,
    "voltage_ok": true,
    "current_ok": true
  }
}
```

**Fields**:
- `drive_energy_kwh`: Total drive (discharge) energy in kilowatt-hours
- `regen_energy_kwh`: Total regenerative energy captured in kilowatt-hours
- `loss_energy_kwh`: Total energy losses in kilowatt-hours
- `delta_info_energy_j`: Change in informational energy (Thermodynamic term) in Joules
- `total_cycles`: Total equivalent full charge/discharge cycles
- `efficiency_percent`: Regenerative efficiency (regen/drive × 100)
- `safety`: Safety status flags
  - `thermal_ok`: Temperature within safe range
  - `soc_ok`: State of charge within safe range
  - `cell_protection_ok`: No cell-level protection issues
  - `voltage_ok`: Voltage within safe range
  - `current_ok`: Current within safe range

---

## Error Responses

All error responses follow this format:

```json
{
  "error_code": 404,
  "error_message": "Not Found"
}
```

**Common Error Codes**:
- `404 Not Found`: Endpoint does not exist
- `405 Method Not Allowed`: Only GET requests are supported
- `500 Internal Server Error`: Server-side error (rare)

---

## Integration Guide

### C++ Integration

```cpp
#include "ds_rest_api.hpp"

// Initialize API server
ds::api::RestApiServer api_server("0.0.0.0", 8080);
api_server.start();

// In your control loop...
while (running) {
    // Update battery state
    auto enhanced_state = middleware.enhance_cycle(...);
    
    // Compute torque limits
    auto torque_result = torque_mgr.compute_torque_limit(...);
    
    // Compute regen limits
    auto regen_result = regen_mgr.compute_regen_limit(...);
    
    // Get diagnostics
    auto diagnostics = middleware.get_diagnostics();
    
    // Update API server (thread-safe)
    api_server.update_state(enhanced_state, torque_result, regen_result, diagnostics);
    
    // Optional: update energy metrics
    api_server.update_energy_cycle(drive_kwh, regen_kwh, loss_kwh, 
                                   delta_info_j, cycles, efficiency_pct);
}

// Clean shutdown
api_server.stop();
```

### Python Client

```python
import requests

# Fetch battery state
response = requests.get('http://localhost:8080/api/battery')
battery = response.json()

print(f"SOC: {battery['physical_state']['state_of_charge'] * 100:.1f}%")
print(f"Voltage: {battery['physical_state']['voltage_v']:.1f} V")
print(f"Current: {battery['physical_state']['current_a']:.1f} A")
```

### JavaScript Client

```javascript
fetch('http://localhost:8080/api/battery')
  .then(response => response.json())
  .then(data => {
    console.log(`SOC: ${data.physical_state.state_of_charge * 100}%`);
    console.log(`Voltage: ${data.physical_state.voltage_v} V`);
  });
```

---

## Security Considerations

1. **Read-Only**: The API provides no write operations; state cannot be modified remotely
2. **No Authentication**: Currently, the API has no authentication (designed for trusted LAN)
3. **Network Isolation**: Deploy on isolated vehicle networks, not public internet
4. **Firewall**: Use firewall rules to restrict access to authorized clients only
5. **Rate Limiting**: No built-in rate limiting; use external tools if needed

---

## Performance Characteristics

- **Request Latency**: < 1 ms typical (local mutex lock + JSON serialization)
- **Thread Safety**: All state access is mutex-protected
- **Non-Blocking**: Server runs in dedicated thread; does not block control loops
- **Memory Footprint**: Minimal (single state snapshot + HTTP server overhead)
- **CPU Usage**: Negligible (< 0.1% on typical embedded processor)

---

## Troubleshooting

### Server fails to start

**Problem**: `Failed to start API server`

**Possible Causes**:
- Port 8080 already in use
- Insufficient permissions to bind to port
- Network interface not available

**Solutions**:
- Use a different port: `RestApiServer("0.0.0.0", 8081)`
- Check for conflicting services: `netstat -tuln | grep 8080`
- Run with appropriate permissions

### Connection refused

**Problem**: Client cannot connect to server

**Possible Causes**:
- Server not running
- Firewall blocking connection
- Wrong IP address/port

**Solutions**:
- Verify server is running
- Check firewall rules: `sudo ufw allow 8080`
- Verify bind address and port match client configuration

### Empty or stale data

**Problem**: API returns default/zero values

**Possible Causes**:
- `update_state()` not called from control loop
- State update happening less frequently than expected

**Solutions**:
- Ensure `update_state()` is called regularly in control loop
- Check control loop timing and frequency

---

## Examples

See the `examples/` directory for complete working examples:

- `rest_api_server.cpp`: Complete server application with simulated vehicle
- `rest_api_client.py`: Python client with monitoring mode
- `Makefile` or CMakeLists.txt: Build configuration

---

## License

Licensed under the MIT License - See LICENSE file for details

## Authors

Don Michael Feeney Jr.

## Version

4.0.0
