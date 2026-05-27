# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

"""
High-level test flow (why this exists):

- Tests may be parametrized with an "app config" dict (see utils.app_config()).
- For each distinct config used by collected tests, we prebuild a dedicated
  "test_app" binary in a persistent cache directory (pytest's cache).
- The "app" fixture then resolves (cfg -> build dir -> path to the built binary)
  and provides tests with both:
    - cfg: the config dict used to build the binary
    - path: filesystem path to the resulting executable

Pytest integration points used here:

- Fixture parametrization:
    pytest.mark.parametrize(..., indirect=True) injects parameters into fixtures
    via request.param (during test setup).
- Collection hook:
    pytest_collection_finish() runs once after collection, before tests execute.
    We use it to prebuild all needed binaries up-front (and only once per run,
    even under pytest-xdist).
"""

import pytest
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import app_prebuild
from app_wrapper.app_manager import AppManager

# A small typed wrapper that tests consume:
# - cfg is the (possibly empty) AppConfig dict used for the build
# - path points to the executable produced for that config


@dataclass(frozen=True)
class App:
    cfg: app_prebuild.AppConfig
    path: Path

# A helper wrapper for spawned app processes
# - process is the subprocess.Popen instance for the spawned app
# - wrapper is the AppManager instance connected to the app's control socket


@dataclass(frozen=True)
class SpawnedApp:
    process: subprocess.Popen
    wrapper: AppManager

    def connect(self):
        self.wrapper.connect()
        return self

    def close(self):
        self.wrapper.close()

    def wait(self, timeout=None):
        return self.process.wait(timeout=timeout)

    def terminate(self):
        self.process.terminate()

    def kill(self):
        self.process.kill()

    @property
    def pid(self):
        return self.process.pid

    @property
    def rpc(self):
        return self.wrapper

    # Context manager support for automatic cleanup (with statement)
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        assert self.wait(timeout=2) == 0
        return False

# A helper type to manage spawning apps and allowing type inferennce for the app_spawner fixture


@dataclass(frozen=True)
class Spawner:
    app: App
    spawned_apps: list[SpawnedApp]

    def spawn_app(self, timeout=1.0) -> SpawnedApp:
        wrapper = AppManager(host="localhost", timeout=timeout)
        wrapper.create_socket()

        process = subprocess.Popen(
            [str(self.app.path), str(wrapper.get_port())]
        )

        spawned = SpawnedApp(process=process, wrapper=wrapper)
        spawned.connect()

        self.spawned_apps.append(spawned)
        return spawned


@pytest.fixture
def app(request):
    # This fixture executes during the *setup phase* of each test that requests
    # "app" (or any fixture depending on "app").
    #
    # If the test used pytest.mark.parametrize(..., indirect=True) for "app",
    # pytest passes the parameter value through request.param.
    # If the test didn't parametrize "app", request.param is absent and we use
    # an empty config {} (meaning "default build").

    # NOTE: this is the canonical approach to check if a fixture is parametrized
    # with pytest.mark.parametrize(..., indirect=True). Exact same parameters
    # are found on collection phase with
    # app_prebuild.try_getting_parametrized_app_cfg
    app_cfg = getattr(request, "param", {}) or {}

    # Build output layout:
    # - app_prebuild.ensure_app_build_dir(...) returns a config-hash-specific
    #   directory in pytest's persistent cache, e.g. .pytest_cache/d/build/<hash>/
    # - the actual executable is expected to be named "test_app" inside it
    #
    # The build is triggered elsewhere (pytest_collection_finish), so here we
    # only *resolve the path* and let tests execute it.
    app_path = app_prebuild.ensure_app_build_dir(
        request.session, app_cfg) / "test_app"
    return App(cfg=app_cfg, path=app_path)


@pytest.fixture
def app_spawner(app):
    spawned_apps: list[SpawnedApp] = []

    yield Spawner(app=app, spawned_apps=spawned_apps)

    leaked_pids: list[int] = []

    for spawned in spawned_apps:
        if spawned.process.poll() is None:
            leaked_pids.append(spawned.pid)
            spawned.terminate()
            try:
                spawned.wait(timeout=5)
            except subprocess.TimeoutExpired:
                spawned.kill()
                spawned.wait()

    if leaked_pids:
        pytest.fail(
            f"Some spawned apps were still running after test completion: {leaked_pids}")


def pytest_collection_finish(session: pytest.Session):
    # Pytest hook: called once after the test collection phase completes and
    # session.items contains all collected tests. At this point we know exactly
    # which app configs are needed by the collected test set.
    #
    # We use this to prebuild *all distinct* app configurations up-front so that:
    # - tests fail early if build is broken
    # - individual test runs don't race to build the same binary
    # - under pytest-xdist, each worker calls this hook, but only one worker
    #   will acquire the lock and do the build, while others will wait for it to
    #   finish and see prebuild.success if it succeeded, or prebuild.error if it
    #   failed.

    # NOTE: don't build apps if we're just listing them with --collect-only
    if not session.config.getoption("collectonly", False):
        app_prebuild.ensure_all_configs_prebuilt(session)
