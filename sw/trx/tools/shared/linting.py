#!/usr/bin/env python3
import json
import re
import sys
from collections.abc import Callable, Iterable
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from shared.paths import PROJECT_PATHS, CommonPaths

# enable importing translation_utils from this directory
sys.path.insert(0, str(Path(__file__).resolve().parent))

import jsonschema
from json_utils import load_json5

SHARED_PROJECT = "libtrx"
CHILD_PROJECTS = ["tr1", "tr2"]
PROJECTS = [SHARED_PROJECT] + CHILD_PROJECTS


@dataclass
class LintContext:
    root_dir: Path
    versioned_files: list[Path]


@dataclass
class GameString:
    project: str
    path: Path
    key: str
    text: str


@dataclass
class LintWarning:
    path: Path
    message: str
    line: int | None = None

    def __str__(self) -> str:
        prefix = str(self.path)
        if self.line is not None:
            prefix += f":{self.line}"
        return f"{prefix}: {self.message}"


def lint_json_validity(
    context: LintContext, path: Path
) -> Iterable[LintWarning]:
    if path.suffix != ".json":
        return
    try:
        json.loads(path.read_text())
    except json.JSONDecodeError as ex:
        yield LintWarning(path, f"malformed JSON: {ex!s}")


def lint_newlines(context: LintContext, path: Path) -> Iterable[LintWarning]:
    text = path.read_text(encoding="utf-8")
    if not text:
        return
    if not text.endswith("\n"):
        yield LintWarning(path, "missing newline character at end of file")
    if text.endswith("\n\n"):
        yield LintWarning(path, "extra newline character at end of file")


def lint_trailing_whitespace(
    context: LintContext, path: Path
) -> Iterable[LintWarning]:
    if path.suffix == ".md":
        return
    for i, line in enumerate(path.open("r"), 1):
        if line.rstrip("\n").endswith(" "):
            yield LintWarning(path, "trailing whitespace", line=i)


def lint_const_primitives(
    context: LintContext, path: Path
) -> Iterable[LintWarning]:
    if path.suffix != ".h":
        return
    for i, line in enumerate(path.open("r"), 1):
        if re.search(r"const (int[a-z0-9_]*|bool)\b\s*[a-z]", line):
            yield LintWarning(path, "useless const", line=i)
        if re.search(r"\*\s*const(?!\s?\*)", line):
            yield LintWarning(path, "useless const", line=i)


def lint_meson_build_sort_order(
    context: LintContext, path: Path
) -> Iterable[LintWarning]:
    if path.name != "meson.build" or path.parent.name == "dwarfstack":
        return
    pattern = re.compile(r"\bcommon_sources\s*=\s*\[(.*?)\]", re.S)
    match = pattern.search(path.read_text())

    if not match:
        yield LintWarning(path, "unable to find sources array")
    else:
        block = match.group(1)
        lines = [l.strip().strip(",") for l in block.splitlines() if l.strip()]
        if not lines:
            yield LintWarning(path, "unable to parse sources array")
        else:
            clean = [l.strip("'") for l in lines]
            init = [l for l in clean if l == "init"]
            resources = [l for l in clean if l == "resources"]
            middle = [l for l in clean if l not in ("init", "resources")]
            is_sorted = clean == init + sorted(middle) + resources
            if not is_sorted:
                yield LintWarning(path, "source list is not ordered")


def lint_clang_format_markers(
    context: LintContext, path: Path
) -> Iterable[LintWarning]:
    if path.suffix != ".c":
        return
    clang_format_off_open = False
    for i, line in enumerate(path.open("r"), 1):
        if "// clang-format on" in line:
            clang_format_off_open = False
        if "// clang-format off" in line:
            if clang_format_off_open:
                yield LintWarning(
                    path,
                    "found `// clang-format off` before previous block was closed "
                    "with `// clang-format on`",
                    line=i,
                )
            clang_format_off_open = True


def get_relevant_project(context: LintContext, path: Path) -> str:
    for project, project_path in get_project_paths(context).items():
        if path.absolute().is_relative_to(project_path.absolute()):
            break
    else:
        raise RuntimeError(f"{path}: Unable to get project path")
    return project


