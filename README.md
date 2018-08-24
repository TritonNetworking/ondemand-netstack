# DC Circuit Switch

## Overview

This repo holds the custom network stack that can be LD\_PRELOAD into unmodified application. Implementations can be any of standard socket, MPI, DPDK, etc.
The repo is organized as follows:

  * [`src`](src): includes all source code:
  * [`script`](script): includes script wrappers to run programs
  * [`build`](build): default CMake output directory (it's `git`-ignored)
  * [`docs`](docs): includes documentation

### On-going Development

MPI transport is currently being developed, which is on `feature/mpi_transport` branch.

## Run Instructions

### General

This project is CMake-based, so `cmake` is required to build it.

I tested all programs on Ubuntu 18.04 machines, with Mallenox OFED v4.4.

To get started, go to [`script`](script) directory and run [`build.sh`](script/build.sh) `-f` (subsequent builds can omit the `-f` flags, see below). This will generate outputs in [`build`](build) directory, following CMake output format.

  * [`build.sh`](script/build.sh) runs `cmake` and `make` for you. Here are the flags:
    * `-f`: removes everything under `build` folder and re-generate `cmake` output. **This is needed for the first time.**
    * `-d`: sets `cmake` build type to DEBUG. **Must be used with `-f`.**
    * `-c`: runs `make clean` before `make`.
  * MPI is required to build MPI code:
    * [`setup-mpi.sh`](script/setup-mpi.sh) loads the MPI library.
    * OpenMPI is used by default, and is included with OFED v4.4 (installed at `/usr/mpi/gcc/openmpi-3.1.1rc1`).
    * HPC-X can also be used by changing the flag in the script, but the default OpenMPI is recommended.
  * Configuration files:
    * [`config`](script/config) files under `script` and sub-directories set up environment variables.
    * [`hosts.config`](script/hosts.config) is a plaintext list of hostnames, used for launching parallel ssh and MPI commands.
  * Miscellaneous files:
    * [`install-tools.sh`](script/install-tools.sh) includes the packages needed.
    * [`setup-host.sh`](script/setup-host.sh) includes end host setup script for HPC environment.

