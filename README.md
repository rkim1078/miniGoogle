# Mini Google
A fully-functional simplified version of Google, adapted from a [course project](https://courses.cs.washington.edu/courses/cse333/25au/) to be portable on WSL.

Built from the ground up:
- Implementing data structures in C to store information
- A local search engine in C++ capable of saving and reading index files from disk
- A polished, internet-accessible version of this search engine

## Overview
This project is organized as follows:
- `miniGoogle/hw1`: C implementation of a doubly-linked list and a chained hashtable
- `miniGoogle/hw2`: C implementation of a local search engine, functions much like `grep`
- `miniGoogle/hw3`: C++ adaptation of local search engine, able to save/read files from disk
- `miniGoogle/hw4`: Internet-accessible version of search engine, complete with front-end, server-side C++ networking code that listens for and accepts client connections, and defenses against XSS and directory traversal attacks
- `miniGoogle/projdocs/test_tree`: benchmark test cases for each portion; see Testing for more

## Installation/Usage
Install basic prerequisites and dependencies, if not already present:
```
sudo apt update
sudo apt install -y build-essential gdb clang cmake
```
You can verify installation of dependencies with:
```
gcc --version
make --version
```
**Note for WSL:** Do not run or store this project under `/mnt` on WSL. Windows-backed filesystems break Linux symlinks used in this project, and will typically break builds. Always keep the project inside your WSL home directory.

Clone repository:
```
git clone placeholder
cd miniGoogle/
```
Build using makefile:
```
for d in hw1 hw2 hw3 hw4; do (cd "$d" && make); done
```
For a clean build, if necessary:
```
for d in hw1 hw2 hw3 hw4; do (cd "$d" && make clean && make); done
```
To start the Internet-accessible server (any valid port number should do):
```
./http333d 5555 ../projdocs unit_test_indices/*
```
If the server is running on a local machine, visit: http://localhost:5555/ on Firefox or Chrome, as well as http://localhost:5555/static/bikeapalooza_2011/Bikeapalooza.html. To exit, visit http://localhost:5555/quitquitquit or find the PID with `ps -u` and `kill <pid>`. If the server is hosted over an SSH connection, visit the same pages using the domain of that connection instead.

## Testing
All tests, neatly organized:
```
for d in hw1 hw2 hw3 hw4; do echo "Running $d/test_suite..."; (cd "$d" && ./test_suite); done
```
Or, in each `miniGoogle/hwN` directory, you can run those tests after building by running the `test_suite`:
```
./test_suite
```
You can also check out the local search engines from parts 2 and 3, respectively, using:
```
cd hw2
./searchshell ./test_tree
```
```
./filesearchshell unit_test_indices/*.idx
```
