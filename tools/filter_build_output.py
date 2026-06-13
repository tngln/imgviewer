import locale
import sys
from pathlib import Path


NOISE_PREFIXES = (
    "Note: including file:",
    "\u6ce8\u610f: \u5305\u542b\u6587\u4ef6:",
)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: filter_build_output.py <log-file>", file=sys.stderr)
        return 1

    path = Path(sys.argv[1])
    encoding = locale.getpreferredencoding(False) or "utf-8"

    with path.open("r", encoding=encoding, errors="replace", newline="") as handle:
        for line in handle:
            if line.lstrip().startswith(NOISE_PREFIXES):
                continue
            sys.stdout.write(line)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
