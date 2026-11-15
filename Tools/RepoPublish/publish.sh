#!/usr/bin/env bash
# Publish a directory of this repository as a repository in the organization, keeping its history.
#
# One-shot bootstrap (Docs/Internal/repo-layout-plan.md §4.1): split the directory into commits
# whose root is that directory, then push them to a brand-new repository. Once a component has
# been published, its repository in the organization is the source of truth and this script
# refuses to publish it again: an existing main is never overwritten.
#
#   DRY_RUN=true   (default) split and report, push nothing
#   DRY_RUN=false  create the repository if needed and push
#   ONLY=a,b       publish only the named components
#   REUSE=true     reuse an existing publish/<name> branch instead of splitting again
#
# The split starts at the commit that first touched the directory: earlier commits cannot contain
# it, so walking the whole history would only cost time. That commit is grafted to be a root for
# the duration of the split (a temporary replace ref, removed afterwards), which is what makes
# git-subtree process dozens of commits instead of the whole history.
#
# Requires git (with git-subtree), gh (authenticated) and python3 or python (to read the manifest).
set -euo pipefail

ORG="${ORG:-Audiosurf-Tweaker}"
DRY_RUN="${DRY_RUN:-true}"
ONLY="${ONLY:-}"
MANIFEST="Tools/RepoPublish/components.json"

root="$(git rev-parse --show-toplevel)"
cd "$root"

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
say() { printf '%s\n' "$*"; }

command -v git >/dev/null || die "git is required"
command -v gh >/dev/null || die "gh is required"
PY="$(command -v python3 || command -v python || true)"
[ -n "$PY" ] || die "python3 (or python) is required to read $MANIFEST"

[ -z "$(git status --porcelain)" ] || die "the working tree is not clean"

ACTIVE_REPLACE=""
cleanup() { [ -n "$ACTIVE_REPLACE" ] && git replace -d "$ACTIVE_REPLACE" >/dev/null 2>&1 || true; }
trap cleanup EXIT

# Emits one component per line: name <TAB> prefix <TAB> repo <TAB> visibility <TAB> description.
rows() {
  "$PY" - "$MANIFEST" "$ONLY" <<'PY'
import json, sys

sys.stdout.reconfigure(newline="\n")   # Windows would otherwise emit CRLF and a stray CR in the last field
manifest, only = sys.argv[1], sys.argv[2]
wanted = {name.strip() for name in only.split(",") if name.strip()}
with open(manifest, encoding="utf-8") as f:
    components = json.load(f)
for c in components:
    if wanted and c["name"] not in wanted:
        continue
    print("\t".join([c["name"], c["prefix"], c["repo"], c["visibility"], c.get("description", "")]))
PY
}

git_push() { # <repo> <refspec>
  local url="https://github.com/$ORG/$1.git"
  if [ -n "${GH_TOKEN:-}" ]; then
    local auth
    auth="$(printf 'x-access-token:%s' "$GH_TOKEN" | base64 | tr -d '\n')"
    git -c "http.https://github.com/.extraheader=AUTHORIZATION: basic $auth" push "$url" "$2"
  else
    git -c credential.helper='!gh auth git-credential' push "$url" "$2"
  fi
}

publish() { # <name> <prefix> <repo> <visibility> <description>
  local name="$1" prefix="$2" repo="$3" visibility="$4" description="$5"
  local mode branch files commits tree source_tree history base

  mode="$(git ls-tree HEAD -- "$prefix" | awk '{print $1}')"
  [ -n "$mode" ] || die "$prefix does not exist at HEAD"
  [ "$mode" = "160000" ] && die "$prefix is already a submodule - publish is one-shot"
  [ "$mode" = "040000" ] || die "$prefix is not a directory"

  history="$(git rev-list --reverse HEAD -- "$prefix")"
  [ -n "$history" ] || die "$prefix has no history"
  base="${history%%$'\n'*}"

  say ""
  say "== $name -> $ORG/$repo ($visibility)"
  say "   source: $prefix (history from $base)"

  branch="publish/$name"
  if [ "${REUSE:-false}" = "true" ] && git rev-parse --verify "refs/heads/$branch" >/dev/null 2>&1; then
    say "   reusing $branch"
  else
    git branch -D "$branch" >/dev/null 2>&1 || true
    git replace -d "$base" >/dev/null 2>&1 || true
    git replace --graft "$base"
    ACTIVE_REPLACE="$base"
    git subtree split -b "$branch" --prefix="$prefix" HEAD
    git replace -d "$base" >/dev/null 2>&1 || true
    ACTIVE_REPLACE=""
  fi

  files="$(git ls-tree -r --name-only "$branch" | wc -l | tr -d ' ')"
  commits="$(git rev-list --count "$branch")"
  tree="$(git rev-parse "${branch}^{tree}")"
  source_tree="$(git rev-parse "HEAD:$prefix")"
  [ "$tree" = "$source_tree" ] || die "$branch tree differs from HEAD:$prefix - re-run without REUSE"
  say "   files: $files, commits: $commits, tree: $tree"

  if [ "$DRY_RUN" = "true" ]; then
    say "   dry run: nothing pushed"
    return
  fi

  if gh repo view "$ORG/$repo" >/dev/null 2>&1; then
    remote_main="$(git ls-remote "https://github.com/$ORG/$repo.git" refs/heads/main | awk '{print $1}')"
    if [ -n "$remote_main" ]; then
      if [ "$remote_main" = "$(git rev-parse "$branch")" ]; then
        say "   already published at $remote_main"
        return
      fi
      die "$ORG/$repo already has main at $remote_main - refusing to overwrite"
    fi
  else
    say "   creating $ORG/$repo"
    gh repo create "$ORG/$repo" "--$visibility" --description "$description" >/dev/null
  fi

  say "   pushing main"
  git_push "$repo" "$branch:main"
  say "   main: $(git ls-remote "https://github.com/$ORG/$repo.git" refs/heads/main | awk '{print $1}')"
}

components="$(rows)"
[ -n "$components" ] || die "no components selected"
while IFS=$'\t' read -r name prefix repo visibility description; do
  [ -n "$name" ] || continue
  description="${description%$'\r'}"
  publish "$name" "$prefix" "$repo" "$visibility" "$description"
done <<< "$components"

say ""
say "done (DRY_RUN=$DRY_RUN)"
