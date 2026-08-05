/*
 * ============================================================================
 * DS EV Battery Enhancement Core - CLI & Shared Library Implementation
 * ============================================================================
 */

#include "ds_battery_enhancement.hpp"
#include "ds_bms_middleware_v2.hpp"
#include "torque_enhancement.hpp"
#include "ds_regen_braking_manager_v1.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iomanip>
#include <sstream>

// ============================================================================
// EXTERN "C" INTERFACE FOR PYTHON CTYPES WRAPPER (Option B)
// ============================================================================

extern "C" {

struct DSResult {
    double degradation;
    double remaining_capacity_percent;
    double cycles_to_80_percent;
    double ds_confidence;
    double recommended_current_limit;
    double recommended_voltage_limit;
    double recommended_temperature;
    double estimated_charge_time;
    int degradation_warning;
    int input_clamped;
    int numerical_stability;
};

void* ds_create(double capacity_ah, double nominal_voltage_v) {
    try {
        auto* middleware = new ds_plugin::DSBMSMiddleware();
        middleware->init(capacity_ah, nominal_voltage_v);
        return static_cast<void*>(middleware);
    } catch (...) {
        return nullptr;
    }
}

void ds_destroy(void* handle) {
    if (handle) {
        delete static_cast<ds_plugin::DSBMSMiddleware*>(handle);
    }
}

int ds_enhance(void* handle, double voltage, double current, double temp, double soc, double dt, DSResult* out_result) {
    if (!handle || !out_result) return -1;

    try {
        auto* middleware = static_cast<ds_plugin::DSBMSMiddleware*>(handle);
        auto enhanced = middleware->enhance_cycle(voltage, current, temp, soc, dt);

        out_result->degradation = enhanced.state.degradation;
        out_result->remaining_capacity_percent = enhanced.health.remaining_capacity_percent;
        out_result->cycles_to_80_percent = enhanced.health.cycles_to_80_percent;
        out_result->ds_confidence = enhanced.ds_confidence;
        out_result->recommended_current_limit = enhanced.charging.recommended_current_limit;
        out_result->recommended_voltage_limit = enhanced.charging.recommended_voltage_limit;
        out_result->recommended_temperature = enhanced.charging.recommended_temperature;
        out_result->estimated_charge_time = enhanced.charging.estimated_charge_time;
        out_result->degradation_warning = enhanced.degradation_warning ? 1 : 0;
        out_result->input_clamped = enhanced.input_clamped ? 1 : 0;
        out_result->numerical_stability = enhanced.numerical_stability ? 1 : 0;

        return 0; // Success
    } catch (...) {
        return -2; // Failure
    }
}

} // extern "C"

// ============================================================================
// COMMAND LINE INTERFACE (Option A)
// ============================================================================

#ifdef DS_BUILD_CLI

static void print_usage() {
    std::cout << "Usage: ds_enhancer [options]\n"
              << "Options:\n"
              << "  -v, --voltage <V>        Measured battery pack voltage (V)\n"
              << "  -c, --current <A>        Measured battery pack current (A)\n"
              << "  -t, --temp <C>           Measured battery temperature (°C)\n"
              << "  -s, --soc <0.0-1.0>      State of Charge fraction\n"
              << "  -d, --dt <sec>           Elapsed time delta in seconds\n"
              << "  --capacity <Ah>          Battery nominal capacity (default: 75.0)\n"
              << "  --nominal-voltage <V>    Battery nominal voltage (default: 400.0)\n"
              << "  --json                   Format output as raw JSON string\n"
              << "  --help                   Display this help message\n";
}

