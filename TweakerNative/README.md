# TweakerNative

Windows x64 C++23 host library for QuickPlayerCore. Independent of
TweakerPlugin, ASBridge and InjectHelper. Its C ABI reads an owned metadata snapshot
and writes only the title of a file supplied by the caller. QuickPlayerCore invokes
the writer on staging copies in its existing background preparation tasks.

`metadata.hxx` defines the ABI. `NativeAudioMetadata` uses source-generated
LibraryImport declarations and frees snapshots through SafeHandle. File handles
are closed before either native operation returns. Exceptions never cross the ABI.

Build through `dotnet build QuickPlayerCore/QuickPlayerCore.csproj` or the usual UI
build/deploy commands. `TweakerNative.targets` configures CMake for x64, builds Release,
and adds the DLL and licenses to transitive output/publish assets. Managed tests use
the same DLL. Requires MSVC, Windows SDK and CMake 3.24+ (use a CMake version supporting
your Visual Studio generator).

First configure downloads SHA-256-pinned official release archives:

- TagLib 2.3.2, including its bundled utfcpp 4.2.0;
- zlib 1.4, retaining compressed ID3 support.

TagLib, zlib and the MSVC runtime are statically linked; no separate TagLib/zlib/VC
runtime DLL is required. Dependencies remain unmodified in `build/msbuild/_deps`.
For an offline build, populate that CMake cache first or use CMake's standard
`FETCHCONTENT_SOURCE_DIR_TAGLIB` / `FETCHCONTENT_SOURCE_DIR_ZLIB` overrides when
configuring the same build directory. Licenses are copied to `Licenses/` with the DLL.

Verification: `QuickPlayerCore.Tests` exercises all four supported formats with
generated fixtures, Unicode metadata/paths, failed reads and staging-file lifetime.
See `Docs/Internal/native-aot-audit.md` for migration and independent decoder checks.
