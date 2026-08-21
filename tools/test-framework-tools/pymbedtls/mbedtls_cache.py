# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import argparse
import hashlib
import shutil
import subprocess
import sys
import json
import filelock
import os

from pathlib import Path
from git import Repo, InvalidGitRepositoryError, NoSuchPathError, GitCommandError, Git


MBEDTLS_REPO_URL = "https://github.com/Mbed-TLS/mbedtls"
INSTALLED_MBEDTLS_CONFIG_FILE = "mbedtls_config_custom.h"


# Debug function to print verbose output
VERBOSE_OUTPUT = False
def print_v(*args, **kwargs):
    if VERBOSE_OUTPUT:
        print(
            *args,
            file=sys.stderr,
            **kwargs,
        )

class MbedTLSConfigEditor:
    """
    Wrapper around MbedTLS config.py script. Executes config.py
    with the same interpreter as the current script.

    Provides API for setting and unsetting config options in a selected
    mbedtls_config.h file.

    We call it by subprocess rather than importing config.py because
    it may change across different MbedTLS versions.
    """
    def __init__(self, source_dir: Path, config_file: Path):
        self.source_dir = source_dir
        self.config_file = config_file
        self.config_py = source_dir / "scripts" / "config.py"

    def _run(self, *args: str) -> None:
        subprocess.run(
            [
                sys.executable,
                str(self.config_py),
                "-f",
                str(self.config_file),
                *args,
            ],
            cwd=self.source_dir,
            check=True,
            # To be sure if the output doesn't accidentally get inside
            # cmake variable when script is called from CMake.
            stdout=sys.stderr,
            stderr=sys.stderr,
        )

    def set(self, name: str, value: str | None = None) -> None:
        if value is None:
            self._run("set", name)
        else:
            self._run("set", name, str(value))

    def unset(self, name: str) -> None:
        self._run("unset", name)

    def set_many(self, names: list[str]) -> None:
        for name in sorted(set(names)):
            self.set(name)

    def unset_many(self, names: list[str]) -> None:
        for name in sorted(set(names)):
            self.unset(name)

    def apply_profile(self, profile: dict) -> None:
        """
        Applies dict of format
        {
            set: [
                <list of binary config options to set>
            ],
            unset: [
                <list of binary config options to unset>
            ],
            values: {
                <name>: <value>,
                ...
            }
        }

        to default mbedtls_config.h.
        """
        for name in sorted(set(profile.get("unset", []))):
            self.unset(name)

        for name in sorted(set(profile.get("set", []))):
            self.set(name)

        for name, value in sorted(profile.get("values", {}).items()):
            self.set(name, str(value))

def is_empty_config(config: dict | None) -> bool:
    if config is None:
        return True

    return not any([
        config.get("set"),
        config.get("unset"),
        config.get("values"),
    ])

def get_config_name(
        config: dict | None,
        make_PIC: bool = False,
) -> str:
    hash_input = {
        "config": config,
        "make_PIC": make_PIC,
    }
    config_buffer = json.dumps(hash_input, sort_keys=True).encode()
    config_hash = hashlib.sha256(config_buffer).hexdigest()

    if is_empty_config(config):
        return f"default-{config_hash}"

    if "config_name" in config:
        return f"{config['config_name']}-{config_hash}"

    return config_hash

def prepare_mbedtls_config_file(
        source_path: Path,
        configs_dir: Path,
        config_name: str,
        config: dict | None,
) -> Path:
    config_dir = configs_dir / config_name
    config_dir.mkdir(parents=True, exist_ok=True)

    config_file = config_dir / "mbedtls_config.h"

    if config_file.exists():
        print_v(f"Config already exists: {config_file}")
        return config_file

    default_config_file = source_path / "include" / "mbedtls" / "mbedtls_config.h"

    if not default_config_file.exists():
        raise RuntimeError(f"Default MbedTLS config not found: {default_config_file}")

    config_file = config_dir / INSTALLED_MBEDTLS_CONFIG_FILE
    shutil.copy2(default_config_file, config_file)

    if config is not None:
        mbedtls_config_editor = MbedTLSConfigEditor(
            source_dir=source_path,
            config_file=config_file,
        )
        mbedtls_config_editor.apply_profile(config)

    return config_file

