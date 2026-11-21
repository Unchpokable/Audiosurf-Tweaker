#!/usr/bin/env bash
# Mirrors Revive into the organization's mirror repository and asks the nightly channel to build.
# Runs from .github/workflows/mirror-org.yml on a push to Revive; also usable by hand.
#
#   MIRROR_REPO   default Audiosurf-Tweaker/Audiosurf-Tweaker
#   NIGHTLY_REPO  default Audiosurf-Tweaker-Audiosurf-Tweaker-Nightly (see below)
#   SOURCE_BRANCH default Revive
#
# Needs GH_TOKEN with write access to the mirror and permission to dispatch to the nightly repo.
set -euo pipefail

MIRROR_REPO="${MIRROR_REPO:-Audiosurf-Tweaker/Audiosurf-Tweaker}"
NIGHTLY_REPO="${NIGHTLY_REPO:-Audiosurf-Tweaker/Audiosurf-Tweaker-Nightly}"
SOURCE_BRANCH="${SOURCE_BRANCH:-Revive}"

root="$(git rev-parse --show-toplevel)"
cd "$root"

say() { printf '%s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[ -n "${GH_TOKEN:-}" ] || die "GH_TOKEN is required"

sha="$(git rev-parse "$SOURCE_BRANCH")"
url="https://github.com/$MIRROR_REPO.git"

git_push() { # <refspec>
  local auth
  # A persisted checkout credential would be a second Authorization header; drop it if present.
  git config --local --unset-all http.https://github.com/.extraheader >/dev/null 2>&1 || true
  auth="$(printf 'x-access-token:%s' "$GH_TOKEN" | base64 | tr -d '\n')"
  git -c "http.https://github.com/.extraheader=AUTHORIZATION: basic $auth" push "$url" "$1"
}

# Fast-forward only: if the mirror's main has a commit of its own, this fails and that is the signal
# to look, not to overwrite it. The mirror is a copy, never a second source of truth.
say "==> Mirroring $SOURCE_BRANCH ($sha) to $MIRROR_REPO main"
git_push "$SOURCE_BRANCH:main"

say "==> Mirroring release tags"
for tag in $(git tag --list --merged "$SOURCE_BRANCH"); do
    if git ls-remote --exit-code "$url" "refs/tags/$tag" >/dev/null 2>&1; then
        continue
    fi
    say "    $tag"
    git_push "refs/tags/$tag:refs/tags/$tag"
done

say "==> Dispatching the nightly build for $sha"
gh api "repos/$NIGHTLY_REPO/dispatches" \
    -f event_type=nightly \
    -F "client_payload[sha]=$sha" >/dev/null
say "    dispatched"
