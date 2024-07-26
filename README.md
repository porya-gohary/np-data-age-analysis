<h1 align="center">
  Data-age analysis for multi-rate task chains
</h1>

<h4 align="center">An analysis framework for obtaining the bounds of task chains' data age in a multi-rate directed acyclic graph (DAG)</h4>

<p align="center">
  <a href="https://github.com/porya-gohary/Multi-rate-DAG-Framework/blob/master/LICENSE.md">
    <img src="https://img.shields.io/hexpm/l/apa"
         alt="Gitter">
  </a>
    <img src="https://img.shields.io/badge/Made%20with-C++-orange">

</p>
<p align="center">
  <a href="#-dependencies-and-required-packages">Dependencies</a> •
  <a href="#-build-instructions">Build</a> •
  <a href="#-input-format">Input Format</a> •
  <a href="#%EF%B8%8F-usage">Usage</a> •
  <a href="#%EF%B8%8F-output-files">Output Files</a> •
  <a href="#-license">License</a>
</p>

## 📜 Introduction
This repository contains the implementations of the data-age analysis for multi-rate task chains with **non-preemptive tasks**. 
The analysis framework is based on the start and finish times that are extracted from [schedule-abstraction graph](https://github.com/gnelissen/np-schedulability-analysis) analysis 
and is capable of obtaining the bounds of task chains' data age in a multi-rate directed acyclic graph (DAG).

⚠️ Note: This work uses the classic definition of data age, which is the longest delay between any source job,
and the latest sink job that is related to that source job via one of the chain instances.

For more information, please refer to the following paper:
```
@inproceedings{gohari2022data,
  title={Data-age analysis for multi-rate task chains under timing uncertainty},
  author={Gohari, Pourya and Nasri, Mitra and Voeten, Jeroen},
  booktitle={Proceedings of the 30th International Conference on Real-Time Networks and Systems},
  pages={24--35},
  year={2022}
}
```

## 📦 Dependencies and Required Packages
- A modern C++ compiler supports the **C++14 standard**. Recent versions of `clang` and `g++` on Linux is known to work.

- The [CMake](https://cmake.org) build system. For installation using `apt` (Ubuntu, Debian...):
```bash
  sudo apt-get install cmake -y
```
- The [yaml-cpp](https://github.com/jbeder/yaml-cpp) library.


## 📋 Build Instructions
These instructions assume a Linux host.

If `yaml-cpp` is not installed on your system, its submodule should be pulled by running the following command:
```bash
git submodule update --init --recursive
```

To compile the tool, first generate an appropriate `Makefile` with `cmake` and then use it to actually build the source tree.
```bash
    # 1. enter the build directory
    $ cd build
    # 2. generate the Makefile
    $ cmake ..
    # 3. build everything
    $ make -j

```

## 📄 Input Format
The tool works with Yaml input files ([Example](./examples/dag-task-3.prec.yaml)). Each Yaml file contains the following information:
- Task specification:
    * Task ID 
    * Vertex ID (could be same as Task ID)
    * Best-case execution time (BCET)
    * Worse-case execution time (WCET)
    * Period
    * Deadline
    * Mapped processing element (PE)
    * Set of successors
  
## ⚙️ Usage
The options of the analysis are as follows (`./run_analysis -h`):
```
Usage: run_analysis [OPTIONS]... [DAG Task]...

Data-age Analysis for Multi-rate Task chains

Options:
  -h, --help            show this help message and exit
  -m NUM_PROCESSORS, --multiprocessor=NUM_PROCESSORS
                        set the number of processors of the platform
  -t TIME-MODEL, --time=TIME-MODEL
                        choose 'discrete' or 'dense' time (default: discrete)
  -l TIMEOUT, --time-limit=TIMEOUT
                        maximum CPU time allowed (in seconds, zero means no limit)
  -d DEPTH, --depth-limit=DEPTH
                        abort graph exploration after reaching given depth (>= 2)
  -n, --naive           use the naive exploration method (default: merging)
  -w, --wcet            use WCET as actual execution time and zero jitter for
                        every job (default: off)
  --header              print a column header
  -g, --save-graph      store the state graph in Graphviz dot format (default: off)
  -r, --save-response-times
                        store the best- and worst-case response times
                        (default: off)
```
### Example
To run the analysis for a partitioned system with four processing elements (PE):
```bash
./run_analysis ../examples/dag-task-3.prec.yaml -m 4 -w
```
This will run the analysis for ([dag-task-3.prec.yaml](./examples/dag-task-3.prec.yaml)) with four cores, without timing uncertainty, EDF scheduling policy and partitioned scheduler.

⚠️ Note: By using the partitioned scheduler, the mapped PE for each task is read from the input file.


## 🗄️ Output Files
The output is provided in two CSV format files and consists of the following:

* Runtime of response-time analysis (Schedule-abstraction graph analysis)
* Data-age bound of each task chain




## 📜 License
Copyright © 2022 [Pourya Gohari](https://pourya-gohari.ir)

Schedule-abstraction graph analysis adapted from [here](https://github.com/gnelissen/np-schedulability-analysis).

This project is licensed under the Apache License 2.0 - see the [LICENSE.md](LICENSE.md) file for details