# TRX Bug Reporting Guide

Thanks for taking the time to report an issue.

Good bug reports help us reproduce problems quickly and spend more time fixing
them instead of guessing. The goal is simple: give us enough information to
see the same bug you saw.

## 1. Where to report bugs

- Please report bugs on GitHub issues.
- If you cannot create a GitHub account, use `#trx-bugs` on Discord.
- For Discord users, please **do not** report bugs in general chat.
  They are much harder to track there and usually miss important details.

## 2. One issue per report

Please open one ticket per bug.

Keeping reports separate makes it easier to reproduce, discuss, fix, and close
each issue cleanly.

## 3. What every report should include

Please include:

- Clear step-by-step reproduction steps
- Exact TRX version
- Operating system
- Logs, especially `TRX.log`
- GPU details if you think it might be relevant

Helpful extras:

- Save files
- Screenshots
- Videos

Extra requirement for custom levels:

- Include the level files

If a custom level bug does not include the level data, we usually cannot debug
it properly.

## 4. How to write reproduction steps

Try to describe the shortest reliable path to the issue.

Good examples:

- > 1. Load Escape from the Base
    > 2. Reach room 66
    > 3. pull the lever
    > 4. game crashes.
- > Play level X → do Y → push block disappears.

Less helpful example:

- `The game crashes sometimes when playing Bartoli's Bughouse.`

That kind of report tells us something is wrong, but not how to reproduce it,
and thus we can't do anything about it.

## 5. Why this matters

Actionable reports are the fastest route to a fix.

If a report does not include reproduction steps, version details, or the files
needed to reproduce the issue, we may not be able to investigate it further and
on a bad day may close it altogether.
