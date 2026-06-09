from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


DEFAULT_UI_SCALE = "0.75"
WORKBENCH_TARGET = "ppocr_workbench"
BUILD_CONFIG = "Release"


def _bootstrap_paths() -> Path:
    return Path(__file__).resolve().parent


def _apply_default_ui_scale(env) -> None:
    if not env.get("QT_SCALE_FACTOR", "").strip():
        env["QT_SCALE_FACTOR"] = env.get("PPOCR_UI_SCALE", "").strip() or DEFAULT_UI_SCALE
    if not env.get("QT_SCALE_FACTOR_ROUNDING_POLICY", "").strip():
        env["QT_SCALE_FACTOR_ROUNDING_POLICY"] = "PassThrough"


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


def _run_command(command: list[str], base_dir: Path) -> int:
    print("+ " + " ".join(command), flush=True)
    completed = subprocess.run(command, cwd=str(base_dir))
    return int(completed.returncode)


def _rebuild_cpp_workbench(base_dir: Path) -> int:
    if _truthy_env("PPOCR_SKIP_REBUILD"):
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
            ], base_dir)

    configured = os.environ.get("PPOCR_WORKBENCH_EXE", "").strip()
    if configured:
        print(
            "PPOCR_WORKBENCH_EXE is set but it is not under a known CMake build directory; "
            "skipping automatic rebuild.",
            flush=True,
        )
        return 0

    configure_code = _run_command(["cmake", "--preset", "vs2026-release"], base_dir)
    if configure_code != 0:
        return configure_code
    return _run_command(["cmake", "--build", "--preset", "release", "--target", WORKBENCH_TARGET], base_dir)


def _runtime_path_entries(base_dir: Path, executable: Path) -> list[str]:
    candidates = [
        executable.parent,
        base_dir / "third_party" / "paddle_inference_gpu" / "paddle" / "lib",
        base_dir / "third_party" / "paddle_inference_gpu" / "third_party" / "install" / "mklml" / "lib",
        base_dir / "third_party" / "paddle_inference_gpu" / "third_party" / "install" / "onednn" / "lib",
        base_dir / "third_party" / "paddle_inference" / "paddle" / "lib",
        base_dir / "third_party" / "paddle_inference" / "third_party" / "install" / "mklml" / "lib",
        base_dir / "third_party" / "paddle_inference" / "third_party" / "install" / "onednn" / "lib",
        Path(os.environ.get("CUDA_PATH", "")) / "bin",
        Path("D:/IDE/TensorRT/bin"),
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


def _launch_cpp_workbench(base_dir: Path, args: list[str]) -> int | None:
    for executable in _candidate_workbench_exes(base_dir):
        if executable.exists():
            env = os.environ.copy()
            _apply_default_ui_scale(env)
            runtime_paths = _runtime_path_entries(base_dir, executable)
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

    rebuild_code = _rebuild_cpp_workbench(base_dir)
    if rebuild_code != 0:
        print(
            "\nRebuild failed. If ppocr_workbench.exe is already open, close it and run again.",
            file=sys.stderr,
        )
        return rebuild_code

    launched = _launch_cpp_workbench(base_dir, args)
    if launched is not None:
        return launched
    _print_missing_cpp_workbench(base_dir)
    return 127


if __name__ == "__main__":
    raise SystemExit(main())
