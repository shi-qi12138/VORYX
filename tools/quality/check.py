"""检查自有 C++ 的排版和完整括号，不修改源文件。"""

import argparse
import difflib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOTS = ("include", "src", "tests", "samples", "tools")


def run(arguments, **kwargs):
    return subprocess.run(
        [str(item) for item in arguments], cwd=ROOT, capture_output=True,
        text=True, encoding="utf-8", errors="replace", timeout=90, **kwargs
    )


def source_files():
    for folder in SOURCE_ROOTS:
        for path in sorted((ROOT / folder).rglob("*")):
            if path.suffix in (".hpp", ".cpp", ".h", ".cc"):
                yield path


def check_format(tool):
    checked = 0
    failed = False
    for path in source_files():
        checked += 1
        original = path.read_text(encoding="utf-8")
        result = run([
            tool, "--style=file", "--fallback-style=none",
            f"--assume-filename={path}",
        ], input=original)
        if result.returncode:
            print(result.stderr, file=sys.stderr)
            return result.returncode
        if result.stdout != original:
            failed = True
            print("".join(difflib.unified_diff(
                original.splitlines(True), result.stdout.splitlines(True),
                fromfile=str(path), tofile="formatted",
            )))
    if not checked:
        raise RuntimeError("No first-party C++ files found")
    print(f"Format: {checked} file(s), {'FAIL' if failed else 'PASS'}")
    return int(failed)


def check_braces(tool, build_dir):
    database = build_dir / "compile_commands.json"
    if not database.is_file():
        raise RuntimeError(f"Missing {database}. Configure with a Ninja preset "
                           "or build the CMake voryx_braces_check target in Visual Studio.")
    entries = json.loads(database.read_text(encoding="utf-8"))
    sources = set()
    for entry in entries:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        path = path.resolve()
        if any(path.is_relative_to(ROOT / folder) for folder in SOURCE_ROOTS):
            sources.add(path)
    if not sources:
        raise RuntimeError("No first-party translation units in compilation database")
    # 锚定实际仓库路径，避免把第三方同名 src 目录纳入检查。
    prefix = re.escape(ROOT.as_posix()).replace("/", r"[\\/]")
    header_filter = f"^{prefix}[\\\\/](include|src|tests|samples|tools)[\\\\/]"
    for path in sorted(sources):
        result = run([
            tool, f"--config-file={ROOT / '.clang-tidy'}",
            f"--header-filter={header_filter}", "-p", build_dir, path,
        ])
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr, file=sys.stderr)
            return result.returncode
    print(f"Braces: {len(sources)} translation unit(s), PASS")
    return 0


def check_enforcement(tool, build_dir):
    # 故意省略括号的负例只放在构建目录，不能混入正式源码枚举。
    probe_root = build_dir / "quality-probes"
    probe_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=probe_root) as directory:
        assert Path(directory).resolve().is_relative_to(build_dir.resolve())
        for complete in (True, False):
            path = Path(directory) / "braces_probe.cpp"
            body = "{ return 1; }" if complete else "return 1;"
            path.write_text(
                f"int Probe(bool ready) {{ if (ready) {body} return 0; }}\n",
                encoding="utf-8",
            )
            result = run([
                tool, f"--config-file={ROOT / '.clang-tidy'}", path,
                "--", "-std=c++20",
            ])
            diagnosed = "readability-braces-around-statements" in (
                result.stdout + result.stderr
            )
            expected = result.returncode == 0 if complete else (
                result.returncode != 0 and diagnosed
            )
            if not expected:
                print(result.stdout + result.stderr)
                raise RuntimeError("Braces positive/negative enforcement probe failed")
    print("Braces enforcement: complete accepted, missing braces rejected")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("format", "braces", "selftest"))
    parser.add_argument("--tool", required=True)
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    if args.mode == "format":
        return check_format(args.tool)
    if args.build_dir is None:
        parser.error("--build-dir is required for braces/selftest")
    if args.mode == "braces":
        return check_braces(args.tool, args.build_dir.resolve())
    return check_enforcement(args.tool, args.build_dir.resolve())


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"Quality check failed: {error}", file=sys.stderr)
        sys.exit(1)
