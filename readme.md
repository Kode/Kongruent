Kongruent is a language and compiler for data-parallel programming aka shaders. It compiles Kongruent code to its own bytecode and can then transform it into HLSL, GLSL, MSL (for Metal), WGSL (for WebGPU) and SPIR-V.

It is Robert's successor project to https://github.com/Kode/krafix which includes earlier work by Robert and Bill who later contributed HLSL and Metal support to SPIRV-Cross.

The project is very self-contained. The compiler is written from scratch, it uses a new shader bytecode and the only lib in use is stb_ds for some hash maps (and dxc when running on Windows, to compile HLSL to bytecode).

Kongruent integrates deeply with Kore 3 (see https://github.com/Kode/Kore/tree/v3) and a bunch of samples can be found at https://github.com/Kode/Kore-Samples/tree/v3.

Some concepts differ from other shader languages. The compiler is pointed at all the shader files at once (by adding one or more directories using the -i parameter) and treats them like one big program with several entry points instead of treating it like completely separate programs. Every file can see the global variables and functions from all the other files (no include directives needed) and pipelines are defined in the language itself. The compiler uses this to do cross-shader optimizations and to make data-binding automatic – there are no binding indices or anything like that in the language, instead the compiler writes all the binding code and some code to initialize resources with the correct options (see https://github.com/Kode/Kongruent/tree/main/sources/integrations) which are inferred from resource usage within the shaders instead of manual attributes.

But… all of this is in a prototypical state. A lot of things are incomplete, output needs some optimization-passes, there aren't any options for adding shaders at runtime yet, there is little error handling, …