def ensure_submodules(source_path: Path) -> None:
    repo = Repo(str(source_path))

    if not (source_path / ".gitmodules").exists():
        print_v(f"No submodules found in: {source_path}")
        return

    print_v(f"Synchronizing submodules in: {source_path}")
    repo.git.submodule("sync", "--recursive")

    print_v(f"Updating submodules in: {source_path}")
    repo.git.submodule("update", "--init", "--recursive")

def is_valid_git_repo(path: Path) -> bool:
    try:
        Repo(str(path))
        return True
    except (InvalidGitRepositoryError, NoSuchPathError):
        return False


def ensure_mirror_repo(repo_url: str, repo_path: Path) -> Repo:
    repo_path.parent.mkdir(parents=True, exist_ok=True)

    if not repo_path.exists():
        print_v(f"Cloning mirror repo to: {repo_path}")
        return Repo.clone_from(repo_url, str(repo_path), mirror=True)

    if not is_valid_git_repo(repo_path):
        raise RuntimeError(
            f"Path exists but is not a valid Git repository: {repo_path}"
        )

    repo = Repo(str(repo_path))

    if not repo.bare:
        raise RuntimeError(
            f"Expected bare mirror repository, but got non-bare repo: {repo_path}"
        )

    print_v(f"Updating existing mirror repo: {repo_path}")

    # For mirror repos this is usually better than normal pull.
    repo.git.remote("update", "--prune")
    repo.git.fetch("--tags", "--prune")

    return repo


def ensure_worktree(repo: Repo, source_path: Path, ref: str) -> Path:
    source_path.parent.mkdir(parents=True, exist_ok=True)

    if source_path.exists():
        source_repo = Repo(str(source_path))
        current_commit = source_repo.git.rev_parse("HEAD")
        wanted_commit = repo.git.rev_parse(f"{ref}^{{commit}}")

        if current_commit == wanted_commit:
            print_v(f"Worktree already exists and matches {ref}: {source_path}")
            ensure_submodules(source_path)
            return source_path

        raise RuntimeError(
            f"Worktree exists but points to {current_commit}, "
            f"expected {wanted_commit} for {ref}: {source_path}"
        )

    # clean up any existing worktrees that were deleted manually
    repo.git.worktree("prune")

    try:
        repo.git.worktree(
            "add",
            "--detach",
            str(source_path),
            ref,
        )
    except GitCommandError as exc:
        stderr = exc.stderr or ""

        if "missing but already registered worktree" in stderr:
            repo.git.worktree("prune")

            repo.git.worktree(
                "add",
                "--force",
                "--detach",
                str(source_path),
                ref,
            )
        else:
            raise

    return source_path


def is_valid_mbedtls_package_dir(mbedtls_root_dir: Path) -> bool:
    return (mbedtls_root_dir / "MbedTLSTargets.cmake").exists()


def read_success_file(success_file: Path, mbedtls_root_dir: Path) -> Path | None:
    if not success_file.exists():
        return None

    if is_valid_mbedtls_package_dir(mbedtls_root_dir):
        print_v(f"Build success file found: {success_file}")
        return mbedtls_root_dir

    print_v(f"Ignoring stale build success file: {success_file}")
    return None

def write_success_file(success_file: Path) -> None:
    success_file.parent.mkdir(parents=True, exist_ok=True)
    tmp_success_file = success_file.with_name(
        f"{success_file.name}.{os.getpid()}.tmp"
    )
    tmp_success_file.touch()
    os.replace(tmp_success_file, success_file)

def get_build_success_file(
        builds_dir: Path,
        config: dict | None = None,
        make_PIC: bool = False,
) -> Path:
    return builds_dir / f"{get_config_name(config, make_PIC)}.success"