int main(int argc, char* argv[]) {
    double voltage = 355.0;
    double current = 50.0;
    double temp = 25.0;
    double soc = 0.65;
    double dt = 0.1;
    double capacity = 75.0;
    double nominal_voltage = 400.0;
    bool format_json = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-v" || arg == "--voltage") && i + 1 < argc) {
            voltage = std::stod(argv[++i]);
        } else if ((arg == "-c" || arg == "--current") && i + 1 < argc) {
            current = std::stod(argv[++i]);
        } else if ((arg == "-t" || arg == "--temp") && i + 1 < argc) {
            temp = std::stod(argv[++i]);
        } else if ((arg == "-s" || arg == "--soc") && i + 1 < argc) {
            soc = std::stod(argv[++i]);
        } else if ((arg == "-d" || arg == "--dt") && i + 1 < argc) {
            dt = std::stod(argv[++i]);
        } else if (arg == "--capacity" && i + 1 < argc) {
            capacity = std::stod(argv[++i]);
        } else if (arg == "--nominal-voltage" && i + 1 < argc) {
            nominal_voltage = std::stod(argv[++i]);
        } else if (arg == "--json") {
            format_json = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage();
            return 1;
        }
    }

    try {
        ds_plugin::DSBMSMiddleware middleware;
        middleware.init(capacity, nominal_voltage);

        auto enhanced = middleware.enhance_cycle(voltage, current, temp, soc, dt);
        auto diag = middleware.get_diagnostics();

        if (format_json) {
            std::cout << "{\n"
                      << "  \"version\": \"" << ds::DSEnhancement::get_version() << "\",\n"
                      << "  \"degradation\": " << enhanced.state.degradation << ",\n"
                      << "  \"remaining_capacity_percent\": " << enhanced.health.remaining_capacity_percent << ",\n"
                      << "  \"cycles_to_80_percent\": " << enhanced.health.cycles_to_80_percent << ",\n"
                      << "  \"ds_confidence\": " << enhanced.ds_confidence << ",\n"
                      << "  \"recommended_current_limit\": " << enhanced.charging.recommended_current_limit << ",\n"
                      << "  \"recommended_voltage_limit\": " << enhanced.charging.recommended_voltage_limit << ",\n"
                      << "  \"recommended_temperature\": " << enhanced.charging.recommended_temperature << ",\n"
                      << "  \"estimated_charge_time_min\": " << enhanced.charging.estimated_charge_time << ",\n"
                      << "  \"degradation_warning\": " << (enhanced.degradation_warning ? "true" : "false") << ",\n"
                      << "  \"numerical_stability\": " << (enhanced.numerical_stability ? "true" : "false") << ",\n"
                      << "  \"input_clamped\": " << (enhanced.input_clamped ? "true" : "false") << "\n"
                      << "}\n";
        } else {
            std::cout << "====================================================================\n"
                      << "DS BATTERY ENHANCER CLI - ANALYSIS RESULT\n"
                      << "====================================================================\n"
                      << std::fixed << std::setprecision(4)
                      << "Version:                      " << ds::DSEnhancement::get_version() << "\n"
                      << "Degradation Rate:             " << enhanced.state.degradation * 100.0 << " %\n"
                      << "Remaining SOH Capacity:       " << enhanced.health.remaining_capacity_percent << " %\n"
                      << "Estimated Cycles to 80% SOH:  " << enhanced.health.cycles_to_80_percent << "\n"
                      << "DS Core Model Confidence:    " << enhanced.ds_confidence * 100.0 << " %\n"
                      << "Optimal Charge Current Limit: " << enhanced.charging.recommended_current_limit << " A\n"
                      << "Optimal Charge Voltage Limit: " << enhanced.charging.recommended_voltage_limit << " V\n"
                      << "Target Charging Temp:         " << enhanced.charging.recommended_temperature << " °C\n"
                      << "Estimated Time to Charge:     " << enhanced.charging.estimated_charge_time << " min\n"
                      << "Warnings Triggered:           " << (enhanced.degradation_warning ? "YES ⚠️" : "NO ✓") << "\n"
                      << "Numerical Stability Check:    " << (enhanced.numerical_stability ? "PASSED ✓" : "FAILED ⚠️") << "\n"
                      << "Input Clamp Check:            " << (enhanced.input_clamped ? "CLAMPED ⚠️" : "NOMINAL ✓") << "\n"
                      << "====================================================================\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Execution error: " << e.what() << "\n";
        return 2;
    }

    return 0;
}

#endif // DS_BUILD_CLI
