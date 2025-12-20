#!/bin/bash

set -e

# Docker image name (must match what docker compose builds)
IMAGE_NAME="faster-fuzzy-mapper"

# Map command names to binaries - this is the single source of truth
declare -A COMMAND_BINARIES
COMMAND_BINARIES["make_mapping"]="/mapper/make_mapping"
COMMAND_BINARIES["make_indexes"]="/mapper/make_indexes"
COMMAND_BINARIES["explore"]="/mapper/explore"
COMMAND_BINARIES["shell"]=""  # handled specially

usage() {
    echo "Usage: $0 <command> [args...]"
    echo ""
    echo "Use this script to run commands inside a docker deployment."
    echo ""
    echo "Available commands:"
    echo "  make_mapping - Build the base mapping from MusicBrainz database"
    echo "  make_indexes - Build the search indexes (artist and recording) used for the cache"
    echo "  explore      - Run the interactive explorer shell (debugging the mapping)"
    echo "  shell        - Open a bash shell in the container"
    echo ""
    echo "Arguments after the command name are passed to the container process."
    echo ""
    echo "Examples:"
    echo "  $0 make_mapping"
    echo "  $0 make_indexes [--skip-artists] [--force-rebuild]"
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

# Validate command exists in COMMAND_BINARIES
if [[ ! -v COMMAND_BINARIES[$COMMAND] ]]; then
    echo "Error: Invalid command '$COMMAND'"
    echo "Valid commands: ${!COMMAND_BINARIES[*]}"
    echo ""
    usage
fi

echo "Running: $COMMAND"

# Common docker run options as an array to handle spaces properly
DOCKER_OPTS=(
    --rm
    -v mapper-volume:/data
    --network musicbrainz-docker_default
    -e INDEX_DIR=/data
    -e "CANONICAL_MUSICBRAINZ_DATA_CONNECT=dbname=musicbrainz_db user=musicbrainz host=musicbrainz-docker-db-1 port=5432 password=musicbrainz"
)

# Handle shell specially - needs interactive terminal
if [ "$COMMAND" = "shell" ]; then
    echo "Opening interactive shell..."
    docker run "${DOCKER_OPTS[@]}" -it "$IMAGE_NAME" /bin/bash
else
    BIN="${COMMAND_BINARIES[$COMMAND]}"
    if [ -z "$BIN" ]; then
        echo "Error: No binary configured for command '$COMMAND'"
        exit 1
    fi
    if [ $# -gt 0 ]; then
        echo "Additional arguments: $@"
    fi
    # Run with --rm to automatically clean up container after exit
    docker run "${DOCKER_OPTS[@]}" "$IMAGE_NAME" "$BIN" "$@"
fi

echo "Done."