def ensure_build(source_path: Path, builds_dir: Path, config: dict | None = None, make_PIC: bool = False) -> Path:
    build_dir_name = get_config_name(config, make_PIC)

    build_path = builds_dir / build_dir_name
    cmake_config_dir = builds_dir / f"{build_dir_name}_tmp"
    configs_dir = builds_dir / "configs"
    success_file = get_build_success_file(builds_dir, config, make_PIC)

    expected_mbedtls_root_dir = build_path / "lib" / "cmake" / "MbedTLS"

    if mbedtls_root_dir := read_success_file(
            success_file,
            expected_mbedtls_root_dir,
    ):
        return mbedtls_root_dir

    repo = Repo(str(source_path))

    # Reuse the same MbedTLS worktree for different configurations. Before
    # preparing a new build, reset any files that may have been modified by a
    # previous configuration attempt.
    repo.git.reset("--hard", "HEAD")
    repo.git.clean("-xdf")

    config_file = prepare_mbedtls_config_file(
        source_path=source_path,
        configs_dir=configs_dir,
        config_name=build_dir_name,
        config=config,
    )

    # If an install tree already exists but the success marker is missing,
    # validate the minimal package structure and recreate the marker
    if is_valid_mbedtls_package_dir(expected_mbedtls_root_dir):
        print_v(f"Build already exists: {build_path}")
        write_success_file(success_file)
        return expected_mbedtls_root_dir

    if cmake_config_dir.exists():
        shutil.rmtree(cmake_config_dir)

    subprocess.run(
        [
            "cmake",
            "-S", str(source_path),
            "-B", str(cmake_config_dir),
            f"-DCMAKE_INSTALL_PREFIX={build_path.resolve()}",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_POSITION_INDEPENDENT_CODE={'ON' if make_PIC else 'OFF'}",
            "-DENABLE_PROGRAMS=OFF",
            "-DENABLE_TESTING=OFF",
            "-DCMAKE_INSTALL_LIBDIR=lib",
            # Keep MBEDTLS_CONFIG_FILE relative so that the installed CMake
            # package does not contain an absolute path to the generated
            # config file. During this build, the generated config is made
            # visible through CMAKE_C_FLAGS below
            f"-DMBEDTLS_CONFIG_FILE:STRING={INSTALLED_MBEDTLS_CONFIG_FILE}",
            (
                f"-DCMAKE_C_FLAGS=-I{config_file.parent} "
                # Add the generated config directory to the compiler include
                # path so MBEDTLS_CONFIG_FILE="mbedtls_config.h" resolves to
                # the config prepared for this build
                #
                # Older MbedTLS releases may fail with newer GCC versions due
                # to -Wunterminated-string-initialization. Disable that warning.
                # Apple Clang does not recognize this GCC warning option, so
                # also suppress unknown-warning-option diagnostics
                "-Wno-unknown-warning-option "
                "-Wno-unterminated-string-initialization"
            )
        ],
        check=True,
        stdout=sys.stderr,
        stderr=sys.stderr,
    )

    subprocess.run(
        ["cmake", "--build", str(cmake_config_dir), "--target", "install", "--parallel"],
        check=True,
        stdout=sys.stderr,
        stderr=sys.stderr,
    )

    # Install the generated config next to MbedTLS headers. Consumers of the
    # installed package will resolve MBEDTLS_CONFIG_FILE="mbedtls_config.h"
    # through the package include paths, without relying on the temporary
    # configs/<cfg_hash> directory used during the build
    #
    # This keeps the installed MbedTLS package relocatable, which is useful
    # when the build cache is moved between CI stages or restored under a
    # different filesystem path
    installed_config_file = build_path / "include" / INSTALLED_MBEDTLS_CONFIG_FILE
    shutil.copy2(config_file, installed_config_file)

    write_success_file(success_file)
    return expected_mbedtls_root_dir


def get_current_repo_root() -> Path:
    current_repo = Repo(search_parent_directories=True)
    return Path(current_repo.working_dir)

def setup_git_safe_directories(*paths: Path) -> None:
    """
    Allow Git operations on cache repositories that may be owned by another
    user, e.g. when restored from CI cache or used inside a container

    This does not make Git worktrees relocatable. If the cache directory is
    moved, Git metadata may still contain stale absolute paths and the source
    cache may need to be recreated
    """
    os.environ["GIT_CONFIG_COUNT"] = str(len(paths))

    for i, path in enumerate(paths):
        os.environ[f"GIT_CONFIG_KEY_{i}"] = "safe.directory"
        os.environ[f"GIT_CONFIG_VALUE_{i}"] = str(path.resolve())

