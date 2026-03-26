from collections.abc import Iterable


def uniq[T](source: list[T]) -> list[T]:
    return list(dict.fromkeys(source))


def chunks[T](generator: Iterable[T], n: int) -> Iterable[list[T]]:
    """Yield successive chunks from a generator."""
    chunk = []
    for item in generator:
        chunk.append(item)
        if len(chunk) == n:
            yield chunk
            chunk = []
    if chunk:
        yield chunk
