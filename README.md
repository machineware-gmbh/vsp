# VCML Session Protocol (VSP) Client

[![Build Status](https://github.com/machineware-gmbh/vsp/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vsp/actions/workflows/cmake.yml)
[![Lint Status](https://github.com/machineware-gmbh/vsp/actions/workflows/lint.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vsp/actions/workflows/lint.yml)
[![Style Status](https://github.com/machineware-gmbh/vsp/actions/workflows/style.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vsp/actions/workflows/style.yml)
[![Sanitzer Status](https://github.com/machineware-gmbh/vsp/actions/workflows/asan.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vsp/actions/workflows/asan.yml)
[![Nightly Status](https://github.com/machineware-gmbh/vsp/actions/workflows/nightly.yml/badge.svg?branch=main)](https://github.com/machineware-gmbh/vsp/actions/workflows/nightly.yml)

This repository contains a C++ implementation of a VCML Session Protocol (VSP) client.
A standalone CLI-based application can be used to connect to a VSP server from the terminal.
Documentation of the VSP can be found [here](https://github.com/machineware-gmbh/vcml/blob/main/doc/session.md).

----

## CMake Options

The following CMake options can be used during confiuration

| CMake Option    | Default | Description                  |
|-----------------|---------|------------------------------|
| `VSP_TESTS`     | `OFF`   | Build test suite             |
| `VSP_COVERAGE`  | `OFF`   | Generate code-coverage data  |
| `VSP_LINTER`    |         | Specify a code linter to use |
| `VSP_CLI`       | `OFF`   | Build the CLI application    |

----

## Configuration and Build

Use CMake to configure the project.
Example for a debug build:

| Placeholder     | Description                                                                     | Example               |
|-----------------|---------------------------------------------------------------------------------|-----------------------|
| `<project_dir>` | Working directory where the repositiory is cloned                               | `~/vsp`               |
| `<build_dir>`   | Build directory where the build artifacts are stored                            | `<project_dir>/build` |
| `<install_dir>` | Install directory where the libray, executables, and header files are copied to | `/opt/vsp`            |

```bash
git clone --recursive https://github.com/machineware-gmbh/vsp.git <project_dir> # clone the repository and its submodules
mkdir -p <build_dir> # create the build directory
cd <build_dir> # change directory to the build dir
cmake \
    -DCMAKE_BUILD_TYPE=DEBUG \
    -DCMAKE_INSTALL_PREFIX=<install_dir> \
    -DVSP_CLI=ON \
    <project_dir> # configure the project
cmake --build <build_dir> -- -j $(nproc) # build the project
cmake --build <build_dir> -t install # install the project
```

After installation, you should find the header files, library, and the CLI application (if enabled) in the `<install_dir>`.

----

## Simulation Events

Modules can report what happens in the simulation: transactions on their
sockets and registers (traces), LED changes and UART output. Installing a
handler on a module subscribes to these events for that module and everything
below it; passing `nullptr` removes the handler and unsubscribes. Handlers can
only be changed while the simulation is stopped.

Events arrive with the status updates of the session and handlers run on the
calling thread: when `step` or `stop` return, all events up to that point
have been delivered. After `run`, call `check_running` regularly, otherwise
the simulator drops the oldest events (see `session::on_events_dropped`).

```cpp
#include <vsp.h>

vsp::session sess("localhost", 4444);
vsp::module* cpu = sess.find_module("system.cpu0");

// traces of one protocol, as a typed struct
cpu->on_trace<vsp::trace_tlm>(
    [](const vsp::trace_info& info, const vsp::trace_tlm& tx) {
        printf("%s %s @0x%llx\n", info.port.name(), tx.command.c_str(),
               (unsigned long long)tx.address);
    });

// traces of all protocols, the payload holds one of the trace_* structs
sess.find_module("system")->on_trace(
    [](const vsp::trace_info& info, const vsp::trace_payload& tx) {
        std::cout << info << " " << tx << std::endl;
    });

// led changes and uart output of everything below the board
vsp::module* board = sess.find_module("system.board");
board->on_led([](const vsp::led_event& ev) {
    printf("%s led %zu: %d\n", ev.leds.name(), ev.index, ev.state);
});
board->on_uart([](const vsp::uart_event& ev) { std::cout << ev.data; });

sess.step(1000, 10000); // 1us, wait up to 10s; handlers have been called

cpu->on_trace<vsp::trace_tlm>(nullptr); // unsubscribe
```

`module::is_traceable()`, `has_leds()` and `has_uart()` tell whether a module
or one of its children can report these events.

In the CLI, use `events add trace|led|uart <obj>...`, `events rm ...` and
`events log [file]`; `events` shows the status.

----

## License

This project is proprietary and confidential work and requires a separate
license agreement to use - see the [LICENSE](LICENSE) file for details.
