# Release channels

Two repositories in the organization consume this one (plan:
[Docs/Internal/repo-layout-plan.md](../../Docs/Internal/repo-layout-plan.md)).

| Repository | What it does |
|---|---|
| `Audiosurf-Tweaker/Audiosurf-Tweaker` | Mirror of `Revive` (`main`). Updated by `.github/workflows/mirror-org.yml` and `Tools/RepoPublish/mirror.sh` on every push to `Revive`; fast-forward only, so a commit of its own fails the push instead of being overwritten. Holds the manual **Public Release** workflow. |
| `Audiosurf-Tweaker/Audiosurf-Tweaker-Nightly` | No code. [`nightly.yml`](nightly.yml) is dispatched by the mirror after each push to `main`, checks the mirror out with the component submodules, runs `Scripts/Deploy.ps1` and publishes `Audiosurf-Tweaker-Nightly-<yyyyMMdd>.zip` as a pre-release. |

## Secret

`ORG_PUBLISH_TOKEN` in the personal repository: a classic PAT with `repo` and `workflow` scopes,
authorized in the organization. It pushes the mirror (including `.github/workflows` changes) and
dispatches the nightly. Without it the mirror workflow fails with an authentication error.

The release and nightly workflows initialize every public submodule with
`git submodule update --init --recursive` (components, Detours, LuaJIT, DirectX SDK). Harness and
the Quest3D SDK are `update = none`, so they are left out of that pass and handled separately.

## Runner

The in-game plugin needs the proprietary Quest3D SDK and the DirectX June 2010 SDK.

- The **DirectX SDK is a submodule**: `TweakerPlugin/DirectX-SDK-June2010` points at
  [testing-laboratory/DirectX-SDK-June2010](https://github.com/testing-laboratory/DirectX-SDK-June2010)
  (public, small), so `--recurse-submodules` and the workflows bring it automatically.
- The **Quest3D SDK stays private**. The supported layout is a private organization repository
  mounted at `TweakerPlugin/Quest3D-SDK` with `update = none`, so public clones skip it and only a
  workflow with a token can check it out. `init-sdk.ps1` performs that checkout with
  `QUEST3D_TOKEN`; without the token, or without the submodule, the build continues without the
  in-game plugin (UI-only bundle).

### Giving Actions access to the private SDK

`GITHUB_TOKEN` is scoped to the repository that runs the workflow and **cannot** clone another
private repository. The options, best first:

| Option | How | Notes |
|---|---|---|
| **GitHub App** (recommended long-term) | App installed on the org with `Contents: read` on the SDK repo; the workflow mints an installation token | short-lived token, no long-lived secret |
| **Fine-grained PAT** | `Contents: read` on the SDK repo only; stored as an **organization secret** `QUEST3D_TOKEN` granted to the mirror and Nightly | simplest; rotate it |
| **Deploy key** | read-only SSH key on the SDK repo; store the private key as a secret and use `ssh-agent`/`insteadOf` | no org permissions, one repo only |

A private repository is still stored on GitHub and readable by organization owners: if the Quest3D
license forbids that, the remaining option is a **self-hosted runner** with the SDK installed
outside GitHub, selected through `NIGHTLY_RUNNER` / the `runner` input. The workflows already
support that.

### When the private repository exists

1. Add it as `TweakerPlugin/Quest3D-SDK` with `update = none` (and `branch = main`), remove
   `/Quest3D-SDK/` from `TweakerPlugin/.gitignore`.
2. Set `QUEST3D_TOKEN` in the mirror and Nightly repositories (or an org secret granted to them).
3. `init-sdk.ps1` starts checking it out; nothing else in the workflows changes.

### Hosted runner

A hosted `windows-latest` runner has the DirectX SDK through the submodule but not Quest3D, so
its bundle is UI-only unless the repository variable `NIGHTLY_RUNNER` (Nightly) or the `runner`
input (Public Release) points at a self-hosted runner with the SDK.

## Nightly workflow

The Nightly repository has no code, so its workflow is not mirrored from here: the source is kept
as [`nightly.yml`](nightly.yml) and copied into that repository by hand. Keep the two identical.
