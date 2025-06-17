{
  description = "Azure Key Vault and Managed HSM Engine for OpenSSL";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachSystem [ "aarch64-linux" "x86_64-linux" ] (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages.default = pkgs.callPackage ./default.nix { };

        packages.azure-keyvault-engine = self.packages.${system}.default;

        # Development shell with all dependencies
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            cmake
            pkg-config
            openssl
            curl
            json_c
            # Development tools
            gdb
            valgrind
            # Azure CLI for testing
            azure-cli
          ];

          shellHook = ''
            echo "Azure Key Vault Engine Development Environment"
            echo "Available commands:"
            echo "  cmake, make     - Build tools"
            echo "  openssl         - OpenSSL CLI"
            echo "  az              - Azure CLI"
            echo "  gdb, valgrind   - Debugging tools"
            echo ""
            echo "To build:"
            echo "  cd src && mkdir -p build && cd build"
            echo "  cmake .. && make"
            echo ""
            echo "To test engine loading:"
            echo "  openssl engine -vvv -t e_akv"
          '';
        };

        # Additional development shell with testing dependencies
        devShells.testing = pkgs.mkShell {
          buildInputs = with pkgs; [
            cmake
            pkg-config
            openssl
            curl
            json_c
            azure-cli
            # Testing tools
            shellcheck
            # Certificate management tools
            cfssl
            # Additional debugging
            strace
            ltrace
          ];

          shellHook = ''
            echo "Azure Key Vault Engine Testing Environment"
            echo "Run './test_akv_engine.sh' to execute the test suite"
          '';
        };

        # Checks for CI/development
        checks = {
          build = self.packages.${system}.default;
          
          # Lint shell scripts
          shellcheck = pkgs.runCommand "shellcheck" {
            buildInputs = [ pkgs.shellcheck ];
          } ''
            shellcheck ${./test_akv_engine.sh}
            touch $out
          '';
        };

        # Apps for easy execution
        apps = {
          default = flake-utils.lib.mkApp {
            drv = pkgs.writeShellScriptBin "azure-keyvault-engine-info" ''
              echo "Azure Key Vault Engine for OpenSSL"
              echo "Engine file: ${self.packages.${system}.default}/lib/openssl-engines/e_akv.so"
              echo ""
              echo "To use with OpenSSL:"
              echo "  export OPENSSL_ENGINES=${self.packages.${system}.default}/lib/openssl-engines"
              echo "  openssl engine -vvv -t e_akv"
              echo ""
              echo "Or use the wrapper:"
              echo "  ${self.packages.${system}.default}/bin/openssl-with-akv engine -vvv -t e_akv"
            '';
          };

          test = flake-utils.lib.mkApp {
            drv = pkgs.writeShellScriptBin "test-azure-keyvault-engine" ''
              export OPENSSL_ENGINES=${self.packages.${system}.default}/lib/openssl-engines
              cd ${./.}
              ./test_akv_engine.sh engine
            '';
          };
        };
      });
}