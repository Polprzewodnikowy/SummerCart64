#include "version.h"


static version_t version = {
#ifdef GIT_BRANCH
    .git_branch = GIT_BRANCH,
#else
#warning "No GIT_BRANCH provided"
#endif
#ifdef GIT_TAG
    .git_tag = GIT_TAG,
#else
#warning "No GIT_TAG provided"
#endif
#ifdef GIT_SHA
    .git_sha = GIT_SHA,
#else
#warning "No GIT_SHA provided"
#endif
};


version_t *version_get (void) {
    return &version;
}
