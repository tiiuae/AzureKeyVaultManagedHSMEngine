{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build dependencies
    cmake
    pkg-config
    openssl
    curl
    json_c
    
    # Development tools
    gdb
    valgrind
    azure-cli
    
    # Utilities
    which
    file
  ];

  shellHook = ''
    export PROJECT_ROOT="$(pwd)"
    export PKG_CONFIG_PATH="${pkgs.openssl.dev}/lib/pkgconfig:${pkgs.curl.dev}/lib/pkgconfig:${pkgs.json_c}/lib/pkgconfig:$PKG_CONFIG_PATH"
    
    echo "=== Azure Key Vault Engine Development Environment ==="
    echo "OpenSSL version: $(${pkgs.openssl}/bin/openssl version)"
    echo "Available engines directory: $(${pkgs.openssl}/bin/openssl version -e)"
    echo ""
    echo "Quick start:"
    echo "  mkdir -p src/build && cd src/build"
    echo "  cmake .."
    echo "  make"
    echo "  ${pkgs.openssl}/bin/openssl engine -vvv -t ./e_akv.so"
    echo ""
    echo "Environment variables set:"
    echo "  PROJECT_ROOT=$PROJECT_ROOT"
    echo "  PKG_CONFIG_PATH (includes OpenSSL, curl, json-c)"
    echo ""
  '';
}