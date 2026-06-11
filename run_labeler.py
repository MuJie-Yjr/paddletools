from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path


DEFAULT_UI_SCALE = "0.75"
WORKBENCH_TARGET = "ppocr_workbench"
BUILD_CONFIG = "Release"
DEFAULT_CONFIGURE_PRESET = "vs2026-release"
DEFAULT_BUILD_PRESET = "release"
DEFAULT_LOCAL_CONFIGURE_PRESET = "local-vs2026-release"
DEFAULT_LOCAL_BUILD_PRESET = "local-release"


def _bootstrap_paths() -> Path:
    return Path(__file__).resolve().parent


def _apply_default_ui_scale(env) -> None:
    if not env.get("QT_SCALE_FACTOR", "").strip():
        env["QT_SCALE_FACTOR"] = env.get("PPOCR_UI_SCALE", "").strip() or DEFAULT_UI_SCALE
    if not env.get("QT_SCALE_FACTOR_ROUNDING_POLICY", "").strip():
        env["QT_SCALE_FACTOR_ROUNDING_POLICY"] = "PassThrough"


def _expand_preset_value(value: str, base_dir: Path, parent_env: dict[str, str]) -> str:
    text = str(value).replace("${sourceDir}", str(base_dir))

    def replace_env(match: re.Match[str]) -> str:
        return parent_env.get(match.group(1), "")

    text = re.sub(r"\$penv\{([^}]+)\}", replace_env, text)
    text = re.sub(r"\$env\{([^}]+)\}", replace_env, text)
    return text


