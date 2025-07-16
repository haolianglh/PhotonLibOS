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
#include <fuse3/fuse.h>
#else
#include <fuse.h>
#endif

#include <unistd.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>


namespace photon {
namespace fs {

struct fuse_pipe {
    size_t size;
    int can_grow;
    int pipefd[2];

    int setup() {
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

        can_grow = 1;
        fcntl(pipefd[0], F_SETPIPE_SZ, 1052672);
        return 0;
    }

    int setdown() {
        close(pipefd[0]);
        close(pipefd[1]);
        return 0;
    }

};


}  // namespace fs
}  // namespace photon
