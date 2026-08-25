#!/usr/bin/env python3
"""Static contracts for hardware API ordering that host tests cannot execute."""

from pathlib import Path


REPO_DIR = Path(__file__).resolve().parent.parent
PROBE = REPO_DIR / "examples/ModbusRTUMasterProbe/ModbusRTUMasterProbe.ino"


def function_body(source: str, signature: str) -> str:
    signature_offset = source.find(signature)
    if signature_offset < 0:
        raise AssertionError(f"missing function: {signature}")
    body_start = source.find("{", signature_offset + len(signature))
    if body_start < 0:
        raise AssertionError(f"missing function body: {signature}")

    depth = 0
    for offset in range(body_start, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[body_start + 1 : offset]
    raise AssertionError(f"unterminated function body: {signature}")


source = PROBE.read_text(encoding="utf-8")
configure_body = function_body(source, "void configureModbusBaud(uint32_t baud)")

# `modbus.begin()` does not start a Stream. Reconfiguration must stop RX, apply
# SoftwareSerial's physical baud, and only then update Modbus timing.
ordered_statements = (
    "mySerial.end();",
    "mySerial.begin(baud);",
    "modbus.begin(baud);",
)
positions = [configure_body.find(statement) for statement in ordered_statements]
if any(position < 0 for position in positions):
    raise AssertionError(
        "configureModbusBaud must stop, start, and configure the transport"
    )
if positions != sorted(positions) or len(set(positions)) != len(positions):
    raise AssertionError(
        "configureModbusBaud ordering must be end -> serial begin -> Modbus begin"
    )

if source.count("mySerial.begin(baud);") != 1:
    raise AssertionError("SoftwareSerial baud ownership must remain centralized")
if source.count("modbus.begin(baud);") != 1:
    raise AssertionError("Modbus baud ownership must remain centralized")
if source.count("configureModbusBaud(baud);") != 2:
    raise AssertionError(
        "Probe must configure both its EEPROM startup baud and interactive changes"
    )
setup_body = function_body(source, "void setup()")
select_baud_body = function_body(source, "void selectBaud()")
if setup_body.count("configureModbusBaud(baud);") != 1:
    raise AssertionError("Probe setup must start SoftwareSerial at its EEPROM baud")
if select_baud_body.count("configureModbusBaud(baud);") != 1:
    raise AssertionError("Probe baud selection must reconfigure SoftwareSerial")

print("Example contract: Probe starts and reconfigures SoftwareSerial before Modbus")
