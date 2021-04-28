# Let's Play

## As of May 1st, 2021 this is being rewritten, see the v2.0 branch

[![Discord](https://discordapp.com/api/guilds/572941118065606667/widget.png)](https://discord.gg/GNChjnn)

A web-based collaborative RetroArch frontend

![Screenshot](https://raw.githubusercontent.com/ctrlaltf2/lets-play-server/master/screenshot.png)

# What is a "web-based collaborative RetroArch frontend"?
Why thanks for asking, a A web-based collaborative RetroArch frontend is just a fancy phrase for a video game you can play online, collaboratively, with friends by taking turns at the controller (so to speak). Users connect using [the web portion](https://github.com/ctrlaltf2/lets-play-client) which connects to this, the backend, and are then able to interact with an emulator that (usually) runs a retro video game system of some sort.

# Requirements
 - Compiler with support for C++14
 - CMake version >= 3.2
 - CPU with support for SSE2 and SSSE3 (most CPUs do)

# Building
To build, simply type `cmake .` in the top level directory, then type `make`. To do parallel builds (recommended), type `make -j#` where `#` is the number of cores you have on your machine. After the build, the binary will be in `./bin/` as `letsplay`.

# Where do I put the extra files (bios, etc) that would normally go in the frontend's system directory?
The RetroArch cores will be instructed to look in the `system` folder inside of the data directory. The location of the data directory varies based on what is specified in the configuration. If `dataDirectory` is equal to `System Default`, Let's Play will first go to `XDG_DATA_HOME/letsplay` as the data dir, and if XDG envs are undefined, `~/.local/share/letsplay`.

# Note
The default admin password is `LetsPlay`. **This should be changed for your own security**. Change the password by directly modifying the config. The values that should be modified are `config["serverConfig"]["salt"]` and `config["serverConfig"]["adminHash"]`. The hash should be generated by taking your password, appending the salt value, and md5 hashing it.
