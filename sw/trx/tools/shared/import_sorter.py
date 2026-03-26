import functools
import re
from collections.abc import Iterable
from pathlib import Path
from shutil import which
from subprocess import run

from shared.files import find_versioned_files


def custom_sort(source: Iterable[str], forced_order: list[str]) -> list[str]:
    def key_func(item: str) -> tuple[str, int]:
        if item in forced_order:
            return (forced_order[0], forced_order.index(item))
        return (item, 0)

    return sorted(source, key=key_func)


def sort_imports_single_file(
    path: Path,
    root_dir: Path,
    own_include_map: dict[str, str],
    fix_map: dict[str, str],
    forced_order: list[str],
) -> None:
    source = path.read_text()
    try:
        rel_path = path.relative_to(root_dir)
    except ValueError:
        return

    own_include = str(rel_path.with_suffix(".h"))
    own_include = own_include_map.get(str(rel_path), own_include)

    for key, value in fix_map.items():
        source = re.sub(
            r'(#include ["<])' + re.escape(key) + '([">])',
            r"\1" + value + r"\2",
            source,
        )

    def cb(match):
        includes = re.findall(r'#include (["<][^"<>]+[">])', match.group(0))
        groups = {
            "self": set(),
            "local": set(),
            "shared": set(),
            "external": set(),
        }
        for include in includes:
            if include.strip('"<>') == own_include:
                groups["self"].add(include)
            elif include.startswith("<trx"):
                groups["shared"].add(include)
            elif include.startswith("<"):
                groups["external"].add(include)
            elif include.startswith('"'):
                groups["local"].add(include)

        groups = {key: value for key, value in groups.items() if value}

        ret = "\n\n".join(
            "\n".join(
                f"#include {include}"
                for include in custom_sort(group, forced_order)
            )
            for group in groups.values()
        ).strip()
        return ret

    source = re.sub(
        "^#include [^\n]+(\n*#include [^\n]+)*",
        cb,
        source,
        flags=re.M,
    )
    if source != path.read_text():
        path.write_text(source)


def sort_imports(
    root_dir: Path,
    system_include_dirs: list[Path],
    paths: list[Path],
    own_include_map: dict[str, str],
    fix_map: dict[str, str],
    forced_order: list[str],
) -> None:
    for path in paths:
        sort_imports_single_file(
            path,
            root_dir=root_dir,
            own_include_map=own_include_map,
            fix_map=fix_map,
            forced_order=forced_order,
        )
