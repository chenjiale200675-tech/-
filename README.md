# CodexMarksPlugin cloud build

This package builds a CorelDRAW 2022 x64 CPG plugin.

The repository must remain private because `src/VGCoreAuto.tlb` comes from the local CorelDRAW installation and is only used to generate COM bindings during compilation.

After GitHub Actions finishes, download `CodexMarksPlugin.cpg` and place it in:

`E:\CorelDRAW Graphics Suite 2022\Draw\Plugins64\CodexMarksPlugin.cpg`

Restart CorelDRAW after installation.
