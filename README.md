Steps for compiling
-------------------

Fetch imgui-sfml (added as a submodule to this project):

1. `git submodule init`
2. `git submodule update`

imgui-sfml assumes that `find_package(SFML ...)` is able to find SFML.  Since
SFML installs `FindSML.cmake` to the wrong location on Linux systems (? and
possibly other platforms), it probably isn't able to do this. Unless fixed
upstream, you therefore have to symlink from the correct location to
`FindSML.cmake`:

3. `ln -s /usr/share/SFML/cmake/Modules/FindSML.cmake /usr/share/cmake-[version number]/Modules`

Then build by

4. `mkdir build`
5. `cd build`
6. `cmake ..`
7. `make`