def ensure_mbedtls_build(
        version: str = "v3.6.5",
        config: dict | None = None,
        cache_dir: Path | None = None,
        make_PIC: bool = False
) -> Path:
    if cache_dir is None:
        cache_dir = get_current_repo_root() / ".mbedtls_cache"
    else:
        cache_dir = Path(cache_dir)

    cache_dir.mkdir(parents=True, exist_ok=True)
    git_cache_dir = cache_dir / "git"
    sources_cache_dir = cache_dir / "src"
    builds_cache_dir = cache_dir / "builds"

    mbedtls_repo_path = git_cache_dir / "mbedtls.git"
    mbedtls_sources_path = sources_cache_dir / version
    mbedtls_build_path = builds_cache_dir / version

    build_config_name = get_config_name(config, make_PIC)
    expected_mbedtls_root_dir = (
        mbedtls_build_path / build_config_name / "lib" / "cmake" / "MbedTLS"
    )
    success_file = get_build_success_file(mbedtls_build_path, config, make_PIC)

    lock_file_mirror = cache_dir / "locks" / "lock-mirror.lock"
    lock_file_worktree = cache_dir / "locks" / f"lock-{version}.lock"
    lock_file_config = cache_dir / "locks" / version / f"config-{build_config_name}.lock"

    lock_file_mirror.parent.mkdir(parents=True, exist_ok=True)
    lock_file_worktree.parent.mkdir(parents=True, exist_ok=True)
    lock_file_config.parent.mkdir(parents=True, exist_ok=True)

    setup_git_safe_directories(mbedtls_repo_path, mbedtls_sources_path)

    with filelock.FileLock(lock_file_config):
        if mbedtls_root_dir := read_success_file(
                success_file,
                expected_mbedtls_root_dir,
        ):
            return mbedtls_root_dir

        with filelock.FileLock(lock_file_mirror):
            mbedtls_repo = ensure_mirror_repo(
                repo_url=MBEDTLS_REPO_URL,
                repo_path=mbedtls_repo_path,
            )

        with filelock.FileLock(lock_file_worktree):
            source_path = ensure_worktree(
                repo=mbedtls_repo,
                source_path=mbedtls_sources_path,
                ref=version,
            )

            ensure_submodules(source_path)

        mbedtls_root_dir = ensure_build(
            source_path,
            mbedtls_build_path,
            make_PIC=make_PIC,
            config=config,
        )

    return mbedtls_root_dir

def main() -> None:
    parser = argparse.ArgumentParser(description="MBEDTLS Cache Utility")
    parser.add_argument(
        "--cache-dir",
        default=None,
        type=str,
        help="Directory for cache files",
    )
    parser.add_argument(
        "-v",
        "--version",
        default="v3.6.7",
        type=str,
        help="Version of MBEDTLS to checkout",
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Enable verbose output"
    )
    parser.add_argument(
        "--config",
        help="Mbedtls config option in format KEY=VALUE;KEY1=VALUE1, values can be ON/OFF/numeric/string",
        default=""
    )
    parser.add_argument(
        "-P", "--make-PIC", action="store_true",
        help="Build mbedtls with position independent code flag enabled. Turned OFF by default.",
        default=False
    )

    args = parser.parse_args()

    global VERBOSE_OUTPUT
    VERBOSE_OUTPUT = args.verbose

    config = {
        "set": [],
        "unset": [],
        "values": {},
    }

    if args.config:
        for cfg in args.config.split(";"):
            if "=" not in cfg:
                print(f"Invalid config option: {cfg}. Expected format KEY=VALUE.", file=sys.stderr)
                sys.exit(1)

            key, value = cfg.split("=", 1)
            if value.upper() in {"ON", "TRUE", "1"}:
                config["set"].append(key)
            elif value.upper() in {"OFF", "FALSE", "0"}:
                config["unset"].append(key)
            else:
                config["values"][key] = value

    cache_dir = (
        Path(args.cache_dir).resolve() # resolve to absolute path if needed
        if args.cache_dir is not None
        else get_current_repo_root() / ".mbedtls_cache"
    )

    mbedtls_root_dir = ensure_mbedtls_build(
        version=args.version,
        config=config,
        cache_dir=cache_dir,
        make_PIC=args.make_PIC
    )
    print(mbedtls_root_dir)


if __name__ == "__main__":
    main()
