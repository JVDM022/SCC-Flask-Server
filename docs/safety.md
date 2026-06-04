# Safety Philosophy

The ESP32 owns hard safety. Hardware interlocks, manual kill, hard kill, and heater lockout must be implemented in firmware and electrical design so the rig remains safe when the server, database, network, ML model, or HMI fails.

Flask, ML, and MPC must never be the only safety layer. Backend alarm handling provides visibility and audit records, but it is not a substitute for firmware and hardware protection.

Server recommendations are advisory until explicitly enabled by a separate, reviewed control path. The MPC endpoint returns a recommended heater PWM for operator or research evaluation only.

Heater lockout, manual kill, and hard kill override everything. When any of these states is active, the advisory MPC returns `recommended_pwm = 0`.