def get_project_paths(context: LintContext) -> dict[str, Path]:
    return {project: context.src_dir for project in PROJECTS}


def lint_game_flow_schema(context: LintContext):
    schema_path = CommonPaths.docs_dir / "gameflow.schema.json"
    if not schema_path.exists():
        return
    schema = load_json5(schema_path)
    validator_cls = jsonschema.validators.validator_for(schema)
    validator = validator_cls(schema=schema)
    for project, paths in PROJECT_PATHS.items():
        game_flow_paths = paths.shipped_data_dir.rglob("**/gameflow.json5")
        for game_flow_path in game_flow_paths:
            data = load_json5(game_flow_path)
            for error in validator.iter_errors(instance=data):
                yield LintWarning(game_flow_path, error)


def _get_known_config_keys(config_map_paths: list[Path]) -> set[str]:
    known_keys: set[str] = set()
    # Match the first argument of X_CFG_* macros:
    # - bare key form: X_CFG_BOOL(audio.master_volume, ...)
    # - explicit string key form: X_CFG_ENUM_EX("ui.airbar_location", ...)
    pattern = re.compile(
        r"""^X_CFG_[A-Z0-9_]+(?:_EX)?\(\s*(?:"([^"]+)"|([a-zA-Z0-9_.]+))\s*,"""
    )
    for config_map_path in config_map_paths:
        if not config_map_path.exists():
            continue
        for line in config_map_path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            match = pattern.match(stripped)
            if not match:
                continue
            key = match.group(1) or match.group(2)
            if key:
                known_keys.add(key)
    return known_keys


def _get_preset_config_entries(preset_path: Path) -> dict[str, Any]:
    try:
        preset_data = load_json5(preset_path)
    except Exception as ex:
        raise ValueError(f"failed to parse JSON5: {ex!s}") from ex

    if not isinstance(preset_data, dict):
        raise ValueError("root must be an object")

    config_data = preset_data.get("config")
    if config_data is None:
        raise ValueError("missing 'config' object")
    if not isinstance(config_data, dict):
        raise ValueError("'config' must be an object")
    return config_data


def lint_preset_setting_keys(context: LintContext) -> Iterable[LintWarning]:
    config_dir = CommonPaths.src_dir / "config"
    config_map_paths = [
        config_dir / "map.def",
        config_dir / "map_tr1.def",
        config_dir / "map_tr2.def",
        config_dir / "map_tr3.def",
    ]
    known_keys = _get_known_config_keys(config_map_paths)
    if not known_keys:
        yield LintWarning(config_dir / "map.def", "unable to parse config keys")
        return

    presets_dir = CommonPaths.shipped_data_dir / "cfg/presets"
    if not presets_dir.exists():
        return

    for preset_path in sorted(presets_dir.glob("*.json5")):
        try:
            config_data = _get_preset_config_entries(preset_path)
        except ValueError as ex:
            yield LintWarning(preset_path, str(ex))
            continue

        for key in config_data:
            if key not in known_keys:
                yield LintWarning(
                    preset_path, f"unknown preset setting key: '{key}'"
                )


ALL_FILE_LINTERS: list[
    Callable[[LintContext, Path], Iterable[LintWarning]]
] = [
    lint_json_validity,
    lint_newlines,
    lint_trailing_whitespace,
    lint_const_primitives,
    lint_meson_build_sort_order,
    lint_clang_format_markers,
]

ALL_BULK_LINTERS: list[
    Callable[[LintContext, list[Path]], Iterable[LintWarning]]
] = [
]

ALL_REPO_LINTERS: list[
    Callable[[LintContext, list[Path]], Iterable[LintWarning]]
] = [
    lint_game_flow_schema,
    lint_preset_setting_keys,
]


def lint_file(context: LintContext, file: Path) -> Iterable[LintWarning]:
    for linter_func in ALL_FILE_LINTERS:
        yield from linter_func(context, file)


def lint_bulk_files(
    context: LintContext, files: list[Path]
) -> Iterable[LintWarning]:
    for linter_func in ALL_BULK_LINTERS:
        yield from linter_func(context, files)


def lint_repo(context: LintContext) -> Iterable[LintWarning]:
    for linter_func in ALL_REPO_LINTERS:
        yield from linter_func(context)
