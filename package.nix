{ lib
, stdenv
, fetchFromGitHub
, cmake
, pkg-config
, openssl
, curl
, json_c
}:

stdenv.mkDerivation rec {
  pname = "azure-keyvault-engine";
  version = "1.0.0";

  src = ./.;

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    openssl
    curl
    json_c
  ];

  # Change to src directory before building
  preConfigure = ''
    cd src
  '';

  # Configure CMake to use Nix-provided libraries
  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DOPENSSL_API_COMPAT=0x10100000L"
  ];

  # Override the hardcoded library search to use Nix paths
  postPatch = ''
    # Replace hardcoded library names with pkg-config based discovery
    substituteInPlace src/CMakeLists.txt \
      --replace 'find_library(SSL_LIB libssl.so REQUIRED)' 'find_package(OpenSSL REQUIRED)' \
      --replace 'find_library(CRYPTO_LIB libcrypto.so REQUIRED)' '# CRYPTO_LIB handled by OpenSSL package' \
      --replace 'find_library(CURL_LIB libcurl.so REQUIRED)' 'find_package(CURL REQUIRED)' \
      --replace 'find_library(JSONC_LIB libjson-c.so REQUIRED)' 'find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONC REQUIRED json-c)' \
      --replace 'target_link_libraries(eakv
  ''${CRYPTO_LIB}
  ''${SSL_LIB}
  ''${CURL_LIB}
  ''${JSONC_LIB}
  )' 'target_link_libraries(eakv
  OpenSSL::SSL
  OpenSSL::Crypto
  ''${CURL_LIBRARIES}
  ''${JSONC_LIBRARIES}
  )
target_include_directories(eakv PRIVATE
  ''${JSONC_INCLUDE_DIRS}
  )'
  '';

  # Install to the correct OpenSSL engines directory
  installPhase = ''
    runHook preInstall
    
    mkdir -p $out/lib/engines-${lib.versions.major openssl.version}
    cp e_akv.so $out/lib/engines-${lib.versions.major openssl.version}/
    
    # Also install to a standard location for easier access
    mkdir -p $out/lib/openssl-engines
    cp e_akv.so $out/lib/openssl-engines/
    
    runHook postInstall
  '';

  # Add engine directory to OpenSSL's search path via wrapper
  postInstall = ''
    mkdir -p $out/bin
    cat > $out/bin/openssl-with-akv << 'EOF'
#!/bin/bash
export OPENSSL_ENGINES="$out/lib/engines-${lib.versions.major openssl.version}:$out/lib/openssl-engines''${OPENSSL_ENGINES:+:$OPENSSL_ENGINES}"
exec ${openssl}/bin/openssl "$@"
EOF
    chmod +x $out/bin/openssl-with-akv
  '';

  meta = with lib; {
    description = "Azure Key Vault and Managed HSM Engine for OpenSSL";
    longDescription = ''
      An OpenSSL engine that allows applications to use RSA/EC private keys 
      protected by Azure Key Vault and Managed HSM. The engine leverages the 
      OpenSSL engine interface to perform cryptographic operations inside 
      Azure Key Vault and Managed HSM.
    '';
    homepage = "https://github.com/microsoft/AzureKeyVaultManagedHSMEngine";
    license = licenses.mit;
    maintainers = [ ];
    platforms = [ "aarch64-linux" "x86_64-linux" ];
  };
}