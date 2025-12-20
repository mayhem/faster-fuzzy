#!/bin/bash

set -e

# TODO: Server doesn't work.

VALID_COMMANDS=("make_index" "make_cache" "explore" "shell")

usage() {
    echo "Usage: $0 <command> [args...]"
    echo ""
    echo "Available commands:"
    echo "  make_index  - Build the base index from MusicBrainz database"
    echo "  make_cache  - Build the search indexes (artist and recording)"
    echo "  explore     - Run the interactive explorer"
    echo "  shell       - Open a bash shell in the container"
    echo ""
    echo "Arguments after the command name are passed to the container process."
    echo ""
    echo "Examples:"
    echo "  $0 make_index"
    echo "  $0 make_index --build-indexes"
    echo "  $0 make_cache"
    echo "  $0 make_cache --skip-artists"
    echo "  $0 make_cache --skip-recordings"
    echo "  $0 explore"
    echo "  $0 shell"
    echo ""
    echo "To run the server, do docker compose up"
    exit 1
}

# Check if command argument provided
if [ -z "$1" ]; then
    echo "Error: No command specified"
    usage
fi

COMMAND="$1"
shift  # Remove command from arguments, rest will be passed to container

# Validate command name
valid=false
for c in "${VALID_COMMANDS[@]}"; do
    if [ "$COMMAND" = "$c" ]; then
        valid=true
        break
    fi
done

if [ "$valid" = false ]; then
    echo "Error: Invalid command '$COMMAND'"
    usage
fi

echo "Running: $COMMAND"

# Map command names to binaries
declare -A COMMAND_BINARIES
COMMAND_BINARIES["make_index"]="/mapper/create"
COMMAND_BINARIES["make_cache"]="/mapper/indexer"
COMMAND_BINARIES["explore"]="/mapper/explore"
COMMAND_BINARIES["server"]="/mapper/server"

# Handle shell specially - needs interactive terminal
if [ "$COMMAND" = "shell" ]; then
    echo "Opening interactive shell..."
    docker compose run --rm -it mapper /bin/bash
else
    BIN="${COMMAND_BINARIES[$COMMAND]}"
    if [ $# -gt 0 ]; then
        echo "Additional arguments: $@"
    fi
    # Run with --rm to automatically clean up container after exit
    # Pass binary and any additional arguments to the container
    docker compose run --rm mapper "$BIN" "$@"
fi

echo "Done."
