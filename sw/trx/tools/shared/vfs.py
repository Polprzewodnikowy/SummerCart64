import difflib
from pathlib import Path


class VirtualFilesystem:
    """Lazy disk operations to avoid disk thrashing."""

    def __init__(self) -> None:
        self.files = {}

    def get(self, path: Path) -> None:
        if content := self.files.get(path):
            return content
        content = path.read_text()
        self.files[path] = content
        return content

    def put(self, path: Path, content: str) -> None:
        self.files[path] = content

    def show_diff(self) -> bool:
        has_changes = False
        for path, new_content in self.files.items():
            if path.exists():
                old_content = path.read_text()
            else:
                old_content = None
            if old_content != new_content:
                print(
                    "".join(
                        difflib.unified_diff(
                            (
                                old_content.splitlines(keepends=True)
                                if old_content
                                else []
                            ),
                            new_content.splitlines(keepends=True),
                            fromfile=str(path),
                            tofile=str(path),
                        )
                    )
                )
                has_changes = True
        return has_changes

    def commit(self) -> None:
        for path, content in self.files.items():
            if not path.exists() or path.read_text() != content:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
