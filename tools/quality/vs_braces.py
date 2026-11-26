"""在 Visual Studio 构建目录内为 clang-tidy 生成 Ninja 编译数据库。"""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys

from tools.environment.build import ROOT, windows_environment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--instance", type=Path, required=True)
    parser.add_argument("--platform", choices=("Win32", "x64", "ARM", "ARM64"), required=True)
    parser.add_argument("--sdk")
    parser.add_argument("--config", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--define", action="append", default=[])
    args = parser.parse_args()

    arch = "x86" if args.platform == "Win32" else args.platform.lower()
    env = windows_environment(args.instance, arch, args.sdk)
    bundled = args.instance / "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
    ninja = str(bundled) if bundled.is_file() else shutil.which("ninja", path=env["PATH"])
    if ninja is None:
        raise RuntimeError("Install Ninja or the Visual Studio CMake tools component for voryx_braces_check.")

    database_dir = args.build_dir / "quality-ninja" / args.config
    print(f"Generating clang-tidy compilation database in {database_dir}", flush=True)
    # 辅助配置只负责生成编译命令。实际检查仍使用根 .clang-tidy。
    subprocess.run([
        args.cmake, "-S", str(ROOT), "-B", str(database_dir), "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}", f"-DCMAKE_CXX_COMPILER={args.compiler}",
        f"-DCMAKE_BUILD_TYPE={args.config}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DVORYX_ENABLE_QUALITY_CHECKS=OFF", "-DVORYX_INSTALL=OFF",
        *(f"-D{definition}" for definition in args.define),
    ], cwd=ROOT, env=env, check=True)
    subprocess.run([
        sys.executable, str(ROOT / "tools/quality/check.py"), "braces",
        "--tool", args.tool, "--build-dir", str(database_dir),
    ], cwd=ROOT, env=env, check=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Visual Studio braces check failed: {error}", file=sys.stderr)
        sys.exit(1)
