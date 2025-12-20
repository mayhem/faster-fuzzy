#!/bin/bash

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Docker image name (must match what docker compose builds)
IMAGE_NAME="faster-fuzzy-mapper"

# Environment file
ENV_FILE="$SCRIPT_DIR/.env"

# Map command names to binaries - this is the single source of truth
declare -A COMMAND_BINARIES
COMMAND_BINARIES["make_mapping"]="/mapper/make_mapping"
COMMAND_BINARIES["make_indexes"]="/mapper/make_indexes"
COMMAND_BINARIES["explore"]="/mapper/explore"
COMMAND_BINARIES["shell"]=""  # handled specially

usage() {
    echo "Usage: $0 <command> [options...]"
    echo ""
    echo "Use this script to run commands inside a docker deployment."
    echo "Configuration is read from .env file in the script directory."
    echo ""
    echo "Available commands:"
    echo "  make_mapping - Build the base mapping from MusicBrainz database"
    echo "  make_indexes - Build the search indexes (artist and recording) used for the cache"
    echo "  explore      - Run the interactive explorer shell (debugging the mapping)"
    echo "  shell        - Open a bash shell in the container"
    echo ""
    echo "Options are passed to the container process (e.g., --skip-artists, --force-rebuild)"
    echo ""
    echo "Examples:"
    echo "  $0 make_mapping"
    echo "  $0 make_indexes [--skip-artists] [--force-rebuild]"
    echo "  $0 explore"
    echo "  $0 shell"
    echo ""
    echo "To run the server, do: docker compose up"
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

# Check for .env file
if [ ! -f "$ENV_FILE" ]; then
    echo "Error: .env file not found at $ENV_FILE"
    echo "Please create a .env file with the required configuration."
    echo ""
    echo "Required variables:"
    echo "  INDEX_DIR=/data"
    echo "  CANONICAL_MUSICBRAINZ_DATA_CONNECT=dbname=... user=... host=... port=... password=..."
    exit 1
fi

echo "Running: $COMMAND"

# Common docker run options as an array
DOCKER_OPTS=(
    --rm
    -v faster-fuzzy_mapper-volume:/data
    --network musicbrainz-docker_default
    --env-file "$ENV_FILE"
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
        echo "Options: $@"
    fi
    # Run with --rm to automatically clean up container after exit
    docker run "${DOCKER_OPTS[@]}" "$IMAGE_NAME" "$BIN" "$@"
fi

echo "Done."
fi

echo "Done."
