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
- The **Quest3D SDK is private** and lives in
  [`Audiosurf-Tweaker/Quest3D-SDK`](https://github.com/Audiosurf-Tweaker/Quest3D-SDK) (private).
  It is mounted at `TweakerPlugin/Quest3D-SDK` as a submodule with `update = none`, so public
  clones skip it; only a workflow with a GitHub App token can check it out. `init-sdk.ps1`
  performs that checkout; without a token the build continues without the in-game plugin
  (UI-only bundle).

### GitHub App access

`GITHUB_TOKEN` is scoped to the workflow's repository and cannot clone another private repository.
The channel uses a **GitHub App owned by the organization**, installed on the SDK repository only,
read-only:

1. Organization **Settings → Developer settings → GitHub Apps → New GitHub App**. A GitHub App
   cannot be created from `gh` or the REST API — the web/manifest flow is the only way.
2. Name it anything neutral (`audiosurf-tweaker-sdk-reader`), set the homepage to the main
   repository, and **uncheck Webhook → Active**.
3. Repository permissions: **Contents: Read-only**, nothing else (Metadata is always read).
4. **Where can this GitHub App be installed?** Only on this account.
5. Create it, note the **App ID**, then **Generate a private key** — the `.pem` downloads once.
6. **Install App** → organization → **Only select repositories** → `Quest3D-SDK`.
7. In the mirror and Nightly repositories (or as organization values granted to them):
   - variable `QUEST3D_APP_ID` — the App ID;
   - secret `QUEST3D_APP_PRIVATE_KEY` — the `.pem` contents.

The workflow mints a one-hour installation token scoped to `Quest3D-SDK`
(`actions/create-github-app-token`) and passes it to `init-sdk.ps1`. Nothing long-lived is stored
beyond the private key, and the action masks the token in logs.

Alternatives if the App is not workable: a fine-grained PAT with `Contents: read` on the SDK
repository (stored as `QUEST3D_TOKEN`), or a read-only deploy key. Both are less scoped and
longer-lived.

### When the private repository is not available

The workflows skip the SDK and build a UI-only bundle. A self-hosted runner with the SDK installed
outside GitHub (`NIGHTLY_RUNNER` / the `runner` input) remains the fallback when the SDK must not
be stored on GitHub at all.

### Hosted runner

A hosted `windows-latest` runner has the DirectX SDK through the submodule but not Quest3D unless
the App variables are configured; even with the SDK checked out, the plugin builds normally there.

### Hosted runner

A hosted `windows-latest` runner has the DirectX SDK through the submodule but not Quest3D, so
its bundle is UI-only unless the repository variable `NIGHTLY_RUNNER` (Nightly) or the `runner`
input (Public Release) points at a self-hosted runner with the SDK.

## Nightly workflow

The Nightly repository has no code, so its workflow is not mirrored from here: the source is kept
as [`nightly.yml`](nightly.yml) and copied into that repository by hand. Keep the two identical.
