{
  description = "ngscopeclient";
  nixConfig.bash-prompt = "[nix(ngscopeclient)] ";

  # Nixpkgs / NixOS version to use.
  inputs.nixpkgs.url = "nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  outputs = { nixpkgs, ... }:
    let
      # System types to support.
      supportedSystems =
        [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];

      # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      # Nixpkgs instantiated for supported system types.
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; });
    in rec {
      # Provide some binary packages for selected system types.
      packages = forAllSystems (system:
        let
          pkgs = nixpkgsFor.${system};
          inherit (pkgs) callPackage;
          outPathOf = x: if (builtins.hasAttr "lib" x) 
                         then x.lib.outPath
                         else x.out.outPath;
          libAppend = x: ''
              export LD_LIBRARY_PATH="${outPathOf x}/lib:$LD_LIBRARY_PATH"
              '';
        in rec {
          ffts = callPackage ./nix/ffts.nix { };
          scopehal-apps = callPackage ./nix/scopehal-apps.nix { inherit ffts; };
          default = scopehal-apps;
          libraryPaths = builtins.concatStringsSep "" (map libAppend scopehal-apps.buildInputs);
      });

      devShell = forAllSystems (system:
        let
          pkgs = nixpkgsFor.${system};
          scopeHal = packages.${system}.scopehal-apps;
        in pkgs.mkShell {
          nativeBuildInputs = scopeHal.nativeBuildInputs;
          buildInputs = scopeHal.buildInputs;
          hardeningDisable = [ "all" ];
          shellHook = "echo Welcome to the ngscopeclient devShell!\n" + 
                      packages.${system}.libraryPaths;
        }
      );
    };
}
