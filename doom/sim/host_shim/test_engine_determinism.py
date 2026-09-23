#!/usr/bin/env python3
"""Run the SDL-free Doom host renderer twice and compare indexed frames."""

import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    executable = pathlib.Path(sys.argv[1])
    whx = pathlib.Path(sys.argv[2])
    with tempfile.TemporaryDirectory(prefix="fcpico-host-determinism-") as work:
        root = pathlib.Path(work)
        runs = []
        for name in ("one", "two"):
            output = root / name
            result = subprocess.run(
                [str(executable), "--whx", str(whx), "--demo", "1",
                 "--frames", "600", "--lockstep", "--dump-8bit", str(output)],
                text=True, capture_output=True, timeout=60, check=True,
            )
            if "host frames=600" not in result.stdout:
                raise AssertionError(result.stdout + result.stderr)
            runs.append([path.read_bytes() for path in sorted(output.glob("*.raw"))])
        if len(runs[0]) != 600 or runs[0] != runs[1]:
            raise AssertionError("600-frame indexed output differs between runs")
        if len(set(runs[0])) < 2:
            raise AssertionError("renderer did not produce changing frames")
    print("600/600 indexed frames identical across two runs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
