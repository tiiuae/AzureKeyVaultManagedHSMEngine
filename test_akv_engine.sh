	#!/bin/bash

	# Azure Key Vault OpenSSL Engine Test Suite  
	# Tests: 1) Engine loading 2) Certificate generation 3) File signing 4) sbsign style signing

	set -e  # Exit on any error

	# Colors for output
	RED='\033[0;31m'
	GREEN='\033[0;32m'
	YELLOW='\033[1;33m'
	BLUE='\033[0;34m'
	NC='\033[0m' # No Color

	# Test configuration
	KEY_VAULT_TYPE="vault"
	KEY_VAULT_NAME="ghaf-secureboot-testkv"
	KEY_NAME="uefi-signing-key"
	KEY_ID="${KEY_VAULT_TYPE}:${KEY_VAULT_NAME}:${KEY_NAME}"

	# Test files
	TEST_FILE="test_data.txt"
	TEST_BINARY="test.efi"
	CERT_FILE="uefi-signing-cert.pem"
	SIG_FILE="test_data.sig"
	SIGNED_BINARY="test_signed.efi"

	# Function to print colored output
	print_status() {
	    local status=$1
	    local message=$2
	    case $status in
		"INFO")
		    echo -e "${BLUE}[INFO]${NC} $message"
		    ;;
		"SUCCESS")
		    echo -e "${GREEN}[SUCCESS]${NC} $message"
		    ;;
		"ERROR")
		    echo -e "${RED}[ERROR]${NC} $message"
		    ;;
		"WARNING")
		    echo -e "${YELLOW}[WARNING]${NC} $message"
		    ;;
	    esac
	}

	# Function to setup Azure access token
	setup_azure_token() {
	    print_status "INFO" "Setting up Azure CLI access token..."
	    
	    if ! command -v az &> /dev/null; then
		print_status "ERROR" "Azure CLI not found. Please install Azure CLI."
		exit 1
	    fi
	    
	    USER_TOKEN=$(az account get-access-token --resource https://vault.azure.net --query accessToken -o tsv)
	    if [ $? -ne 0 ]; then
		print_status "ERROR" "Failed to get Azure access token. Please ensure you're logged in with 'az login'."
		exit 1
	    fi
	    
	    export AZURE_CLI_ACCESS_TOKEN="$USER_TOKEN"
	    print_status "SUCCESS" "Azure access token set successfully"
	}

	# Function to cleanup test files
	cleanup_test_files() {
	    print_status "INFO" "Cleaning up test files..."
	    rm -f "$TEST_FILE"  "$CERT_FILE" "$SIG_FILE" "$SIGNED_BINARY"
	    rm -f core core.*  # Remove core dumps
	}

	# Function to test engine loading
	test_engine_loading() {
	    print_status "INFO" "Testing Azure Key Vault engine loading..."
	    
	    local output
	    output=$(openssl engine -t e_akv 2>&1)
	    local exit_code=$?
	    
	    echo "Output: $output"
	    
	    if [ $exit_code -eq 0 ] && echo "$output" | grep -q "e_akv"; then
		print_status "SUCCESS" "Engine loading test passed"
		return 0
	    else
		print_status "ERROR" "Engine loading test failed (exit code: $exit_code)"
		print_status "ERROR" "Output: $output"
		return 1
	    fi
	}

	# Function to test certificate generation
	test_certificate_generation() {
	    print_status "INFO" "Testing certificate generation..."
	    
	    local output
	    output=$(openssl req \
		-new \
		-x509 \
		-days 365 \
		-engine e_akv \
		-keyform engine \
		-key "$KEY_ID" \
		-out "$CERT_FILE" \
		-subj "/C=FI/O=GHAF/CN=UEFI Signing Certificate" \
		-config /dev/null 2>&1)
	    local exit_code=$?
	    
	    echo "Output: $output"
	    
	    if [ $exit_code -eq 0 ] && [ -f "$CERT_FILE" ]; then
		print_status "SUCCESS" "Certificate generation test passed"
		
		# Verify certificate
		openssl x509 -in "$CERT_FILE" -text -noout | head -10
		return 0
	    else
		print_status "ERROR" "Certificate generation test failed (exit code: $exit_code)"
		print_status "ERROR" "Output: $output"
		return 1
	    fi
	}

	# Function to test file signing
	test_file_signing() {
	    print_status "INFO" "Testing file signing..."
	    
	    # Create test file
	    echo "This is a test file for Azure Key Vault signing" > "$TEST_FILE"
	    
	    local output
	    output=$(openssl dgst \
		-sha256 \
		-engine e_akv \
		-keyform engine \
		-sign "$KEY_ID" \
		-out "$SIG_FILE" \
		"$TEST_FILE" 2>&1)
	    local exit_code=$?
	    
	    echo "Output: $output"
	    
	    if [ $exit_code -eq 0 ] && [ -f "$SIG_FILE" ]; then
		print_status "SUCCESS" "File signing test passed"
		
		# Display signature info
		echo "Signature file size: $(wc -c < "$SIG_FILE") bytes"
		return 0
	    else
		print_status "ERROR" "File signing test failed (exit code: $exit_code)"
		print_status "ERROR" "Output: $output"
		return 1
	    fi
	}

	# Function to test sbsign style signing
	test_sbsign_signing() {
	    print_status "INFO" "Testing sbsign style signing..."
	    
	    # Check if sbsign is available
	    if ! command -v sbsign &> /dev/null; then
		print_status "WARNING" "sbsign not found, skipping sbsign test"
		return 0
	    fi
	    
	    # Create a minimal EFI binary (dummy data)
	    # Note: This creates a file that sbsign can process, though it's not a real EFI binary

	    
	    # Check if certificate exists for sbsign
	    if [ ! -f "$CERT_FILE" ]; then
		print_status "WARNING" "Certificate file not found, skipping sbsign test"
		return 0
	    fi
	    
	    local output
	    # Note: sbsign doesn't support provider interface directly
	    # This test may fail until sbsign is updated for OpenSSL 3.0 providers

	    output=$(OPENSSL_CONF=/dev/null ../sbsigntools/src/sbsign \
		--engine e_akv \
		--keyform engine \
		--key "$KEY_ID" \
		--cert "$CERT_FILE" \
		--output "$SIGNED_BINARY" \
		"$TEST_BINARY" 2>&1)
	    local exit_code=$?
	    
	    echo "Output: $output"
	    
	    if [ $exit_code -eq 0 ] && [ -f "$SIGNED_BINARY" ]; then
		print_status "SUCCESS" "sbsign style signing test passed"
		
		# Display signed binary info
		echo "Signed binary size: $(wc -c < "$SIGNED_BINARY") bytes"
		return 0
	    else
		print_status "ERROR" "sbsign style signing test failed (exit code: $exit_code)"
		print_status "ERROR" "Output: $output"
		return 1
	    fi
	}

	# Function to run comprehensive tests
	run_all_tests() {
	    local test_results=()
	    local total_tests=0
	    local passed_tests=0
	    
	    print_status "INFO" "Starting Azure Key Vault OpenSSL Engine Test Suite"
	    print_status "INFO" "Key ID: $KEY_ID"
	    print_status "INFO" "OpenSSL Version: $(openssl version)"
	    echo
	    
	    # Test 1: Engine Loading
	    echo "=================================================="
	    echo "Test 1: Engine Loading"
	    echo "=================================================="
	    total_tests=$((total_tests + 1))
	    if test_engine_loading; then
		test_results+=("Engine Loading: PASSED")
		passed_tests=$((passed_tests + 1))
	    else
		test_results+=("Engine Loading: FAILED")
	    fi
	    echo
	    
	    # Test 2: Certificate Generation
	    echo "=================================================="
	    echo "Test 2: Certificate Generation"
	    echo "=================================================="
	    total_tests=$((total_tests + 1))
	    if test_certificate_generation; then
		test_results+=("Certificate Generation: PASSED")
		passed_tests=$((passed_tests + 1))
	    else
		test_results+=("Certificate Generation: FAILED")
	    fi
	    echo
	    
	    # Test 3: File Signing
	    echo "=================================================="
	    echo "Test 3: File Signing"
	    echo "=================================================="
	    total_tests=$((total_tests + 1))
	    if test_file_signing; then
		test_results+=("File Signing: PASSED")
		passed_tests=$((passed_tests + 1))
	    else
		test_results+=("File Signing: FAILED")
	    fi
	    echo
	    
	    # Test 4: sbsign Style Signing
	    echo "=================================================="
	    echo "Test 4: sbsign Style Signing"
	    echo "=================================================="
	    total_tests=$((total_tests + 1))
	    if test_sbsign_signing; then
		test_results+=("sbsign Signing: PASSED")
		passed_tests=$((passed_tests + 1))
	    else
		test_results+=("sbsign Signing: FAILED")
	    fi
	    echo
	    
	    # Print summary
	    echo "=================================================="
	    echo "Test Summary"
	    echo "=================================================="
	    for result in "${test_results[@]}"; do
		if echo "$result" | grep -q "PASSED"; then
		    print_status "SUCCESS" "$result"
		else
		    print_status "ERROR" "$result"
		fi
	    done
	    echo
	    print_status "INFO" "Total Tests: $total_tests"
	    print_status "INFO" "Passed: $passed_tests"
	    print_status "INFO" "Failed: $((total_tests - passed_tests))"
	    
	    if [ $passed_tests -eq $total_tests ]; then
		print_status "SUCCESS" "All tests passed!"
		return 0
	    else
		print_status "ERROR" "Some tests failed!"
		return 1
	    fi
	}

	# Main execution
	main() {
	    # Handle command line arguments
	    case "${1:-all}" in
		"engine"|"1")
		    test_engine_loading
		    ;;
		"cert"|"2")
		    setup_azure_token
		    test_certificate_generation
		    ;;
		"sign"|"3")
		    setup_azure_token
		    test_file_signing
		    ;;
		"sbsign"|"4")
		    setup_azure_token
		    test_sbsign_signing
		    ;;
		"all"|"")
		    setup_azure_token
		    run_all_tests
		    ;;
		"clean")
		    cleanup_test_files
		    print_status "SUCCESS" "Cleanup completed"
		    ;;
		"help"|"-h"|"--help")
		    echo "Usage: $0 [test_type]"
		    echo
		    echo "Test types:"
		    echo "  engine, 1    - Test engine loading only"
		    echo "  cert, 2      - Test certificate generation only"
		    echo "  sign, 3      - Test file signing only"
		    echo "  sbsign, 4    - Test sbsign style signing only"
		    echo "  all          - Run all tests (default)"
		    echo "  clean        - Clean up test files"
		    echo "  help         - Show this help"
		    echo
		    echo "Examples:"
		    echo "  $0              # Run all tests"
		    echo "  $0 engine       # Test engine loading only"
		    echo "  $0 clean        # Clean up test files"
		    ;;
		*)
		    print_status "ERROR" "Unknown test type: $1"
		    print_status "INFO" "Use '$0 help' for usage information"
		    exit 1
		    ;;
	    esac
}

# Trap to cleanup on exit
trap cleanup_test_files EXIT

# Run main function
main "$@"
