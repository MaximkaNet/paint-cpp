# Paint in linux terminal
Works on linux only!

### Requirements
- Linux
- CMake (build system)
- Clang (c++ compiler)

### To run application:
1. Configure the project
    - In root folder of this project make directory, for example: `build`. Run this command `mkdir build`.
    - Next run `cd build && cmake ../`. You will see configuration logs and after that `-- Configuring done (...)`
2. Build the project
    - Run build command `cmake --build .`.
3. Run executable
    - You can run executable program in `build` folder. To run it: `./canvas_app`

**Congrats! You run Paint in linux terminal.**