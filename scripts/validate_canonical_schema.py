from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


def _is_integer(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _is_number(value: Any) -> bool:
    return (isinstance(value, int) or isinstance(value, float)) and not isinstance(
        value, bool
    )


def _format_path(path: str, key: str) -> str:
    if not path:
        return key
    if key.startswith("["):
        return f"{path}{key}"
    return f"{path}.{key}"


def _validate_type(expected_type: str, value: Any, path: str) -> str | None:
    if expected_type == "object" and not isinstance(value, dict):
        return f"{path} must be an object"
    if expected_type == "array" and not isinstance(value, list):
        return f"{path} must be an array"
    if expected_type == "string" and not isinstance(value, str):
        return f"{path} must be a string"
    if expected_type == "integer" and not _is_integer(value):
        return f"{path} must be an integer"
    if expected_type == "number" and not _is_number(value):
        return f"{path} must be a number"
    if expected_type == "boolean" and not isinstance(value, bool):
        return f"{path} must be a boolean"
    return None


def validate_instance(
    schema: dict[str, Any], instance: Any, path: str = "$"
) -> list[str]:
    errors: list[str] = []

    expected_type = schema.get("type")
    if isinstance(expected_type, str):
        type_error = _validate_type(expected_type, instance, path)
        if type_error is not None:
            return [type_error]

    if "const" in schema and instance != schema["const"]:
        errors.append(f"{path} must equal {schema['const']!r}")

    enum_values = schema.get("enum")
    if isinstance(enum_values, list) and instance not in enum_values:
        errors.append(f"{path} must be one of {enum_values}")

    if isinstance(instance, dict):
        required = schema.get("required", [])
        if isinstance(required, list):
            for key in required:
                if key not in instance:
                    errors.append(f"{path} missing required property '{key}'")

        if schema.get("additionalProperties") is False:
            allowed = schema.get("properties", {})
            if isinstance(allowed, dict):
                for key in instance:
                    if key not in allowed:
                        errors.append(f"{path} has unexpected property '{key}'")

        properties = schema.get("properties")
        if isinstance(properties, dict):
            for key, property_schema in properties.items():
                if key not in instance:
                    continue
                if not isinstance(property_schema, dict):
                    continue
                child_path = _format_path(path, key)
                errors.extend(
                    validate_instance(
                        schema=property_schema,
                        instance=instance[key],
                        path=child_path,
                    )
                )

    if isinstance(instance, list):
        min_items = schema.get("minItems")
        if _is_integer(min_items) and len(instance) < min_items:
            errors.append(f"{path} must contain at least {min_items} item(s)")

        max_items = schema.get("maxItems")
        if _is_integer(max_items) and len(instance) > max_items:
            errors.append(f"{path} must contain at most {max_items} item(s)")

        if schema.get("uniqueItems"):
            seen: set[str] = set()
            duplicates = False
            for item in instance:
                marker = json.dumps(item, sort_keys=True)
                if marker in seen:
                    duplicates = True
                    break
                seen.add(marker)
            if duplicates:
                errors.append(f"{path} must contain unique items")

        item_schema = schema.get("items")
        if isinstance(item_schema, dict):
            for index, item in enumerate(instance):
                child_path = _format_path(path, f"[{index}]")
                errors.extend(
                    validate_instance(
                        schema=item_schema,
                        instance=item,
                        path=child_path,
                    )
                )

    if isinstance(instance, str):
        min_length = schema.get("minLength")
        if _is_integer(min_length) and len(instance) < min_length:
            errors.append(f"{path} must have length >= {min_length}")

        max_length = schema.get("maxLength")
        if _is_integer(max_length) and len(instance) > max_length:
            errors.append(f"{path} must have length <= {max_length}")

        pattern = schema.get("pattern")
        if isinstance(pattern, str):
            if re.fullmatch(pattern, instance) is None:
                errors.append(f"{path} must match pattern {pattern!r}")

    if _is_integer(instance) or _is_number(instance):
        minimum = schema.get("minimum")
        if _is_number(minimum) and instance < minimum:
            errors.append(f"{path} must be >= {minimum}")

        maximum = schema.get("maximum")
        if _is_number(maximum) and instance > maximum:
            errors.append(f"{path} must be <= {maximum}")

    return errors


def _load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate JSON against canonical schema"
    )
    parser.add_argument("--schema", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args()

    schema = _load_json(args.schema)
    instance = _load_json(args.input)
    if not isinstance(schema, dict):
        print("[FAIL] schema root must be a JSON object")
        return 1

    errors = validate_instance(schema=schema, instance=instance)
    if errors:
        print(f"[FAIL] {args.input} is invalid against {args.schema}")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(f"[OK] {args.input} matches canonical schema {args.schema}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
