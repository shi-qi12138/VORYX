"""按预设配置、编译和测试项目；Windows 下自动初始化 MSVC 环境。"""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parents[2]


def windows_environment(installation=None, target_arch="x64", sdk_version=None):
    env = os.environ.copy()
    if installation is None:
        installer = Path(env.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
        vswhere = installer / "Microsoft Visual Studio/Installer/vswhere.exe"
        if not vswhere.is_file():
            raise RuntimeError("Install Visual Studio C++ build tools (vswhere missing)")
        result = subprocess.run([
            str(vswhere), "-latest", "-products", "*", "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property", "installationPath",
        ], check=True, capture_output=True, text=True)
        installation = result.stdout.strip()
        if not installation:
            raise RuntimeError("No Visual Studio installation with C++ tools found")
    developer = Path(installation) / "Common7/Tools/VsDevCmd.bat"
    if target_arch not in ("x86", "x64", "arm", "arm64"):
        raise RuntimeError(f"Unsupported Visual Studio target architecture: {target_arch}")
    if sdk_version and not all(part.isascii() and part.isdecimal()
                               for part in sdk_version.split(".")):
        raise RuntimeError("Invalid Windows SDK version")
    sdk = f" -winsdk={sdk_version}" if sdk_version else ""
    # cmd /s 要求额外的外层引号；使用原始命令行保留带空格的路径边界。
    command = (
        f'"{env.get("COMSPEC", "cmd.exe")}" /d /s /c '
        f'""{developer}" -no_logo -arch={target_arch} -host_arch=x64{sdk} >nul && set"'
    )
    result = subprocess.run(
        command,
        check=True, capture_output=True, text=True, errors="replace",
    )
    # 不输出完整环境变量，避免泄露与项目无关的凭据。
    for line in result.stdout.splitlines():
        key, separator, value = line.partition("=")
        if separator and key:
            env[key.upper()] = value
    llvm = Path(installation) / "VC/Tools/Llvm/x64/bin"
    if llvm.is_dir():
        env["PATH"] = str(llvm) + os.pathsep + env["PATH"]
    return env


def check_test_plan(preset, plan):
    tests = plan["tests"]
    names = {test["name"] for test in tests}
    labels = {
        label
        for test in tests
        for prop in test.get("properties", ())
        if prop["name"] == "LABELS"
        for label in prop["value"]
    }
    required = {
        "quality.format", "quality.braces", "quality.braces_enforcement",
        "integration.core_consumer", "integration.installed_sdk",
        "integration.null_buffers",
    }
    required_labels = {"cpu", "unit", "contract_null", "integration", "quality"}
    required_suites = ["unit.core.", "contract.null."]
    if "vulkan" in preset:
        required.add("integration.vulkan_buffers")
        required_labels.add("gpu_vulkan")
        required_suites.append("contract.vulkan.")
        if preset.startswith("windows-") or "-x11-" in preset or "-wayland-" in preset:
            required_suites.append("gpu.vulkan.")
        if preset.startswith("windows-"):
            required.add("integration.win32_clear")
        elif "-x11-" in preset:
            required.add("integration.xlib_clear")
        elif "-wayland-" in preset:
            required.add("integration.wayland_clear")
    if "opengl" in preset:
        required.update({"integration.opengl_buffers", "integration.opengl_clear", "integration.opengl_triangle"})
        required_labels.add("gpu_opengl")
        required_suites.append("contract.opengl.")
    missing_tests = sorted(required - names)
    missing_labels = sorted(required_labels - labels)
    missing_suites = [
        suite for suite in required_suites
        if not any(name.startswith(suite) for name in names)
    ]
    if missing_tests or missing_labels or missing_suites:
        raise RuntimeError(
            f"Missing required tests: {missing_tests}; labels: {missing_labels}; "
            f"suites: {missing_suites}"
        )
    return names


def check_test_report(report, planned):
    cases = ElementTree.parse(report).getroot().findall("testcase")
    failures = [
        case.get("name") for case in cases
        if case.get("status") != "run"
        or case.find("skipped") is not None
        or case.find("failure") is not None
        or case.find("error") is not None
    ]
    if failures:
        raise RuntimeError(f"Tests did not pass: {failures}")
    missing = planned - {case.get("name") for case in cases}
    if missing or len(cases) != len(planned):
        raise RuntimeError(f"Incomplete CTest report; missing tests: {sorted(missing)}")
    return len(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", required=True, choices=(
        "windows-debug", "windows-release", "linux-debug", "linux-release",
        "windows-opengl-debug", "linux-opengl-debug", "linux-opengl-software-debug",
        "windows-vulkan-debug", "windows-vulkan-release",
        "linux-vulkan-debug", "linux-vulkan-release", "linux-vulkan-software-debug",
        "linux-vulkan-software-release",
        "linux-vulkan-x11-debug", "linux-vulkan-x11-software-debug",
        "linux-vulkan-wayland-debug", "linux-vulkan-wayland-software-debug",
        "linux-vulkan-x11-release", "linux-vulkan-x11-software-release",
        "linux-vulkan-wayland-release", "linux-vulkan-wayland-software-release",
    ))
    args = parser.parse_args()
    if args.preset.startswith("windows-") != (sys.platform == "win32"):
        parser.error("Preset does not match the current operating system")
    env = windows_environment() if sys.platform == "win32" else os.environ.copy()
    for name in ("cmake", "ctest", "ninja"):
        if shutil.which(name, path=env.get("PATH", env.get("Path", ""))) is None:
            raise RuntimeError(f"Required executable is not on PATH: {name}")

    def run(command, **kwargs):
        print("Running: " + " ".join(command), flush=True)
        return subprocess.run(command, cwd=ROOT, env=env, check=True, **kwargs)

    run(["cmake", "--preset", args.preset])
    run(["cmake", "--build", "--preset", args.preset, "--parallel", "4"])

    reports = ROOT / "build" / args.preset / "reports"
    reports.mkdir(parents=True, exist_ok=True)
    plan_path = reports / "ctest-plan.json"
    report = reports / "ctest.xml"
    report.unlink(missing_ok=True)
    # 测试注册清单随报告归档，便于定位缺失套件。
    with plan_path.open("wb") as output:
        run(["ctest", "--preset", args.preset, "--show-only=json-v1"], stdout=output)
    planned = check_test_plan(args.preset, json.loads(plan_path.read_text(encoding="utf-8")))

    run(["ctest", "--preset", args.preset, "--output-junit", str(report)])
    passed = check_test_report(report, planned)
    print(json.dumps({"preset": args.preset, "result": "passed", "tests": passed}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, ElementTree.ParseError,
            subprocess.CalledProcessError) as error:
        print(f"Environment validation failed: {error}", file=sys.stderr)
        sys.exit(1)
