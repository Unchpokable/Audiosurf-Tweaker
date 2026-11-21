# Release channels

Two repositories in the organization consume this one (plan:
[Docs/Internal/repo-layout-plan.md](../../Docs/Internal/repo-layout-plan.md)).

| Repository | What it does |
|---|---|
| `Audiosurf-Tweaker/Audiosurf-Tweaker` | Mirror of `Revive` (`main`). Updated by `.github/workflows/mirror-org.yml` and `Tools/RepoPublish/mirror.sh` on every push to `Revive`; fast-forward only, so a commit of its own fails the push instead of being overwritten. Holds the manual **Public Release** workflow. |
| `Audiosurf-Tweaker/Audiosurf-Tweaker-Nightly` | No code. [`nightly.yml`](nightly.yml) is dispatched by the mirror after each push to `main`, checks the mirror out with the component submodules, runs `Scripts/Deploy.ps1` and publishes `Audiosurf-Tweaker-Nightly-<yyyyMMdd>.zip` as a regular release: the repository itself is the experimental channel. |

## Credentials

One **GitHub App** owned by the organization covers all three repositories:

| Repository | Access | Used for |
|---|---|---|
| `Audiosurf-Tweaker` (mirror) | Contents: Read and write, **Workflows: Read and write** | fast-forward push of `Revive` → `main`, tags |
| `Audiosurf-Tweaker-Nightly` | Contents: Read and write | `repository_dispatch` and the release |
| `Quest3D-SDK` | Contents: Read-only | checking the private SDK out |

### App setup

1. Organization **Settings → Developer settings → GitHub Apps → New GitHub App**. A GitHub App
   cannot be created from `gh` or the REST API — the web/manifest flow is the only way.
2. Name it anything neutral (`audiosurf-tweaker-builder`), homepage the main repository, **Webhook →
   Active** off.
3. Repository permissions:
   - **Contents: Read and write** — mirror push, nightly dispatch, SDK checkout;
   - **Workflows: Read and write** — the mirror push carries `.github/workflows/*`, and GitHub
     refuses that without this permission;
   - Metadata: Read is automatic.
4. **Where can this GitHub App be installed?** Only on this account. Create it, note the **App ID**,
   then **Generate a private key** — the `.pem` downloads once.
5. **Install App** → organization → **Only select repositories** → the three repositories above.
6. Variables and secrets are repository-scoped, so add the same pair everywhere a workflow that uses
   the App runs:

   | Repository | Variable | Secret |
   |---|---|---|
   | `Unchpokable/Audiosurf-Tweaker` (mirror workflow) | `QUEST3D_APP_ID` | `QUEST3D_APP_PRIVATE_KEY` |
   | `Audiosurf-Tweaker/Audiosurf-Tweaker` (Public Release) | `QUEST3D_APP_ID` | `QUEST3D_APP_PRIVATE_KEY` |
   | `Audiosurf-Tweaker/Audiosurf-Tweaker-Nightly` (Nightly, SDK check) | `QUEST3D_APP_ID` | `QUEST3D_APP_PRIVATE_KEY` |

7. The old `ORG_PUBLISH_TOKEN` secret is then unused and can be removed.

The workflows mint one-hour installation tokens scoped to the repositories they need
(`actions/create-github-app-token`): the mirror job gets the mirror and the nightly, the release and
nightly jobs get `Quest3D-SDK` with `Contents: read` only. Nothing long-lived is stored beyond the
private key, and the token is masked in logs.

Run the **SDK access check** workflow in the Nightly repository (source: [`sdk-access.yml`](sdk-access.yml))
to verify the configuration: it prints whether the App ID and private key are visible to that
repository and does `git ls-remote` against the private SDK with a freshly minted token.

## Runner

The in-game plugin needs the proprietary Quest3D SDK and the DirectX June 2010 SDK.

- The **DirectX SDK is a submodule**: `TweakerPlugin/DirectX-SDK-June2010` points at
  [testing-laboratory/DirectX-SDK-June2010](https://github.com/testing-laboratory/DirectX-SDK-June2010)
  (public, small), so `--recurse-submodules` and the workflows bring it automatically.
- **Dear ImGui is a submodule** too (`TweakerPlugin/ImGui`, tag `v1.92.9`); Detours and LuaJIT were
  already submodules.
- The **Quest3D SDK is private** and lives in
  [`Audiosurf-Tweaker/Quest3D-SDK`](https://github.com/Audiosurf-Tweaker/Quest3D-SDK). It is mounted
  at `TweakerPlugin/Quest3D-SDK` as a submodule with `update = none`, so public clones skip it; only
  a workflow with the App token can check it out. `init-sdk.ps1` performs that checkout; without a
  token the build continues without the in-game plugin (UI-only bundle).

The release and nightly workflows initialize every public submodule with
`git submodule update --init --recursive`; harness and Quest3D are `update = none`, so they are left
for the steps that own them.

### Hosted runner

A hosted `windows-latest` runner builds the complete bundle: DirectX, ImGui, Detours and LuaJIT are
submodules, and Quest3D is checked out through the App token. UI-only is what happens when the App
values are missing — not a runner limitation. Verified: a full nightly built there in 11m49s. A
self-hosted runner (`NIGHTLY_RUNNER` / the `runner` input) remains an option for faster builds or
when the SDK must not live on GitHub.

### When the private repository is not available

The workflows skip the SDK and build a UI-only bundle.

## Nightly workflow

The Nightly repository has no code, so its workflow is not mirrored from here: the source is kept
as [`nightly.yml`](nightly.yml) and copied into that repository by hand. Keep the two identical.
