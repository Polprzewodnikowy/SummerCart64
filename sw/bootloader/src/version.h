#ifndef VERSION_H__
#define VERSION_H__


typedef const struct {
    const char *git_branch;
    const char *git_tag;
    const char *git_sha;
    const char *git_message;
} version_t;


const version_t *version_get (void);


#endif
