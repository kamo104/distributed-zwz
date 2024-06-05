{
  description = "Nix flake for mpi with c/c++";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
  flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs {
        inherit system;
      };
    in         
      {
        devShell = with pkgs; mkShell rec {
          buildInputs =  [
            gdb
            mpi
            gnumake
            clang-tools
            bear
          ];
        };
    });
}

