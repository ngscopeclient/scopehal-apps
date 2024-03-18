{ stdenv, breakpointHook, fetchFromGitHub, makeWrapper,
  cmake, git, lib, pkg-config,
  libsigcxx, gtkmm3, cairomm, yaml-cpp, catch2, glfw, libtirpc, liblxi,
  glew, libllvm, libdrm, elfutils, libxcb, zstd, libxshmfence, xcbutilkeysyms,
  systemd,
  shaderc, vulkan-headers, vulkan-loader, vulkan-tools, glslang, spirv-tools,
  ffts,
  ... }:
stdenv.mkDerivation rec {
  pname = "scopehal-apps";
  version = "b589830";

  src = fetchFromGitHub {
    owner = "ngscopeclient";
    repo = "scopehal-apps";
    rev = "b58983056b0b6a628c97e6fbf57be3cc010717d4";
    hash = "sha256-PYl5qx6dGBfsDOGuQP7kPEh71skR3HPpX7YCVa+uhiY=";
    fetchSubmodules = true;
  };

  nativeBuildInputs = [ cmake git makeWrapper ];
  buildInputs = [
    pkg-config
    libsigcxx
    gtkmm3
    cairomm
    yaml-cpp
    catch2
    glfw
    libtirpc
    liblxi
    glew
    libllvm
    libdrm
    elfutils
    libxcb
    zstd
    libxshmfence
    xcbutilkeysyms
    systemd
    vulkan-headers
    vulkan-loader
    vulkan-tools
    spirv-tools
    glslang
    shaderc
    ffts
 ];

  libraryPaths =
    let
      outPathOf = x: if (builtins.hasAttr "lib" x)
                      then x.lib.outPath
                      else x.out.outPath;
      libAppend = x: "${outPathOf x}/lib";
    in
      builtins.concatStringsSep ":" (map libAppend buildInputs);

  cmakeFlags = [
    "-DCURRENT_GIT_VERSION=${lib.substring 0 7 src.rev}"
    "-Wno-deprecated"
  ];

  postInstall = ''
    ln -s $out/share $out/bin/
    mv -v $out/bin/ngscopeclient $out/bin/ngscopeclient-unwrapped
    makeWrapper $out/bin/ngscopeclient-unwrapped $out/bin/ngscopeclient --set LD_LIBRARY_PATH="${libraryPaths}"
  '';

  meta = with lib; {
    homepage = "https://www.ngscopeclient.org/";
    license = licenses.bsd3;
    maintainers = with lib.maintainers; [ thoughtpolice hansfbaier ];
    platforms = lib.platforms.linux;
  };
}
