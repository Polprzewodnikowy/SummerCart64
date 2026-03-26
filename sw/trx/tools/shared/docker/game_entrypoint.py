import argparse
import os
import shutil
from dataclasses import dataclass, field, fields
from pathlib import Path
from subprocess import check_call, run
from typing import Any, Self

from shared.packaging import create_zip
from shared.paths import TOOLS_DIR
from shared.versioning import generate_package_name, generate_version


@dataclass
class BaseOptions:
    platform: str

    @property
    def build_root(self) -> Path:
        return Path(f"/app/build/trx/{self.platform}/")

    @property
    def version(self) -> str:
        return generate_version()

    @classmethod
    def from_args(cls, args: argparse.Namespace) -> Self:
        cls_fields = [field.name for field in fields(cls)]
        filtered_args = vars(args)
        filtered_args = {
            k: v for k, v in filtered_args.items() if k in cls_fields
        }
        return cls(**filtered_args)


@dataclass
class PackageOptions(BaseOptions):
    tr_version: int | None
    platform: str

    @property
    def default_stem(self) -> Path:
        return generate_package_name(
            engine_version=self.version,
            platform=self.platform,
            game_version=self.tr_version,
        )

    @property
    def ship_dirs(self) -> list[Path]:
        if self.platform == "win-installer":
            return []
        elif self.tr_version is None:
            return [Path(f"/app/data/trx/ship/")]
        else:
            return [
                Path(f"/app/data/common/ship/"),
                Path(f"/app/data/tr{self.tr_version}/ship/"),
            ]

    @property
    def release_zip_files(self) -> list[tuple[Path, str]]:
        if self.platform == "linux":
            return [(self.build_root / f"TRX", f"TRX")]

        elif self.platform == "win":
            return [(self.build_root / f"TRX.exe", f"TRX.exe")]
        elif self.platform == "win-installer":
            assert self.tr_version is not None
            return [
                (
                    TOOLS_DIR
                    / f"installer/out/TR{self.tr_version}X_Installer.exe",
                    generate_package_name(
                        engine_version=self.version,
                        platform=self.platform,
                        game_version=self.tr_version,
                    )
                    + ".exe",
                )
            ]

        return []


@dataclass
class BuildOptions(BaseOptions):
    target: str
    meson_args: list[str] = field(default_factory=list)

    strip_tool = "strip"
    upx_tool = "upx"

    @property
    def build_args(self) -> list[str]:
        if self.platform == "win":
            return [
                "--cross",
                "/app/tools/shared/docker/game-win/meson_linux_mingw32.txt",
            ]
        return []

    @property
    def compressable_exes(self) -> list[Path]:
        if self.platform == "linux":
            return [self.build_root / f"TRX"]
        elif self.platform == "win":
            return [self.build_root / f"TRX.exe"]
        return []

    @property
    def build_target(self) -> Path:
        return Path(f"src/")


def compress_exe(options: BuildOptions, path: Path) -> None:
    if run([options.upx_tool, "-t", str(path)]).returncode != 0:
        check_call([options.strip_tool, str(path)])
        check_call([options.upx_tool, str(path)])


class BaseCommand:
    name: str = NotImplemented
    help: str = NotImplemented

    def decorate_parser(self, parser: argparse.ArgumentParser) -> None:
        pass

    def run(self, args: argparse.Namespace) -> None:
        raise NotImplementedError("not implemented")


class BuildCommand(BaseCommand):
    name = "build"

    def decorate_parser(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--platform")
        parser.add_argument(
            "--target",
            choices=["debug", "release", "debugoptim"],
            required=True,
        )
        parser.add_argument(
            "--meson-arg",
            dest="meson_args",
            action="append",
            default=[],
        )

    def run(self, args: argparse.Namespace) -> None:
        options = BuildOptions.from_args(args)
        pkg_config_path = os.environ.get("PKG_CONFIG_PATH")

        if not (options.build_root / "build.ninja").exists():
            command: list[str | Path] = [
                "meson",
                "setup",
                "--buildtype",
                options.target,
                *options.build_args,
                *options.meson_args,
                options.build_root,
                options.build_target,
            ]
            if pkg_config_path:
                command.extend(["--pkg-config-path", pkg_config_path])
            check_call(command)
        check_call(["meson", "compile", f"TRX"], cwd=options.build_root)

        if options.target == "release":
            for exe_path in options.compressable_exes:
                compress_exe(options, exe_path)


class PackageCommand(BaseCommand):
    name = "package"

    def decorate_parser(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--platform")
        parser.add_argument("--tr-version", type=int, required=True)
        parser.add_argument("-o", "--output", type=Path)
        parser.add_argument("--no-zip", action="store_true")

    def run(self, args: argparse.Namespace) -> None:
        options = PackageOptions.from_args(args)
        run_package(options=options, output=args.output, no_zip=args.no_zip)


class PackageAllCommand(BaseCommand):
    name = "package-all"

    def decorate_parser(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--platform")
        parser.add_argument("-o", "--output-root", type=Path)
        parser.add_argument("--no-zip", action="store_true")

    def run(self, args: argparse.Namespace) -> None:
        for tr_version in [1, 2, 3, None]:
            options = PackageOptions(platform=args.platform, tr_version=tr_version)
            output = None
            if args.output_root:
                if tr_version is not None:
                    output = args.output_root / f"tr{tr_version}"
                else:
                    output = args.output_root / "trx"
            run_package(options=options, output=output, no_zip=args.no_zip)


def run_package(
    options: PackageOptions, output: Path | None, no_zip: bool
) -> None:
    if output:
        zip_path = output
        if zip_path.suffix.lower() != ".zip" and not no_zip:
            zip_path /= options.default_stem + ".zip"
    else:
        zip_path = Path()
        if no_zip:
            zip_path /= options.default_stem
        else:
            zip_path /= options.default_stem + ".zip"


    source_files: list[tuple[Path, str] | Path] = []

    for ship_dir in options.ship_dirs:
        stack = [ship_dir]
        while stack:
            current = stack.pop()
            for item in current.iterdir():
                try:
                    real = item.resolve(strict=True)
                except FileNotFoundError:
                    continue  # broken link, ignore
                if real.is_file():
                    source_files.append((real, str(item.relative_to(ship_dir))))
                elif real.is_dir():
                    stack.append(item)

    for path in options.release_zip_files:
        source_files.append(path)

    if no_zip:
        for src_path, dst_name in source_files:
            dst_path = zip_path / dst_name
            dst_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(src_path, dst_path)
    else:
        zip_path.parent.mkdir(parents=True, exist_ok=True)
        create_zip(zip_path, source_files)
    print(f"Created {zip_path}")


def parse_args(
    commands: dict[str, BaseCommand], **kwargs
) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Docker entrypoint")
    subparsers = parser.add_subparsers(dest="action", help="Subcommands")
    parser.set_defaults(action="build", command=commands["build"])
    parser.set_defaults(**kwargs)

    for command in commands.values():
        subparser = subparsers.add_parser(command.name, help=command.help)
        command.decorate_parser(subparser)
        subparser.set_defaults(command=command)
        subparser.set_defaults(**kwargs)
    result = parser.parse_args()
    return result


def main(**kwargs: Any) -> None:
    commands = {
        command_cls.name: command_cls()
        for command_cls in BaseCommand.__subclasses__()
    }
    args = parse_args(commands, **kwargs)
    args.command.run(args)
