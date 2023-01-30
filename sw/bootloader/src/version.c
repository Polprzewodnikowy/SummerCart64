#include "version.h"


static version_t version = {
#ifdef GIT_BRANCH
    .git_branch = GIT_BRANCH,
#else
#warning "No GIT_BRANCH provided"
    .git_branch = "GIT_BRANCH",
#endif
#ifdef GIT_TAG
    .git_tag = GIT_TAG,
#else
#warning "No GIT_TAG provided"
    .git_tag = "GIT_TAG",
#endif
#ifdef GIT_SHA
    .git_sha = GIT_SHA,
#else
#warning "No GIT_SHA provided"
    .git_sha = "GIT_SHA",
#endif
#ifdef GIT_MESSAGE
    .git_message = GIT_MESSAGE,
#else
#warning "No GIT_MESSAGE provided"
    .git_message = "GIT_MESSAGE",
#endif
};


version_t *version_get (void) {
    return &version;
}
