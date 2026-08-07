# MassivePolyPusher

An OpenGL 2d and 3d renderer.

## Cloning

Run `git clone --recurse-submodules -j8 https://bitbucket.org/wtmrsh/massivepolypusher.git`.  You will need git version 2.13 for `--recurse-submodules` and 2.8+ for `-j8`.

## Building

First build the submodules:

`msbuild ext\utils\build\vs2017\Utils.sln -target:UtilsTests:Rebuild -p:Platform=Win32 -p:Configuration=Release`

Then the main project. The current editor/tool configuration is VS2026 x64:

`msbuild build\vs2026\MassivePolyPusher.sln -target:Build -p:Platform=x64 -p:Configuration=Release`

`PipelineEditor` is a separate executable under `pipeline-editor\build\vs2026\bin\x64\<Configuration>`. Its post-build deployment copies `editor.ini`, which references the repository-level `resources` directory beside the root `build` directory.

## PBR PipelineEditor

Start with `resources/shared/pbr/templates/Minimal.pipeline.xml`, `Shadows.pipeline.xml`, `Full.pipeline.xml`, or `Empty.pipeline.xml`. The reusable default scene is `resources/shared/pbr/DefaultPbrPreview.scene.xml`. PipelineEditor loads the repository-level `resources` tree through the deployed `editor.ini`; resources are not copied into its binary output directory.

Documentation:

- [PipelineEditor authoring guide](doc/PIPELINE_EDITOR_AUTHORING_GUIDE.md)
- [PBR pipeline XML specification](doc/PBR_PIPELINE_XML_SPECIFICATION.md)
- [Preview scene XML specification](doc/PBR_SCENE_XML_SPECIFICATION.md)
- [Diagnostics catalogue](doc/PIPELINE_EDITOR_DIAGNOSTICS.md)
- [CLI validation and smoke tests](doc/PIPELINE_EDITOR_CLI.md)
