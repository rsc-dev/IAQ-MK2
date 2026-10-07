"""Tests for the summary-line parser of serial_dump.py. Run: make -C tests tools"""
from serial_dump import parse_summary

LINE = ("t=312 status=GOOD co2=684 co2_state=ok co2_cycles=309 co2_rejected=1 pm1=5 pm25=8 pm10=11 "
        "pm_state=ok pm_frames=298 pm_crc_err=0 aqi=1 tvoc=42 eco2=612 ens_validity=0 ens_state=ok "
        "temp=22.6 rh=46.1 press=1012.3 gas=86.2 env_state=ok")


def test_parses_summary_line_into_typed_values():
    r = parse_summary(LINE)
    assert r is not None
    assert r["t"] == 312
    assert r["status"] == "GOOD"
    assert r["co2"] == 684
    assert r["temp"] == 22.6
    assert r["env_state"] == "ok"


def test_ignores_event_and_garbage_lines():
    assert parse_summary("[boot] BME680 found") is None
    assert parse_summary("") is None
    assert parse_summary("t=5 status=BAD broken") is None  # token without '='


def test_summary_line_prefixed_by_capture_timestamp_is_still_parsed():
    r = parse_summary("  12.3s " + LINE)
    assert r is not None and r["pm25"] == 8
