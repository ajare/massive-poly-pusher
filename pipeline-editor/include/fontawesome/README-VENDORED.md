# Vendored: Font Awesome 5 Free (icon codepoint header + solid font)

Copied per the project convention of embedding third-party sources directly
(see `cpp/editor/include/imgui/README-VENDORED.md`) rather than a
submodule/FetchContent.

- `IconsFontAwesome5.h` — codepoint constants (`ICON_FA_*`) generated from
  Font Awesome 5 Free's icon metadata by the
  [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders)
  project. MIT licensed.
- `../../resources/fa-solid-900.ttf` — the Font Awesome 5 Free "solid" style
  webfont loaded by PipelineEditor (`FONT_ICON_FILE_NAME_FAS`).
- `../../resources/fa-regular-400.ttf` and `fa-brands-400.ttf` — matching
  upstream Free font files retained with the vendored snapshot; they are not
  loaded by the current toolbar.
  Font Awesome Free's fonts are licensed under the
  [SIL Open Font License 1.1](https://scripts.sil.org/OFL); see
  https://fontawesome.com/license/free.

The solid font is loaded by `Main.cpp` through the shared `imGuiSetup()`
helper and merged into ImGui's default font atlas using the standard
`ImFontConfig::MergeMode` pattern (icon glyphs share the default
font's baseline/line height so `ICON_FA_*` strings can be mixed into normal
button/text labels).

Do not hand-edit `IconsFontAwesome5.h`; re-copy from upstream to update.
