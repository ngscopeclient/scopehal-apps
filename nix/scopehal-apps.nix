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
    rev = "33c7cddc344f2a7a337b8a3987e16e29f5dd4d0b";
    hash = "sha256-kingCorT42tacCrnNEjRRjRREm9g+ju1Zab4xHBFmiI=";
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
