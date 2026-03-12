#!/bin/sh -ue
#
check_executable() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Error: $1 is not found or not executable."
        exit 1
    fi
}

NEXCACHE_SERVER=${NEXCACHE_SERVER:-nexcache-server}
NEXCACHE_PORT=${NEXCACHE_PORT:-56379}
NEXCACHE_TLS_PORT=${NEXCACHE_TLS_PORT:-56443}
TEST_TLS=${TEST_TLS:-0}
SKIPS_AS_FAILS=${SKIPS_AS_FAILS:-0}
ENABLE_DEBUG_CMD=
TLS_TEST_ARGS=
SKIPS_ARG=${SKIPS_ARG:-}
NEXCACHE_DOCKER=${NEXCACHE_DOCKER:-}
TEST_RDMA=${TEST_RDMA:-0}
RDMA_TEST_ARGS=
TEST_CLUSTER=${TEST_CLUSTER:-0}
CLUSTER_TEST_ARGS=

check_executable "$NEXCACHE_SERVER"

# Enable debug command for nexcache-server >= 7.0.0 or any version of nexcache-server.
NEXCACHE_MAJOR_VERSION="$("$NEXCACHE_SERVER" --version|awk -F'[^0-9]+' '{ print $2 }')"
if [ "$NEXCACHE_MAJOR_VERSION" -gt "6" ]; then
    ENABLE_DEBUG_CMD="enable-debug-command local"
fi

tmpdir=$(mktemp -d)
PID_FILE=${tmpdir}/libnexcache-test-nexcache.pid
SOCK_FILE=${tmpdir}/libnexcache-test-nexcache.sock
CONF_FILE=${tmpdir}/nexcache.conf

if [ "$TEST_TLS" = "1" ]; then
    TLS_CA_CERT=${tmpdir}/ca.crt
    TLS_CA_KEY=${tmpdir}/ca.key
    TLS_CERT=${tmpdir}/nexcache.crt
    TLS_KEY=${tmpdir}/nexcache.key

    openssl genrsa -out ${tmpdir}/ca.key 4096
    openssl req \
        -x509 -new -nodes -sha256 \
        -key ${TLS_CA_KEY} \
        -days 3650 \
        -subj '/CN=Libnexcache Test CA' \
        -out ${TLS_CA_CERT}
    openssl genrsa -out ${TLS_KEY} 2048
    openssl req \
        -new -sha256 \
        -key ${TLS_KEY} \
        -subj '/CN=Libnexcache Test Cert' | \
        openssl x509 \
            -req -sha256 \
            -CA ${TLS_CA_CERT} \
            -CAkey ${TLS_CA_KEY} \
            -CAserial ${tmpdir}/ca.txt \
            -CAcreateserial \
            -days 365 \
            -out ${TLS_CERT}

    TLS_TEST_ARGS="--tls-host 127.0.0.1 --tls-port ${NEXCACHE_TLS_PORT} --tls-ca-cert ${TLS_CA_CERT} --tls-cert ${TLS_CERT} --tls-key ${TLS_KEY}"
fi

cleanup() {
  if [ -n "${NEXCACHE_DOCKER}" ] ; then
    docker kill nexcache-test-server
  else
    set +e
    kill $(cat ${PID_FILE})
  fi
  rm -rf ${tmpdir}
}
trap cleanup INT TERM EXIT

# base config
cat > ${CONF_FILE} <<EOF
pidfile ${PID_FILE}
port ${NEXCACHE_PORT}
unixsocket ${SOCK_FILE}
unixsocketperm 777
appendonly no
save ""
EOF

# if not running in docker add these:
if [ ! -n "${NEXCACHE_DOCKER}" ]; then
cat >> ${CONF_FILE} <<EOF
daemonize yes
${ENABLE_DEBUG_CMD}
bind 127.0.0.1
EOF
fi

# if doing tls, add these
if [ "$TEST_TLS" = "1" ]; then
    cat >> ${CONF_FILE} <<EOF
tls-port ${NEXCACHE_TLS_PORT}
tls-ca-cert-file ${TLS_CA_CERT}
tls-cert-file ${TLS_CERT}
tls-key-file ${TLS_KEY}
EOF
fi

# if doing RDMA, add these
if [ "$TEST_RDMA" = "1" ]; then
    cat >> ${CONF_FILE} <<EOF
loadmodule ${NEXCACHE_RDMA_MODULE} bind=${NEXCACHE_RDMA_ADDR} port=${NEXCACHE_PORT}
EOF
RDMA_TEST_ARGS="--rdma-addr ${NEXCACHE_RDMA_ADDR}"
fi

echo ${tmpdir}
cat ${CONF_FILE}
if [ -n "${NEXCACHE_DOCKER}" ] ; then
    chmod a+wx ${tmpdir}
    chmod a+r ${tmpdir}/*
    docker run -d --rm --name nexcache-test-server \
        -p ${NEXCACHE_PORT}:${NEXCACHE_PORT} \
        -p ${NEXCACHE_TLS_PORT}:${NEXCACHE_TLS_PORT} \
        -v ${tmpdir}:${tmpdir} \
        ${NEXCACHE_DOCKER} \
        ${NEXCACHE_SERVER} ${CONF_FILE}
else
    ${NEXCACHE_SERVER} ${CONF_FILE}
fi
# Wait until we detect the unix socket
echo waiting for server
while [ ! -S "${SOCK_FILE}" ]; do
    2>&1 echo "Waiting for server..."
    ps aux|grep nexcache-server
    sleep 1;
done

# Treat skips as failures if directed
[ "$SKIPS_AS_FAILS" = 1 ] && SKIPS_ARG="${SKIPS_ARG} --skips-as-fails"

# if cluster is not available, skip cluster tests
if [ "$TEST_CLUSTER" = "1" ]; then
    CLUSTER_TEST_ARGS="${CLUSTER_TEST_ARGS} --enable-cluster-tests"
fi

${TEST_PREFIX:-} ./client_test -h 127.0.0.1 -p ${NEXCACHE_PORT} -s ${SOCK_FILE} ${TLS_TEST_ARGS} ${SKIPS_ARG} ${RDMA_TEST_ARGS} ${CLUSTER_TEST_ARGS}
