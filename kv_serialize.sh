#!/bin/bash

# kv_serialize.sh
# Serializes a key;value string into a binary stream using a specific little-endian format.
#
# Usage: ./kv_serialize.sh "key_string;value_string" > output.bin
#
# Requires: bash, coreutils, and perl (for little-endian packing).

# --- Configuration ---
INPUT="$1"
# The 'Q<' format specifier ensures 64-bit unsigned integer (uint64_t) in Little-Endian.
# The 'V' format specifier ensures 32-bit unsigned integer (uint32_t) in Little-Endian.
PERL_PACK_SCRIPT='
    # 1. Read input key and value from arguments
    my $input = $ARGV[0];
    my ($key, $value) = split(";", $input, 2);

    # 2. Get lengths (key->len and value->len)
    my $key_len = length($key);
    my $value_len = length($value);

    # 3. Calculate the length of the internal payload (as per C code):
    # (4 + key_len + 4) + (4 + value_len + 4) = 16 + key_len + value_len
    my $total_payload_len = 16 + $key_len + $value_len;

    # 4. Define the serialization structure:
    # Q<: uint64_t Length (Start)
    # V: uint32_t Key_Length (Start)
    # a*: Raw Key Data
    # V: uint32_t Key_Length (End)
    # V: uint32_t Value_Length (Start)
    # a*: Raw Value Data
    # V: uint32_t Value_Length (End)
    # Q<: uint64_t Length (End)
    
    # Pack the binary data and print it to standard output
    print pack(
        "V v a* v v a* v V",
        $total_payload_len,  # Preamble Length (8 bytes)
        $key_len,            # Key Length (Start) (4 bytes)
        $key,                # Key Data
        $key_len,            # Key Length (End) (4 bytes)
        $value_len,          # Value Length (Start) (4 bytes)
        $value,              # Value Data
        $value_len,          # Value Length (End) (4 bytes)
        $total_payload_len   # Postamble Length (8 bytes)
    );
'

# --- Main Execution ---

if [ -z "$INPUT" ]; then
    echo "Usage: $0 \"key;value\"" >&2
    echo "Example: $0 \"user_id;12345\"" >&2
    exit 1
fi

# Execute the Perl script using the input argument
# The output is the raw binary stream
perl -e "$PERL_PACK_SCRIPT" "$INPUT"

# To check the output, you can pipe it to 'xxd -g 8' to see the hex dump.
# Example: ./kv_serialize.sh "user_id;12345" | xxd -g 8
