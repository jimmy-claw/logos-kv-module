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
        let
          src = ./.;

          # Standalone cmake build — does not depend on logos-module-builder
          # for the actual .so output. Builds with LOGOS_CORE_AVAILABLE=OFF
          # so it works without logos-liblogos/logos-cpp-sdk at build time.
          standalone = pkgs.stdenv.mkDerivation {
            pname = "kv_module_plugin";
            version = "0.2.0";

            inherit src;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.qt6.wrapQtAppsHook
            ];

            buildInputs = [
              pkgs.qt6.qtbase
              pkgs.qt6.qtdeclarative
              pkgs.openssl
            ];

            cmakeFlags = [
              "-GNinja"
              "-DCMAKE_BUILD_TYPE=Release"
            ];

            dontWrapQtApps = true;

            installPhase = ''
              runHook preInstall
              mkdir -p $out/lib
              find . -name "kv_module_plugin.so" -exec cp {} $out/lib/ \;
              find . -name "kv_module_plugin.dylib" -exec cp {} $out/lib/ \;
              if [ ! -f $out/lib/kv_module_plugin.so ] && [ ! -f $out/lib/kv_module_plugin.dylib ]; then
                echo "ERROR: kv_module_plugin shared library not found in build output"
                find . -name "*.so" -o -name "*.dylib" | head -10
                exit 1
              fi
              runHook postInstall
            '';
          };
        in
        (moduleOutputs.packages.${pkgs.system} or {}) // {
          inherit standalone;
          test = import ./nix/test.nix { inherit pkgs src; };
        }
      );
    };
}
