#include "display.h"


const struct {
    const char *git_branch;
    const char *git_tag;
    const char *git_sha;
    const char *git_message;
} version = {
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


void version_print (void) {
    display_printf("[ SC64 bootloader metadata ]\n");
    display_printf("branch: %s | tag: %s\n", version.git_branch, version.git_tag);
    display_printf("sha: %s\n", version.git_sha);
    display_printf("msg: %s\n", version.git_message);
    display_printf("\n");
}
