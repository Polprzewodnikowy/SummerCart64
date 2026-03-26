import sys
import zipfile
from collections.abc import Iterable
from pathlib import Path


def create_zip(
    output_path: Path, source_files: Iterable[tuple[Path, str]]
) -> None:
    with zipfile.ZipFile(output_path, "w") as handle:
        for source_path, archive_name in source_files:
            if not source_path.exists():
                print(
                    f"WARNING: {source_path} does not exist", file=sys.stderr
                )
                continue
            handle.write(source_path, archive_name)
