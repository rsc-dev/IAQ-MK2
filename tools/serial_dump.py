# /// script
# requires-python = ">=3.10"
# dependencies = ["pyserial>=3.5", "typer>=0.12"]
# ///
"""Dump live IAQ-MK2 readings from the USB serial log.

Prints the firmware's `key=value` summary lines as a table (default) or every
line verbatim (--raw), optionally appending parsed readings to a CSV file.
Survives the board re-enumerating (for example during an upload).

    uv run tools/serial_dump.py                       # /dev/ttyACM0 until Ctrl-C
    uv run tools/serial_dump.py --seconds 60 --csv readings.csv
    uv run tools/serial_dump.py --raw
"""
from __future__ import annotations

import csv
import os
import re
import time
from pathlib import Path
from typing import Optional

import serial
import typer

app = typer.Typer(add_completion=False)

_NUMBER = re.compile(r"^-?\d+(\.\d+)?$")
_CAPTURE_STAMP = re.compile(r"^\d+(\.\d+)?s\s+(.*)$")

COLUMNS: list[tuple[str, int]] = [
    ("t", 7), ("status", 6), ("co2", 5), ("co2_state", 9), ("pm1", 4), ("pm25", 4), ("pm10", 4),
    ("pm_state", 8), ("aqi", 3), ("tvoc", 5), ("eco2", 5), ("ens_state", 9), ("temp", 5),
    ("rh", 5), ("press", 6), ("gas", 6), ("env_state", 9), ("wifi", 10), ("time", 5),
    ("wx_temp", 7), ("wx_sym", 8), ("wx_rain6h", 9), ("wx_state", 8),
]


def parse_summary(line: str) -> Optional[dict]:
    """Parse one `t=... key=value ...` summary line into typed values; None for other lines."""
    text = line.strip()
    stamped = _CAPTURE_STAMP.match(text)
    if stamped:
        text = stamped.group(2)
    if not text.startswith("t="):
        return None
    out: dict = {}
    for token in text.split():
        if "=" not in token:
            return None
        key, value = token.split("=", 1)
        if _NUMBER.match(value):
            out[key] = float(value) if "." in value else int(value)
        else:
            out[key] = value
    return out


def _header() -> str:
    return " ".join(name.rjust(width) for name, width in COLUMNS)


def _row(reading: dict) -> str:
    return " ".join(str(reading.get(name, "-")).rjust(width) for name, width in COLUMNS)


def _open_port(port: str, deadline: float) -> serial.Serial:
    while True:
        try:
            if os.path.exists(port):
                return serial.Serial(port, 115200, timeout=0.5)
        except (serial.SerialException, OSError):
            pass
        if time.time() > deadline:
            raise typer.Exit(code=typer.echo(f"could not open {port}", err=True) or 1)
        time.sleep(0.2)


@app.command()
def main(
    port: str = typer.Option("/dev/ttyACM0", "--port", "-p", help="serial port of the station"),
    seconds: float = typer.Option(0.0, "--seconds", "-s", help="stop after this many seconds (0 = until Ctrl-C)"),
    raw: bool = typer.Option(False, "--raw", help="print every line verbatim instead of the readings table"),
    csv_path: Optional[Path] = typer.Option(None, "--csv", help="append parsed readings to this CSV file"),
) -> None:
    """Show the station's live readings."""
    start = time.time()
    ser = _open_port(port, start + 30)
    writer: Optional[csv.DictWriter] = None
    csv_file = None
    if csv_path is not None:
        new_file = not csv_path.exists() or csv_path.stat().st_size == 0
        csv_file = csv_path.open("a", newline="")
        writer = csv.DictWriter(csv_file, fieldnames=["host_time"] + [name for name, _ in COLUMNS], extrasaction="ignore")
        if new_file:
            writer.writeheader()
    if not raw:
        typer.echo(_header())
    try:
        while seconds <= 0 or time.time() - start < seconds:
            try:
                line = ser.readline()
            except (serial.SerialException, OSError):
                try:
                    ser.close()
                except Exception:
                    pass
                typer.echo("[serial_dump] port lost, reconnecting", err=True)
                time.sleep(0.5)
                ser = _open_port(port, time.time() + 60)
                continue
            if not line:
                continue
            text = line.decode("utf-8", errors="replace").rstrip("\r\n")
            reading = parse_summary(text)
            if raw:
                typer.echo(f"{time.time() - start:8.1f}s {text}")
            elif reading is not None:
                typer.echo(_row(reading))
            elif text.strip():
                typer.echo(f"        {text}")
            if writer is not None and reading is not None:
                writer.writerow({"host_time": time.strftime("%Y-%m-%dT%H:%M:%S"), **reading})
                csv_file.flush()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        if csv_file is not None:
            csv_file.close()


if __name__ == "__main__":
    app()
