# Integration tests

This directory contains pytest-based integration tests for Anjay Lite.

The tests build and run a small native `test_app` linked with Anjay Lite. Python tests control this app through a local JSON-RPC socket and verify its real LwM2M/CoAP traffic using Python-side test servers from `framework_tools`.

## Directory layout

```
tests/integration/
  app/                 Native C/C++ test application
  app_wrapper/         Python wrapper used to control the app over RPC
  tests/               Integration tests
  conftest.py          pytest fixtures: app, app_spawner
  app_prebuild.py      Prebuilds and caches test app variants (depending on test configuration) before tests run
  utils.py             common pytest/test helpers
  pytest.toml          pytest configuration
  mbedtls/             CMake project to build mbedTLS to use by the test app
```

## How the tests work

A typical test does the following:

1. Starts one or more fake LwM2M servers in Python.
1. Spawns the native `test_app`.
1. Calls [app-side commands](#existing-app-commands) using `app.rpc.call(...)`.
1. Receives LwM2M packets from the fake server.
1. Sends server responses or server-initiated requests.
1. Lets the test fixture clean up the app process.

Example:

```python
from framework_tools.lwm2m.server import Lwm2mServer
import framework_tools.lwm2m.messages as msgs
import utils

def test_register(app_spawner):
    server = Lwm2mServer()
    uri = f"coap://127.0.0.1:{server.get_listen_port()}"
    with app_spawner.spawn_app() as app:
        assert app.rpc.call("init", {
            "endpoint": "test-endpoint",
            "servers": [{
                "uri": uri,
                "security": {
                    "kind": "nosec",
                },
            }],
        }) == 0

        pkt = server.recv()

        utils.assert_msg_equal(
            msgs.Lwm2mRegister(
                "/rd?ep=test-endpoint&lt=50&lwm2m=1.2&b=U"
            ),
            pkt,
        )
```

## Writing tests

Put integration tests in the `tests/` directory.

pytest discovers tests using naming conventions:

- test file names should start with `test_`, for example `test_update.py`
- test function names should start with `test_`, for example `test_register()`

A test that needs the native app should request the `app_spawner` fixture:

```python
def test_something(app_spawner):
    with app_spawner.spawn_app() as app:
        # ...
```

Always prefer using `with app_spawner.spawn_app() as app:`. The context manager closes the app control socket and waits for the app process to exit cleanly.

If a spawned app is leaked, the `app_spawner` fixture terminates it and fails the test.

### LwM2M servers

Create test servers with `Lwm2mServer`:

```python
from framework_tools.lwm2m.server import Lwm2mServer
server = Lwm2mServer()
uri = f"coap://127.0.0.1:{server.get_listen_port()}"
```

For PSK/DTLS tests:

```python
from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.transport import Transport
server = Lwm2mServer(coap.TlsServer(
    psk_identity="identity",
    psk_key="secret",
    transport=Transport.UDP,
))

uri = f"coaps://127.0.0.1:{server.get_listen_port()}"
```

Receive packets from the app:

```python
pkt = server.recv()
```

Send packets to the app:

```python
resp = msgs.Lwm2mChanged.matching(pkt)()
server.send(resp)
```

Compare packets with:

```python
import framework_tools.lwm2m.messages as msgs
expected = msgs.Lwm2mRegister('/rd?ep=test-endpoint&lt=50&lwm2m=1.2&b=U')
utils.assert_msg_equal(expected, pkt)
```

Use `utils.assert_msg_equal()` instead of plain asserts for LwM2M messages. It follows
the semantics of partially-defined expected messages (e.g. known CoAP code but unknown Token),
and prints better error messages.

## Test app RPC

The native test app exposes named commands. Python calls them with:

```python
app.rpc.call("some_command_name", arg1, arg2, ...)
```

For example:

```python
assert app.rpc.call("init", {
    "endpoint": "test-endpoint",
    "servers": [{
        "uri": "coap://127.0.0.1:12345",
        "security": {
            "kind": "nosec",
        },
    }],
}) == 0
```

This calls the C++ function:

```cpp
int init(const Config &config);
```

Arguments are serialized as JSON and deserialized into the C++ function argument types. Return values are serialized back to JSON.

If the app returns an RPC-level error, Python raises an exception. If the C++ command returns an integer error code, it is returned normally and should be checked by the test.

For more details, see [Adding a new app command](#adding-a-new-app-command).

## Existing app commands

### `init`

Initializes the test app as an Anjay Lite client.

Example nosec config:

```python
app.rpc.call("init", {
    "endpoint": "test-endpoint",
    "servers": [{
        "uri": "coap://127.0.0.1:12345",
        "security": {
            "kind": "nosec",
        },
        "lifetime": 60,
    }],
})
```

Example PSK config:

```python
app.rpc.call("init", {
    "endpoint": "test-endpoint",
    "servers": [{
        "uri": "coaps://127.0.0.1:12345",
        "security": {
            "kind": "psk",
            "psk_identity": "identity",
            "psk_key": "secret",
        },
        "lifetime": 60,
    }],
})
```

Example bootstrap config:

```python
app.rpc.call("init", {
    "endpoint": "test-endpoint",
    "servers": [{
        "bootstrap": True,
        "uri": "coaps://127.0.0.1:12345",
        "security": {
            "kind": "psk",
            "psk_identity": "identity",
            "psk_key": "bootstrap-secret",
        },
    }],
})
```

Optional UDP retransmission parameters:

```python
app.rpc.call("init", {
    "endpoint": "test-endpoint",
    "udp_tx_params": {
        "ack_timeout_s": 1,
        "ack_random_factor": 1.01,
        "max_retransmit": 5,
    },
    "servers": [
        # ...
    ],
})
```

Currently supported security kinds are:

- `nosec`
- `psk`

### `send_lifetime`

Queues a LwM2M Send operation containing the Server Object Lifetime resource:

```python
assert app.rpc.call("send_lifetime") == 0
```

## Adding a new app command

App commands are registered in `app/src/commands.hpp`.

To add a new command:

1. Define any argument structures in `commands.hpp`.
1. Add `nlohmann::json` mapping for those structures.
1. Declare the command function.
1. Implement the function in the test app sources.
1. Register the function in `wrap_map`.
1. Call it from Python using `app.rpc.call(...)`.

Example C++ declaration and registration:

```cpp
struct SetFooConfig {
    int value;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetFooConfig, value);

int set_foo(const SetFooConfig &config);

inline std::unordered_map<std::string, utils::AutoWrap> wrap_map = {
    { "init", init },
    { "send_lifetime", send_lifetime },
    { "set_foo", set_foo },
};
```

Example implementation:

```cpp
int set_foo(const SetFooConfig &config) {
    // Use config.value here.
    return 0;
}
```

Example Python call:

```python
assert app.rpc.call("set_foo", {
    "value": 42,
}) == 0
```

RPC arguments are positional. A C++ function like:

```cpp
int get_bar(int a, const std::string &b);
```

is called as:

```python
app.rpc.call("get_bar", 1, "text")
```

For struct arguments, JSON field names must match the fields used by the C++ JSON mapping.

Optional C++ fields represented as `std::optional<T>` may be omitted from JSON input.

## App build configuration

Some tests need the native app to be built with specific CMake options.

Use `utils.app_config(...)`:

```python
import utils

@utils.app_config({
    "ANJ_WITH_LWM2M12": "OFF",
    "ANJ_WITH_LWM2M_CBOR": "OFF",
})
def test_lwm2m11_case(app_spawner):
    # ...
```

Each key/value pair becomes a CMake option:

```
-DANJ_WITH_LWM2M12=OFF
-DANJ_WITH_LWM2M_CBOR=OFF
```

Another example:

```python
@utils.app_config({
    "ANJ_LOG_LEVEL_DEFAULT": "L_TRACE",
})
def test_with_trace_logs(app_spawner):
    # ...
```

Each distinct app config gets a separate cached build directory.

The framework prebuilds all app configurations required by the selected tests before running them. This makes build failures happen early and avoids build races during parallel test runs.

## Running tests

The tests should be ran from `tests/integration` directory, with activated venv that is prepared by `./devconfig`. To do so, run in the root of the repo:

```sh
./devconfig
. venv/bin/activate
cd tests/integration
```

Run all tests:

```sh
pytest
```

Run one file:

```sh
pytest tests/test_update.py
```

Run one test:

```sh
pytest tests/test_update.py::test_update_sent_on_registration_update_trigger
```

Run tests whose names match an expression:

```sh
pytest -k update
```

Run with verbose test names:

```sh
pytest -vv
```

Show stdout/stderr while tests are running:

```sh
pytest -s
```

Useful when debugging app logs or test progress.

Stop after the first failure:

```sh
pytest -x
```

Run tests in parallel:

```sh
pytest -n auto
```

List selected tests without running them:

```sh
pytest --collect-only
```
