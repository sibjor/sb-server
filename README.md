# SB-SERVER

- Note: CMake is used to generate a compile_commands.json for VSCode ClangD usage. 

```bash
# After building with CMake:
ln -s build/compile_commands.json compile_commands.json
```

- Assignee and author: Sixten Björling
- Teacher: Sebastian Jensen
- School: Forsbergs Skola, Kingdom of Sweden
- Starting Date: 24/6/2025
- [Tutorial](https://dev.to/jeffreythecoder/how-i-built-a-simple-http-server-from-scratch-using-c-739)
- [Tutorial repo](https://github.com/JeffreytheCoder/Simple-HTTP-Server)
- Method of refactoring: From C into C++.
- Tools: GCC, CMake, Visual Studio Code, ClangD, Git, Github
- OS: Raspberry Pi OS, Ubuntu OS, (Debian)

## About 

The C++ version is refactored from the C implementation which follows the tutorial linked above.

I chose to refactor a C tutorial I had stumbled upon earlier intended to be applied to act as a HTTP server software running on my Raspberry Pi 5 8GB at home on a 1000/mbit fibre connection hosting one or several of my domains. My thoughts is that it could possibly be expanded wrapping future assignments such as portfolio/frontend network & graphics/physics/game programming on one single environment hosting the projects products as well as source code and free licence publicly on the internet. However, the mission given by our teacher this time is cited: ___"When we refactor, we do not add new features to our code, nor do we solve problems or hunt for bugs: we only change it in ways that make it easier to read and maintain."___ Therefore, I will expand the project through separate git submodules, not interfering with the demands stated above.

## TODO:

1. [ ] Refactor or clean/remove some of the (many) libraries in use.
    - [x] Use `bool` instead of `stdbool.h`
    - [x] Use `<iostream>` for input/output, avoid `stdio.h`
    - [x] Use `std::string` from `<string>` instead of `string.h` functions
    - [x] Use `<thread>`, instead of `pthread.h`
    - [x] Use `<filesystem>` instead of `dirent.h` and `unistd.h` for file and folder operations
    - [x] Use `<regex>` instead of POSIX `regex.h`
    - [x] Use `<chrono>` for time instead of `time.h`
    - [ ] Implement self made error checks instead of `errno.h`
    - [ ] Implement self made debugging checks instead of `assert.h`
2. [ ] Extend the file descriptor "server_fd". _fd = "file descriptor".
    - After acceptance, each client should have their own "client_fd".
    - Eventually a "log_fd" variable to be implemented as well.
    - A class for this could possibly be of use.
3. [ ] Rename variables, functions, structs for clearer description of functionalities.
4. 