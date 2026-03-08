# Builds the kv_module with all backends and runs conformance tests
{ pkgs, src }:

pkgs.stdenv.mkDerivation {
  pname = "kv-module-test";
  version = "0.1.0";

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
    pkgs.gtest
    pkgs.rocksdb
    pkgs.sqlite
  ];

  cmakeFlags = [
    "-GNinja"
    "-DBUILD_TESTS=ON"
    "-DWITH_ROCKSDB=ON"
    "-DWITH_SQLITE=ON"
  ];

  dontWrapQtApps = true;

  doCheck = true;

  checkPhase = ''
    runHook preCheck
    QT_QPA_PLATFORM=offscreen ctest --output-on-failure
    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    touch $out/tests-passed
    runHook postInstall
  '';
}
