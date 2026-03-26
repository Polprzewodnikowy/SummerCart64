import re
from collections.abc import Iterable
from pathlib import Path
from subprocess import check_output


def find_files(
    root_dir: Path,
    extensions: list[str] | None = None,
    exception_patterns: list[re.Pattern[str]] | None = None,
) -> Iterable[Path]:
    stack: list[Path] = [root_dir]

    while stack:
        parent_dir = stack.pop()
        for path in parent_dir.iterdir():
            rel_path = path.relative_to(root_dir)
            if exception_patterns and any(
                pattern.match(str(rel_path)) for pattern in exception_patterns
            ):
                continue

            if path.is_dir():
                stack.append(path)
                continue

            if not path.is_file():
                continue

            if extensions and path.suffix not in extensions:
                continue

            yield path


def find_versioned_files(root_dir: Path | None = None) -> Iterable[Path]:
    for line in check_output(
        ["git", "ls-files"], cwd=root_dir, text=True
    ).splitlines():
        path = Path(line)
        if not path.is_dir():
            if root_dir:
                yield root_dir / path
            else:
                yield path


def is_binary_file(path: Path) -> bool:
    try:
        path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return True
    return False
