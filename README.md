uvm_package_gpc
===================

tool to package uvm bytecode and contract metadata to one file


# Dependencies

* boost 1.55
* https://github.com/cryptonomex/fc
* https://github.com/BlockLink/jsondiff-cpp
* `git submodule update --init --recursive`

# Usage

* `cmake .` and `make` to build
* package_gpc path-to-uvm-bytecode-file path-to-contract-metadata-file
