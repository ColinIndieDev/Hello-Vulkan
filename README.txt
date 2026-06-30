C Projects + Custom NeoVim Controls:

Building Options:

F3 -> Release Build
F4 -> Debug Build
F5 -> Dev Build

F6 -> Building Project to executable
F7 -> Run executable
F8 -> Run executable with perf profiling

Custom controls:

r -> gives info about function, variables etc.-fno-omit-frame-pointer

Perf profiling:

Normal mode:

plg -> generate graph (after recording using F8)
pt -> toggle display hotlines in code (annotate)
ph -> print list with hottest lines

ASAN Suppressions:

Theoretically only needed if using OpenGL or CPL to suppress errors caused by third party libraries (f.e. GLFW & GLAD)
