#!/bin/bash

set -e

VALID_SERVICES=("make_index" "make_cache" "explore" "shell")

usage() {
    echo "Usage: $0 <service> [args...]"
    echo ""
    echo "Available services:"
    echo "  make_index  - Build the base index from MusicBrainz database"
    echo "  make_cache  - Build the search indexes (artist and recording)"
    echo "  explore     - Run the interactive explorer"
    echo "  shell       - Open a bash shell in the container"
    echo ""
    echo "Arguments after the service name are passed to the container command."
    echo ""
    echo "Examples:"
    echo "  $0 make_index"
    echo "  $0 make_index --build-indexes"
    echo "  $0 make_cache"
    echo "  $0 make_cache --skip-artists"
    echo "  $0 make_cache --skip-recordings"
    echo "  $0 explore"
    echo "  $0 shell"
    exit 1
}

# Check if service argument provided
if [ -z "$1" ]; then
    echo "Error: No service specified"
    usage
fi

SERVICE="$1"
shift  # Remove service from arguments, rest will be passed to container

# Validate service name
valid=false
for s in "${VALID_SERVICES[@]}"; do
    if [ "$SERVICE" = "$s" ]; then
        valid=true
        break
    fi
done

if [ "$valid" = false ]; then
    echo "Error: Invalid service '$SERVICE'"
    usage
fi

echo "Running service: $SERVICE"

# Map service names to commands
declare -A SERVICE_COMMANDS
SERVICE_COMMANDS["make_index"]="/mapper/create"
SERVICE_COMMANDS["make_cache"]="/mapper/indexer"
SERVICE_COMMANDS["explore"]="/mapper/explore"

# Handle shell specially - needs interactive terminal
if [ "$SERVICE" = "shell" ]; then
    echo "Opening interactive shell..."
    docker compose run --rm -it explore /bin/bash
else
    CMD="${SERVICE_COMMANDS[$SERVICE]}"
    if [ $# -gt 0 ]; then
        echo "Additional arguments: $@"
    fi
    # Run with --rm to automatically clean up container after exit
    # Pass command and any additional arguments to the container
    docker compose run --rm "$SERVICE" "$CMD" "$@"
fi

echo "Done."
