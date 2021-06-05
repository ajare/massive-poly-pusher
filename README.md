# MassivePolyPusher

An OpenGL 2d and 3d renderer.

## Cloning

Run `git clone --recurse-submodules -j8 https://bitbucket.org/wtmrsh/massivepolypusher.git`.  You will need git version 2.13 for `--recurse-submodules` and 2.8+ for `-j8`.

## Building

First build the submodules:

```cd ext\utils
msbuild build\vs2017\Utils.sln -target:UtilsTests:Rebuild
```