def _user_preset_names(base_dir: Path, section: str) -> set[str]:
    preset_path = base_dir / "CMakeUserPresets.json"
    if not preset_path.exists():
        return set()
    try:
        data = json.loads(preset_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return set()
    return {
        str(item.get("name", ""))
        for item in data.get(section, [])
        if isinstance(item, dict) and item.get("name")
    }


def _apply_user_preset_environment(base_dir: Path, env: dict[str, str]) -> None:
    preset_path = base_dir / "CMakeUserPresets.json"
    if not preset_path.exists():
        return
    try:
        data = json.loads(preset_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"Warning: ignored invalid CMakeUserPresets.json: {exc}", file=sys.stderr)
        return

    requested = env.get("PPOCR_LOCAL_PRESET", "").strip() or DEFAULT_LOCAL_CONFIGURE_PRESET
    presets = [item for item in data.get("configurePresets", []) if isinstance(item, dict)]
    selected = next((item for item in presets if item.get("name") == requested), None)
    if selected is None:
        selected = next((item for item in presets if str(item.get("name", "")).startswith("local-")), None)
    if selected is None:
        return

    parent_env = dict(env)
    for key, value in selected.get("environment", {}).items():
        if value is None:
            env.pop(key, None)
        else:
            env[str(key)] = _expand_preset_value(str(value), base_dir, parent_env)


def _workbench_environment(base_dir: Path) -> dict[str, str]:
    env = os.environ.copy()
    _apply_user_preset_environment(base_dir, env)
    _apply_default_ui_scale(env)
    return env


def _configure_preset_name(base_dir: Path, env: dict[str, str]) -> str:
    configured = env.get("PPOCR_CONFIGURE_PRESET", "").strip()
    if configured:
        return configured
    if DEFAULT_LOCAL_CONFIGURE_PRESET in _user_preset_names(base_dir, "configurePresets"):
        return DEFAULT_LOCAL_CONFIGURE_PRESET
    return DEFAULT_CONFIGURE_PRESET


def _build_preset_name(base_dir: Path, env: dict[str, str]) -> str:
    configured = env.get("PPOCR_BUILD_PRESET", "").strip()
    if configured:
        return configured
    if DEFAULT_LOCAL_BUILD_PRESET in _user_preset_names(base_dir, "buildPresets"):
        return DEFAULT_LOCAL_BUILD_PRESET
    return DEFAULT_BUILD_PRESET


def _candidate_workbench_exes(base_dir: Path) -> list[Path]:
    configured = os.environ.get("PPOCR_WORKBENCH_EXE", "").strip()
    if configured:
        return [Path(configured)]
    return [
        base_dir / "build_vs2026_gpu" / "Release" / "ppocr_workbench.exe",
        base_dir / "build_vs2026" / "Release" / "ppocr_workbench.exe",
        base_dir / "dist" / "ppocr_workbench" / "ppocr_workbench.exe",
    ]


def _candidate_build_dirs(base_dir: Path) -> list[Path]:
    configured = os.environ.get("PPOCR_WORKBENCH_EXE", "").strip()
    if configured:
        executable = Path(configured)
        for build_dir in (base_dir / "build_vs2026_gpu", base_dir / "build_vs2026"):
            try:
                executable.resolve().relative_to(build_dir.resolve())
                return [build_dir]
            except ValueError:
                continue
        return []
    return [
        base_dir / "build_vs2026_gpu",
        base_dir / "build_vs2026",
    ]


def _truthy_env(name: str) -> bool:
    return os.environ.get(name, "").strip().lower() in {"1", "true", "yes", "on"}


def _run_command(command: list[str], base_dir: Path, env: dict[str, str]) -> int:
    print("+ " + " ".join(command), flush=True)
    completed = subprocess.run(command, cwd=str(base_dir), env=env)
    return int(completed.returncode)


def _rebuild_cpp_workbench(base_dir: Path, env: dict[str, str]) -> int:
    if env.get("PPOCR_SKIP_REBUILD", "").strip().lower() in {"1", "true", "yes", "on"}:
        print("PPOCR_SKIP_REBUILD is set; skipping C++ rebuild.", flush=True)
        return 0

    for build_dir in _candidate_build_dirs(base_dir):
        if build_dir.exists():
            return _run_command([
                "cmake",
                "--build",
                str(build_dir),
                "--config",
                BUILD_CONFIG,
                "--target",
                WORKBENCH_TARGET,
            ], base_dir, env)

    configured = os.environ.get("PPOCR_WORKBENCH_EXE", "").strip()
    if configured:
        print(
            "PPOCR_WORKBENCH_EXE is set but it is not under a known CMake build directory; "
            "skipping automatic rebuild.",
            flush=True,
        )
        return 0

    configure_code = _run_command(["cmake", "--preset", _configure_preset_name(base_dir, env)], base_dir, env)
    if configure_code != 0:
        return configure_code
    return _run_command(["cmake", "--build", "--preset", _build_preset_name(base_dir, env), "--target", WORKBENCH_TARGET], base_dir, env)


def _runtime_path_entries(base_dir: Path, executable: Path, env: dict[str, str]) -> list[str]:
    candidates = [
        executable.parent,
        base_dir / "third_party" / "paddle_inference_gpu" / "paddle" / "lib",
        base_dir / "third_party" / "paddle_inference_gpu" / "third_party" / "install" / "mklml" / "lib",
        base_dir / "third_party" / "paddle_inference_gpu" / "third_party" / "install" / "onednn" / "lib",
        base_dir / "third_party" / "paddle_inference" / "paddle" / "lib",
        base_dir / "third_party" / "paddle_inference" / "third_party" / "install" / "mklml" / "lib",
        base_dir / "third_party" / "paddle_inference" / "third_party" / "install" / "onednn" / "lib",
        Path(env.get("CUDA_PATH", "")) / "bin",
        Path(env.get("TENSORRT_ROOT", "")) / "bin",
    ]
    entries: list[str] = []
    seen: set[str] = set()
    for candidate in candidates:
        if not candidate or not candidate.exists():
            continue
        text = str(candidate)
        key = text.lower()
        if key not in seen:
            entries.append(text)
            seen.add(key)
    return entries


def _launch_cpp_workbench(base_dir: Path, args: list[str], env: dict[str, str]) -> int | None:
    for executable in _candidate_workbench_exes(base_dir):
        if executable.exists():
            env = dict(env)
            runtime_paths = _runtime_path_entries(base_dir, executable, env)
            env["PATH"] = os.pathsep.join([*runtime_paths, env.get("PATH", "")])
            completed = subprocess.run([str(executable), *args], cwd=str(base_dir), env=env)
            return int(completed.returncode)
    return None


def _print_missing_cpp_workbench(base_dir: Path) -> None:
    candidates = "\n".join(f"  - {path}" for path in _candidate_workbench_exes(base_dir))
    message = f"""C++ PPOCR Workbench executable was not found.

Checked:
{candidates}

Build it with:
  cmake --preset vs2026-release
  cmake --build --preset release
"""
    print(message, file=sys.stderr)


def main() -> int:
    base_dir = _bootstrap_paths()
    args = sys.argv[1:]
    env = _workbench_environment(base_dir)

    rebuild_code = _rebuild_cpp_workbench(base_dir, env)
    if rebuild_code != 0:
        print(
            "\nRebuild failed. If ppocr_workbench.exe is already open, close it and run again.",
            file=sys.stderr,
        )
        return rebuild_code

    launched = _launch_cpp_workbench(base_dir, args, env)
    if launched is not None:
        return launched
    _print_missing_cpp_workbench(base_dir)
    return 127


if __name__ == "__main__":
    raise SystemExit(main())
