# Let's Play
A web-based collaborative RetroArch frontend

![screenshot](https://raw.githubusercontent.com/ctrlaltf2/lets-play-server/master/screenshot.png)

# What is a "web-based collaborative RetroArch frontend"?
Why thanks for asking, a A web-based collaborative RetroArch frontend is just a fancy phrase for a video game you can play online, collaboratively, with strangers by taking turns at the controller (so to speak). Users connect using [the web portion](https://github.com/ctrlaltf2/lets-play-client) which connects to this, the backend, and are then able to interact with an emulator that (usually) runs a retro video game system of some sort.

# Requirements
 - Compiler with support for C++17
 - websocketpp
 - Boost
    - Program Options
    - UUID
    - Asio (for websocketpp)
    - Filesystem
    - DLL
    - Function
 - turbojpeg

# Building
To build, simply type `cmake .` in the top level directory, then type `make`. To do parallel builds (recommended), type `make -j#` where `#` is the number of cores you have on your machine. After the build, the binary will be in `./bin/` as `letsplay`. If your compiler supports the STL filesystem fully and you'd like to use it, add `-DUseSTLFilesystem=ON`.

# Note
The default admin password is `LetsPlay`. **This should be changed for your own security**. Change the password by directly modifying the config. The values that should be modified are `config["serverConfig"]["salt"]` and `config["serverConfig"]["adminHash"]`. The hash should be generated by taking your password, appending the salt value, and md5 hashing it.
