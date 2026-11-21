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

## Runner

The in-game plugin needs the proprietary Quest3D SDK and the DirectX June 2010 SDK. A hosted
`windows-latest` runner builds everything except `TweakerPlugin.dll`, so its bundle is UI-only.
Set the repository variable `NIGHTLY_RUNNER` in the Nightly repository (and pass `runner` to the
manual Public Release) to a self-hosted runner that has both SDKs for a complete bundle.

## Nightly workflow

The Nightly repository has no code, so its workflow is not mirrored from here: the source is kept
as [`nightly.yml`](nightly.yml) and copied into that repository by hand. Keep the two identical.
