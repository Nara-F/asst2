#!/usr/bin/env python3
"""Run an unchanged Linux x86-64 reference with a verified TSC conversion."""

import hashlib
import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile


SYMBOLS = {
    "_ZZN10CycleTimer14secondsPerTickEvE18secondsPerTick_val": ("SCALE", 8),
    "_ZZN10CycleTimer14secondsPerTickEvE11initialized": ("INITIALIZED", 1),
    "_ZN10CycleTimer14secondsPerTickEv": ("FUNCTION", None),
}


def reference_environment(binary):
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise RuntimeError("the timer repair supports Linux x86-64 only")
    with binary.open("rb") as stream:
        header = stream.read(64)
    if (header[:6] != b"\x7fELF\x02\x01" or len(header) != 64 or
            struct.unpack_from("<HH", header, 16) != (3, 62)):
        raise RuntimeError("expected the supplied x86-64 PIE reference executable")

    output = subprocess.check_output(["nm", "-S", str(binary)], text=True)
    env = os.environ.copy()
    found = set()
    for line in output.splitlines():
        fields = line.split()
        if len(fields) != 4 or fields[3] not in SYMBOLS:
            continue
        name, expected_size = SYMBOLS[fields[3]]
        if expected_size is not None and int(fields[1], 16) != expected_size:
            raise RuntimeError("unexpected timer symbol size")
        env["CS149_REF_TIMER_" + name] = fields[0]
        found.add(fields[3])
    if found != set(SYMBOLS):
        raise RuntimeError("old timer symbols are missing; refusing to guess binary offsets")

    source = Path(__file__).resolve().with_name("ref_timer.c")
    digest = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
    cache = source.parent / ".ref_timer"
    cache.mkdir(exist_ok=True)
    library = cache / ("timer-" + digest + ".so")
    if not library.exists():
        # An atomic rename also allows simultaneous first-time invocations.
        with tempfile.TemporaryDirectory(dir=cache) as build:
            temporary = Path(build) / "timer.so"
            subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror",
                            "-fPIC", "-shared", str(source), "-o", str(temporary),
                            "-ldl", "-lm"], check=True)
            temporary.replace(library)
    env["LD_PRELOAD"] = str(library) + (
        " " + env["LD_PRELOAD"] if env.get("LD_PRELOAD") else "")
    # A parent check invocation must not accidentally stop a normal run.
    env.pop("CS149_REF_TIMER_CHECK", None)
    return env


def main():
    args = sys.argv[1:]
    check = bool(args and args[0] == "--check")
    if check:
        args.pop(0)
    if not args:
        print("Usage: run_reference.py [--check] REFERENCE_BINARY [REFERENCE_ARGS...]",
              file=sys.stderr)
        return 2
    binary = Path(args[0]).resolve(strict=True)
    env = reference_environment(binary)
    if check:
        env["CS149_REF_TIMER_CHECK"] = "1"
    os.execve(binary, [str(binary), *args[1:]], env)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print("ref timer: " + str(error), file=sys.stderr)
        sys.exit(125)
