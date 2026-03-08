{
  description = "Local key-value storage module for Logos Core";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    nixpkgs.follows = "logos-module-builder/nixpkgs";
  };

  outputs = { self, logos-module-builder, nixpkgs, ... }:
    let
      moduleOutputs = logos-module-builder.lib.mkLogosModule {
        src = ./.;
        configFile = ./module.yaml;
      };
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    moduleOutputs // {
      packages = forAllSystems ({ pkgs }:
        let src = ./.; in
        (moduleOutputs.packages.${pkgs.system} or {}) // {
          test = import ./nix/test.nix { inherit pkgs src; };
        }
      );
    };
}
