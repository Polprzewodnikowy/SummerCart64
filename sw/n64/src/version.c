#include "version.h"


#define STR(x)  #x
#define XSTR(s) STR(s)


version_t version = {
    .git_branch = XSTR(GIT_BRANCH),
    .git_tag = XSTR(GIT_TAG),
    .git_sha = XSTR(GIT_SHA),
    .git_message = XSTR(GIT_MESSAGE),
};


version_t *version_get (void) {
    return &version;
}
