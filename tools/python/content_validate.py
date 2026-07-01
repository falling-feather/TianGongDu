from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CONTENT_DIR = ROOT / "content"
REQUIRED_FIELDS = {"schemaVersion", "id", "displayNameKey", "tags", "designerNote"}


def validate_file(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"{path}: invalid JSON at line {exc.lineno}: {exc.msg}"]

    missing = sorted(REQUIRED_FIELDS - set(data))
    if missing:
        errors.append(f"{path}: missing required fields: {', '.join(missing)}")

    if "tags" in data and not isinstance(data["tags"], list):
        errors.append(f"{path}: tags must be a list")

    if "schemaVersion" in data and not isinstance(data["schemaVersion"], int):
        errors.append(f"{path}: schemaVersion must be an integer")

    return errors


def main() -> int:
    files = sorted(CONTENT_DIR.rglob("*.json"))
    if not files:
        print("No content JSON files found.")
        return 1

    errors: list[str] = []
    for path in files:
        errors.extend(validate_file(path))

    if errors:
        print("Content validation failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    print(f"Content validation passed for {len(files)} files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

