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

#pragma once

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 317
#endif

#if FUSE_USE_VERSION >= 30
#include <fuse3/fuse_lowlevel.h>
#else
#include <fuse_lowlevel.h>
#endif

namespace photon {
namespace fs {

struct fuse_pipe {
    size_t size;
    int pipefd[2];

    ssize_t flush();
    int setup();
    int setdown();
};

int fuse_session_receive_splice(
    struct fuse_session *se, struct fuse_buf *buf,
    struct fuse_pipe *iopipe, int fd);

int fuse_session_receive_fd(
    struct fuse_session *se, struct fuse_buf *buf, int fd);

}  // namespace fs
}  // namespace photon
