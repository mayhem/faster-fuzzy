#!/bin/bash

set -e

VALID_SERVICES=("make_index" "make_cache" "explore")

usage() {
    echo "Usage: $0 <service>"
    echo ""
    echo "Available services:"
    echo "  make_index  - Build the base index from MusicBrainz database"
    echo "  make_cache  - Build the search indexes (artist and recording)"
    echo "  explore     - Run the interactive explorer"
    echo ""
    echo "Options:"
    echo "  Any additional arguments are passed to the container command"
    echo ""
    echo "Examples:"
    echo "  $0 make_index"
    echo "  $0 make_cache"
    echo "  $0 make_cache --skip-artists"
    echo "  $0 explore"
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
if [ $# -gt 0 ]; then
    echo "Additional arguments: $@"
fi

# Run with --rm to automatically clean up container after exit
# Pass any additional arguments to the container
docker compose run --rm "$SERVICE" "$@"

echo "Done."
