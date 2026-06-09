from __future__ import annotations

from pathlib import Path


def test_legacy_python_ui_files_are_removed() -> None:
    root = Path(__file__).resolve().parents[1]
    removed_paths = [
        root / "ppocr_labeler",
        root / "tests" / "legacy_python",
        root / "ppocr_labeler.spec",
        root / "requirements-legacy-python.txt",
        root / "scripts" / "verify_env.py",
    ]
    assert not [path for path in removed_paths if path.exists()]
