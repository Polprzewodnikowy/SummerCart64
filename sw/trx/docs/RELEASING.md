# Releasing TRX

## Branching model

We have two branches: `develop` and `stable`. `develop` is where all changes
about to be published in the next release land. `stable` is the latest release.

## Releasing a new version

New version releases are published automatically whenever a new tag is pushed
to the `stable` branch with the help of GitHub actions. The general workflow is
this:

```console
RELEASE_VERSION=...

# Switch to the stable branch.
git checkout stable

# Merge `develop` into it.
git merge develop

# Create a special commit `docs: release X.Y.Z` marking the release in the
# relevant changelog file. Then tag it with `trx-X.Y.Z`.Y.Z`.
# You can do that by hand, or run the command below:
tools/release commit ${RELEASE_VERSION}
tools/release tag ${RELEASE_VERSION}

# Review the changelog content.

# Switch back to develop.
git checkout develop

# Merge stable using fast-forward.
git merge --ff stable

# Review both branches and changes. If everything is okay, push to GitHub.
# You can do this by hand: git push origin develop stable trx-X.Y.Z, or:
# tools/release push ${RELEASE_VERSION}
```

## Hotfixes

Hotfix releases are a bit different as we try to not include non-bugfix changes
in them. Here instead of merging `develop` to `stable` we cherry-pick relevant
changes, resolving conflicts along the way.

## Versioning

We increase the major version for significant releases based on judgment,
typically defaulting to increasing the minor version. Hotfixes increase the
patch version.
