{
  description = "Local key-value storage module for Logos Core";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder/0.2.0";
    nixpkgs.follows = "logos-module-builder/nixpkgs";
  };

  outputs = { self, logos-module-builder, nixpkgs, ... }:
    let
      moduleOutputs = logos-module-builder.lib.mkLogosModule {
        src = ./.;
        flakeInputs = inputs;
      };
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    moduleOutputs // {
      packages = forAllSystems ({ pkgs }:
        moduleOutputs.packages.${pkgs.system} or {}
      );
    };
}
