from pathlib import Path

REPO_DIR = Path(__file__).parent
while REPO_DIR.parent != REPO_DIR and not (REPO_DIR / ".git").exists():
    REPO_DIR = REPO_DIR.parent

TOOLS_DIR = REPO_DIR / "tools"
SRC_DIR = REPO_DIR / "src"
DATA_DIR = REPO_DIR / "data"
DOCS_DIR = REPO_DIR / "docs"

SHARED_SRC_DIR = SRC_DIR / "trx"


class BasePaths:
    data_dir: Path
    shipped_data_dir: Path
    src_dir: Path
    docs_dir: Path


class ProjectPaths(BasePaths):
    def __init__(self, folder_name: str) -> None:
        self.folder_name = folder_name

        self.data_dir = DATA_DIR / folder_name
        self.shipped_data_dir = self.data_dir / "ship"
        self.tools_dir = TOOLS_DIR / folder_name
        self.docs_dir = DOCS_DIR / folder_name
        self.changelog_path = self.docs_dir / "CHANGELOG.md"


TR1Paths = ProjectPaths(folder_name="tr1")
TR2Paths = ProjectPaths(folder_name="tr2")
TR3Paths = ProjectPaths(folder_name="tr3")

CommonPaths = BasePaths()
CommonPaths.data_dir = DATA_DIR / "common"
CommonPaths.shipped_data_dir = CommonPaths.data_dir / "ship"
CommonPaths.src_dir = SHARED_SRC_DIR
CommonPaths.changelog_path = DOCS_DIR / "CHANGELOG.md"
CommonPaths.docs_dir = DOCS_DIR

PROJECT_PATHS = {1: TR1Paths, 2: TR2Paths, 3: TR3Paths}
