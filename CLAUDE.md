# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the Azure Key Vault and Managed HSM Engine for OpenSSL - an open-source project that allows OpenSSL-based applications to use RSA/EC private keys protected by Azure Key Vault and Managed HSM. The engine implements the OpenSSL engine interface to perform cryptographic operations remotely in Azure services.

**Important**: Azure Key Vault should ONLY be used for development purposes with small numbers of requests. For production workloads, use Azure Managed HSM.

## Build Commands

### Linux/Ubuntu Build
```bash
# Install dependencies
sudo apt install -y build-essential libssl-dev libcurl4-openssl-dev libjson-c-dev

# Build the engine
cd src
mkdir build
cd build
cmake ..
make

# Install (requires OpenSSL version detection)
sudo mkdir -p /usr/lib/x86_64-linux-gnu/engines-1.1/
sudo cp e_akv.so /usr/lib/x86_64-linux-gnu/engines-1.1/e_akv.so
```

### Windows Build
```cmd
cd src
msbuild e_akv.vcxproj /p:PkgCurl="C:\vcpkg\packages\curl_x64-windows-static" /p:PkgJson="C:\vcpkg\packages\json-c_x64-windows-static" /p:PkgZ="C:\vcpkg\packages\zlib_x64-windows-static" /p:PkgOpenssl="C:\vcpkg\packages\openssl_x64-windows" /p:Configuration=Release;Platform=x64
```

## Testing

### Basic Engine Test
```bash
openssl engine -vvv -t e_akv
```

### Comprehensive Test Suite
```bash
# Run all tests
./test_akv_engine.sh

# Run specific tests
./test_akv_engine.sh engine    # Engine loading only
./test_akv_engine.sh cert      # Certificate generation
./test_akv_engine.sh sign      # File signing
./test_akv_engine.sh sbsign    # sbsign style signing
./test_akv_engine.sh clean     # Cleanup test files
```

## Architecture

### Core Components

- **Engine Interface** (`ctrl.c`, `dllmain.c`): OpenSSL engine registration and control
- **Key Management** (`key.c`): Azure Key Vault key acquisition and management
- **Cryptographic Operations**: 
  - `rsa.c`: RSA signing, verification, encryption, decryption
  - `ec.c`: Elliptic Curve signing and verification
- **HTTP Client** (`curl.c`): Azure REST API communication
- **Utilities**: `base64.c` (encoding), `e_akv_err.c` (error handling)

### OpenSSL Compatibility

The engine supports both OpenSSL 1.1.x and 3.0+ through:
- `openssl3_compat.h`: Compatibility macros and function mappings
- `OPENSSL_API_COMPAT=0x10100000L`: API compatibility level
- Dynamic installation path detection based on OpenSSL version

### Key Format

Keys are specified as: `{type}:{vault_name}:{key_name}`
- `type`: "vault" (Key Vault) or "managedHsm" (Managed HSM)
- `vault_name`: Name of the Azure Key Vault or Managed HSM
- `key_name`: Name of the key within the vault

### Authentication

Supports Azure CLI authentication via `AZURE_CLI_ACCESS_TOKEN` environment variable. The test script automatically handles token acquisition.

## Dependencies

- **OpenSSL**: 1.1.x or 3.0+ (with compatibility layer)
- **libcurl**: HTTP client for Azure REST API calls
- **json-c**: JSON parsing for Azure API responses
- **CMake**: Build system (Linux)
- **Visual Studio/MSBuild**: Build system (Windows)
- **vcpkg**: Package manager (Windows)

## CI/CD

The project uses Azure Pipelines (`azure-pipelines.yml`) with:
- Linux builds on Ubuntu with CMake
- Windows builds with MSBuild and vcpkg
- Automated testing with both RSA and ECC keys
- Artifact publishing for both platforms