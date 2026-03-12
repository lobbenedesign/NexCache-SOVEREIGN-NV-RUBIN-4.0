#!/bin/bash
# Local test script to mirror GitHub Actions environment in Docker

CONTAINER_NAME="nexcache-build-test"
IMAGE="ubuntu:22.04"

# Clean up any existing container
docker rm -f $CONTAINER_NAME 2>/dev/null

# Run the build in the container
docker run --name $CONTAINER_NAME -v "$(pwd):/build" -w /build $IMAGE bash -c "
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y build-essential pkg-config tcl8.6 tclx libssl-dev git
    
    echo '--- STARTING BUILD (CLEAN) ---'
    make clean
    make -j\$(nproc)
    BUILD_EXIT_CODE=\$?
    
    if [ \$BUILD_EXIT_CODE -eq 0 ]; then
        echo '✅ BUILD SUCCESSFUL'
    else
        echo '❌ BUILD FAILED WITH EXIT CODE \$BUILD_EXIT_CODE'
        exit \$BUILD_EXIT_CODE
    fi
"
