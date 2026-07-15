{
  description = "Local key-value storage module for Logos Core";

  inputs = {
    # Use master (not 0.2.0 tag) — 0.2.0 pins old API requiring configFile
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    nixpkgs.follows = "logos-module-builder/nixpkgs";
  };

  outputs = inputs@{ logos-module-builder, ... }:
    logos-module-builder.lib.mkLogosModule {
      src = ./.;
      flakeInputs = inputs;
    };
}
