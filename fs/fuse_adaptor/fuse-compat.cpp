/*
Copyright 2022 The Photon Authors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "fuse-compat.h"

#if FUSE_USE_VERSION >= 30
#include <fuse3/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <photon/common/alog.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace photon {
namespace fs {

ssize_t fuse_pipe::flush() {
    int nuldev_fd = open("/dev/null", O_WRONLY);
    if (nuldev_fd < 0) {
        LOG_ERROR("fuse-adaptor failed to open null device: `", strerror(errno));
        return -1;
    }

    return splice(pipefd[0], NULL, nuldev_fd, NULL, size, SPLICE_F_MOVE);
}

int fuse_pipe::setup() {
    int rv;
#if !defined(HAVE_PIPE2) || !defined(O_CLOEXEC)
    rv = pipe(pipefd);
    if (rv == -1)
        return rv;

    if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == -1 ||
        fcntl(pipefd[1], F_SETFL, O_NONBLOCK) == -1 ||
        fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) == -1 ||
        fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
#else
    rv = pipe2(pipefd, O_CLOEXEC | O_NONBLOCK);
    if (rv == -1)
        return rv;
#endif

    size_t page_size = getpagesize();
    fcntl(pipefd[0], F_SETPIPE_SZ, page_size *(256 + 1));
    size = page_size * (256 + 1);
    return 0;
}

int fuse_pipe::setdown() {
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}

int fuse_session_receive_splice(
        struct fuse_session *se,
        struct fuse_buf *buf,
        struct fuse_pipe *iopipe,
        int fd) {

    // proto_minor and conn.want_ext are defined internally in libfuse and are
    // not visible here, so we cannot use the following two conditions to determine
    // the availability of splice_read. For now, we assume that splice_read is
    // always enabled.
    // if (se->conn.proto_minor < 14 ||
    //    !(se->conn.want_ext & FUSE_CAP_SPLICE_READ))

    errno = 0;
    ssize_t res = splice(fd, NULL, iopipe->pipefd[1], NULL, iopipe->size, 0);
    int err = errno;

    if (fuse_session_exited(se))
        return 0;

    if (res == -1) {
        if (err == ENODEV) {
            fuse_session_exit(se);
            return 0;
        }

        if (err == EINVAL) {
            iopipe->flush();
        }

        if (err != EINTR && err != EAGAIN)
            LOG_INFO("splice from device error `", err);
        return -err;
    }

    // The struct fuse_in_header is defined internally within libfuse and is not
    // visible externally, so we cannot truly obtain its size. Although we all
    // know that its size is likely 40.
    // if (res < sizeof(struct fuse_in_header)) {
    //    LOG_ERROR("short splice from fuse device `", res);
    //    iopipe->flush();
    //    return -EIO;
    // }

    struct fuse_buf tmpbuf = (struct fuse_buf) {
        .size = (size_t)res,
        .flags = FUSE_BUF_IS_FD,
        .fd = iopipe->pipefd[0],
    };

    buf->fd = tmpbuf.fd;
    buf->flags = tmpbuf.flags;
    buf->size = tmpbuf.size;

    return res;
}

int fuse_session_receive_fd(
        struct fuse_session *se,
        struct fuse_buf *buf,
        int fd) {
    int err = 0;
    ssize_t res = 0;
    size_t bufsize = getpagesize() * (256 + 1);

    if (!buf->mem) {
        buf->mem = malloc(bufsize);
    }
restart:
    errno = 0;
    res = read(fd, buf->mem, bufsize);
    err = errno;
    if (res == -1 && err == EWOULDBLOCK) {
        return 0;
    }

    if (fuse_session_exited(se)) {
        return 0;
    }

    if (res == -1) {
        // ENOENT means the operation was interrupted, it's safe to restart
        if (err == ENOENT)
            goto restart;

        if (err == ENODEV) {
			// Filesystem was unmounted, or connection was aborted
            //  via /sys/fs/fuse/connections
            fuse_session_exit(se);
            return 0;
        }

        // Errors occurring during normal operation: EINTR (read
        // interrupted), EAGAIN (nonblocking I/O), ENODEV (filesystem
        // umounted) */
        if (err != EINTR && err != EAGAIN) {
            LOG_ERROR("reading device was interrupted");
        }

        return -err;
    }

    return res;
}

}  // namespace fs
}  // namespace photon
