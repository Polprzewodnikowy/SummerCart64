import tempfile
from dataclasses import dataclass
from pathlib import Path
from subprocess import check_call


@dataclass
class IconSpec:
    size: int
    type: str


SPECS = [
    IconSpec(size=32, type="bmp"),
    IconSpec(size=16, type="bmp"),
    IconSpec(size=256, type="png"),
    IconSpec(size=128, type="png"),
    IconSpec(size=64, type="png"),
    IconSpec(size=48, type="png"),
    IconSpec(size=32, type="png"),
    IconSpec(size=16, type="png"),
]


def resize_transformer(path: Path, spec: IconSpec) -> None:
    check_call(
        [
            "convert",
            f"{path}[0]",
            "-filter",
            "lanczos",
            "-resize",
            f"{spec.size}x{spec.size}",
            f"PNG:{path}",
        ]
    )


def quantize_transformer(path: Path, spec: IconSpec) -> None:
    quantized_path = path.with_stem(f"{path.stem}-quantized")
    check_call(["pngquant", path, "--output", quantized_path])
    path.write_bytes(quantized_path.read_bytes())
    quantized_path.unlink()


def optimize_transformer(path: Path, spec: IconSpec) -> None:
    check_call(["zopflipng", "-y", path, path])


def convert_transformer(path: Path, spec: IconSpec) -> None:
    if spec.type != "png":
        check_call(["convert", path, f"{spec.type.upper()}:{path}"])


TRANSFORMERS = [
    resize_transformer,
    quantize_transformer,
    optimize_transformer,
    convert_transformer,
]


def generate_icon(source_path: Path, target_path: Path) -> None:
    aux_paths = []

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)
        for spec in SPECS:
            aux_path = tmp_path / f"{spec.size}-{spec.type}.tmp"
            aux_path.write_bytes(source_path.read_bytes())
            for transform in TRANSFORMERS:
                transform(aux_path, spec)

            aux_paths.append(aux_path)

        # NOTE: image order is important for certain software.
        check_call(["identify", *aux_paths])
        check_call(["convert", *aux_paths, target_path])
