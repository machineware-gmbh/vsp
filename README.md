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
    -DVSP_BUILD_CLI=ON \
    <project_dir> # configure the project
cmake --build <build_dir> -- -j $(nproc) # build the project
cmake --build <build_dir> -t install # install the project
```

After installation, you should find the header files, library, and the CLI application (if enabled) in the `<install_dir>`.

----

## License

This project is proprietary and confidential work and requires a separate
license agreement to use - see the [LICENSE](LICENSE) file for details.
