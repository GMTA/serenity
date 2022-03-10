#!/usr/bin/env -S bash ../.port_include.sh
port=dethrace
version="git"
workdir="dethrace-60102a37e4b70625584d66fb9f432e5ba244e052"
files="https://github.com/dethrace-labs/dethrace/archive/60102a37e4b70625584d66fb9f432e5ba244e052.zip dethrace-${version}.zip fe6bfc8bddb2f9818e20349122c6ba9a33c6e7d89ed8787b73ee344f06788f07"
auth_type=sha256
depends=("SDL2")
configopts=()

build() {
    run mkdir -p build
    pushd "${workdir}/build"

    cmake "${configopts[@]}" ..
    make

    popd
}

install() {
    mkdir -p ${SERENITY_INSTALL_ROOT}/usr/local/bin/
    run cp build/dethrace ${SERENITY_INSTALL_ROOT}/usr/local/bin/
    run cp build/lib/cglm/libcglm.so.0.8.3 ${SERENITY_INSTALL_ROOT}/usr/local/lib/
    ln -sf libcglm.so.0.8.3 ${SERENITY_INSTALL_ROOT}/usr/local/lib/libcglm.so.0
    ln -sf libcglm.so.0 ${SERENITY_INSTALL_ROOT}/usr/local/lib/libcglm.so
}
