# Submitting changes

## Pull requests

We commit via pull requests rather than directly to the protected `develop`
branch. Each pull request undergoes a peer review and requires at least one
approval from the development team before merging. We ensure that all
discussions are resolved and aim to test changes prior to merging. When a code
review comment is minor and the author has addressed it, they should mark it as
resolved. Otherwise, we leave discussions open to allow reviewers to respond.
After addressing all change requests, it's considerate to re-request a review
from the relevant parties.

## Changelog

We maintain a changelog for each project in the `CHANGELOG.md` files, recording
any changes except internal modifications or refactors. New features and
original bug fixes should also be documented in the `README.md`. If a change
affects game flow behavior, be sure to update the `GAME_FLOW/` accordingly.
Likewise, changes to the console commands should update `COMMANDS.md`.

## Commit scope

When merging, we use rebasing for a clean commit history. For that reason,
each significant change should have an isolated commit. It's okay to force-push
pull requests.

## Commit messages

**Bug fixes and feature implementations should include the phrase `Resolves
#123`.** For player-facing changes without an existing ticket, a ticket needs
to be created first.

Anything else is just for consistency and general neatness. Our commit messages
aim to respect the 50/72 rule and have the following form:

    module-prefix: description in an imperative mood (max 50 characters)

    Longer description of what happens that can span multiple lines. Each
    line should be maximally 72 characters long, with the exceptions of
    code/log dumps.

The prefix should describe the module that the pull request touches the most.
In general this is the name of the `.c` or `.h` file with the most changes.
Note that this includes the folder names which are separated with `/`. Avoid
underscores (`_`) in favor of dashes (`-`).

The description should be as concise as possible; any details should be given
in the commit message body. Use simple, to the point words like `add`, `fix`,
`remove`, `improve`.

Good:

```text
ui: improve resolution changing

Added the ability for the player to switch resolutions directly from
the game ui.

Resolves #123.
```

Great:

```text
log: fix varargs for Log_Message()

On Linux, the engine crashes when printing the log messages. This
happens because the current code re-uses the same va_list variable on
two calls to vprintf() and vfprintf(). Actually, this is not allowed.
For using the same information on multiple formatting functions, it is
needed to create a copy of the primary va_list to a second one, by using
va_copy(). After rewriting properly the Log_Message() function, the
segmentation fault is gone. Tested on both Linux and Windows builds.
```

> [!NOTE]
> This has no ticket number, but it was an internal change improving support
> for a platform unsupported at that time, which made it acceptable.

Bad:

```text
ui: implemented the ability to switch resolutions from the ui
```

- the subject doesn't use imperative mood
- the subject is too long
- it's missing a ticket number

Bad:

```text
dart: added dart emitters to the savegame (#779)

dart: added dart emitters to the savegame

Add function for checking legacy savegame save flags
Resolves #774.
```

- it duplicates the subject in the message body
- the subject doesn't use imperative mood

When using squash to merge, it is acceptable for GitHub to append the pull
request number, but it's important to carefully review the body field, as it
often includes unwanted content.
