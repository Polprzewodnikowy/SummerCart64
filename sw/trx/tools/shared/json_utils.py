"""Utilities for working with JSON/JSON5 files."""

import json
from pathlib import Path
from typing import Any

try:
    import pyjson5
except ImportError:
    pyjson5 = None


def load_json5(path: Path):
    """Load and parse a JSON5 file as Python data structures. Requires `pyjson5`."""
    return load_json5_from_string(path.read_text(encoding="utf-8"))


def load_json5_from_string(content: str):
    if pyjson5 is None:
        raise RuntimeError("pyjson5 is required to parse JSON5 files")
    return pyjson5.loads(content)


def write_json_to_string(data) -> str:
    return json.dumps(data, ensure_ascii=False, indent=4)


def write_json(path: Path, data) -> None:
    """Serialize `data` as pretty-printed JSON to `path`, with UTF-8 encoding."""
    new_content = write_json_to_string(data) + "\n"
    if path.exists ( ) and new_content != path.read_text():
        path.write_text(new_content, encoding="utf-8")


class JSONPointers:
    """
    Tiny helper around RFC-6901 JSON Pointers.

        jp = JSONPointers(data)
        leaves = list(jp)      # every reachable leaf pointer
        value  = jp["/foo/0"]  # read
        jp["/foo/0"] = "BAR"    # write (in-place)

    Works with any mix of dict / list containers.
    """

    def __init__(self, data: Any) -> None:
        self._root = data
        self._pointers: list[str] | None = None  # lazy-built

    def __iter__(self) -> list[str]:
        """Return all JSON-Pointer strings that lead to a *leaf* value."""
        if self._pointers is None:
            self._pointers = []
            self._walk(self._root, [])
        yield from self._pointers.copy()

    def get(self, ptr: str, default: Any | None = None) -> Any:
        try:
            return self[ptr]
        except (KeyError, IndexError):
            return default

    def __getitem__(self, ptr: str) -> Any:
        """Return the value at *ptr* (raises KeyError / IndexError if missing)."""
        node = self._root
        for token in self._split(ptr):
            node = node[self._index_or_key(node, token)]
        return node

    def __setitem__(self, ptr: str, value: Any) -> None:
        """Replace the value at *ptr* with *value* (in-place), auto-creating
        missing dict or list containers as needed."""
        tokens = self._split(ptr)
        if not tokens:
            raise ValueError(
                "Cannot replace the root object through set_path()"
            )

        node = self._root
        for idx, token in enumerate(tokens[:-1]):
            next_index = tokens[idx + 1].lstrip("-").isdigit()
            key = self._index_or_key(node, token)
            node = self._ensure_container(node, key, next_index)

        last_key = self._index_or_key(node, tokens[-1])
        if isinstance(node, list):
            while last_key >= len(node):
                node.append(None)
        node[last_key] = value
        # invalidate cached pointers: tree shape may have changed
        self._pointers = None

    def __delitem__(self, ptr: str) -> None:
        """Remove the value at *ptr* (in-place), deleting the key or list element."""
        tokens = self._split(ptr)
        if not tokens:
            raise ValueError("Cannot delete the root object")

        node = self._root
        for token in tokens[:-1]:
            node = node[self._index_or_key(node, token)]

        last = tokens[-1]
        key = self._index_or_key(node, last)
        if isinstance(node, dict):
            node.pop(key, None)
        elif isinstance(node, list):
            del node[key]
        else:
            raise TypeError(
                f"Cannot delete at non-container at token '{last}'"
            )
        self._pointers = None

    def _ensure_container(
        self, node: Any, key: str | int, is_index: bool
    ) -> Any:
        """Ensure node[key] exists as dict or list based on is_index, and return it."""
        if isinstance(node, dict):
            if key not in node or not isinstance(node[key], (dict, list)):
                node[key] = [] if is_index else {}
            return node[key]
        if isinstance(node, list):
            while key >= len(node):
                node.append([] if is_index else {})
            if not isinstance(node[key], (dict, list)):
                node[key] = [] if is_index else {}
            return node[key]
        raise TypeError(f"Cannot traverse into non-container at token '{key}'")

    @property
    def data(self) -> Any:
        return self._root

    def _walk(self, node: Any, path: list[str | int]) -> None:
        """DFS that records every *leaf* pointer."""
        if isinstance(node, dict):
            for k, v in node.items():
                self._walk(v, path + [k])
        elif isinstance(node, list):
            for i, v in enumerate(node):
                self._walk(v, path + [i])
        else:
            self._pointers.append(self._encode(path))  # type: ignore[arg-type]

    @staticmethod
    def _split(ptr: str) -> list[str]:
        if ptr == "":
            return []
        if not ptr.startswith("/"):
            raise ValueError(f"Invalid JSON Pointer: {ptr!r}")
        return [
            token.replace("~1", "/").replace("~0", "~")
            for token in ptr.lstrip("/").split("/")
        ]

    @staticmethod
    def _encode(path: list[str | int]) -> str:
        esc = lambda s: str(s).replace("~", "~0").replace("/", "~1")
        return "/" + "/".join(esc(p) for p in path)

    @staticmethod
    def _index_or_key(node: Any, token: str) -> str | int:
        # if we're in a list and token looks like an int → use int index
        if isinstance(node, list) and token.lstrip("-").isdigit():
            return int(token)
        return token
