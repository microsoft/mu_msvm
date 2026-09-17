# OneBranch CI Scripts

Python entrypoints used by `.azuredevops/pipelines/OneBranch/jobs.yml`.

## Main scripts

- `setup.py`: pip install + Stuart setup/update
- `install_toolchain.py`: pool/toolchain-specific tool install logic
- `build.py`: runs `PlatformBuild.py` for one variant and prints command/logs
- `stage_artifacts.py`: stages logs, firmware binaries, maps, and PDBs
- `set_repo_auth.py`: sets/unsets git extraheader auth for internal ADO repos
- `run_local_ci.py`: local developer entrypoint (setup + build + optional staging)
- `get_vpack_name.py`: reference implementation for vpack naming
- `source_checks.py`: updates CI dependencies and runs MsvmPkg source checks

## Local usage

Run one local build variant from repo root:

```powershell
py .\.azuredevops\pipelines\OneBranch\scripts\run_local_ci.py --arch X64 --target DEBUG --tool-chain-tag VS2022
```

Run local verification checks:

```powershell
py .\.azuredevops\pipelines\OneBranch\scripts\tests\verify_ci.py
```

## Stuart Source Checks

The initial onboarding ported mu_msvm commit `61216d796b1f873680c89b8e00141fb049c8efcd`.
Following the merged internal formatting change, enforcement adapts upstream
commit `acbd02cb02843ca6c3ebe5279665aff168876019` to OneBranch.
Shared Stuart settings also incorporate the final CI
changes in `8848e1d684f54b025ebe32707c0919e47a2c0c0e`, including Patina and
recursive submodule declarations. OneBranch temporarily uses non-recursive
source-check provisioning as described below. OpenVMM testing and the GitHub reusable-action
migration are not imported. Shared settings, check policy, BUILDING/DEVGUIDE
documentation, and `pip-requirements.txt` match that upstream revision.
OneBranch uses the same `pip-requirements.txt` as the shared documentation.
`.github/workflows/Platform-Build.yml` remains the earlier onboarding snapshot;
ADO executes OneBranch, not that historical GitHub workflow.
The mirrored workflow is not an authenticated GitHub build of this private
repository, whose MU_BASECORE submodule requires ADO access.

Run from an existing development checkout with the stable Python requirements
installed and top-level submodules initialized:

```powershell
python .\.azuredevops\pipelines\OneBranch\scripts\source_checks.py
```

Use `--setup` on a fresh checkout to install the stable Python requirements and
run `git -c core.autocrlf=false submodule update --init`. This initializes
MU_BASECORE, Common/MU, Feature/DEBUGGER, and Common/PATINA_EDK2 submodules at
their pinned revisions without recursively downloading nested submodules.
Authenticate to the private MU_BASECORE repository first. The script does not
provision compilers, Rust, or firmware binary dependencies.

The intended setup command is `stuart_setup -c .pytool/CISettings.py`, keeping
submodule selection under the shared upstream settings. Its recursive checkout
failed in build 157900552 because the hosted agent could not connect to GitLab
to download libspdm's nested cmocka test dependency. Use top-level checkout for
these source-only checks until fresh hosted agents can resolve all nested
dependencies; the shared settings remain unchanged.

For manual setup on a private development checkout, authenticate first, then run:

```powershell
git config core.autocrlf false
git config core.longpaths true
python -m pip install -r pip-requirements.txt
git -c core.autocrlf=false submodule update --init
stuart_update -c .\.pytool\CISettings.py
```

To omit the internal bulk-formatting revision from local blame output:

```powershell
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

The shared OneBranch template includes one Windows `SourceChecks` job for both
PR and Official builds. All three selected checks are enforced.
It uses the existing NuGet, pip, and ADO repository authentication and always
removes the repository authentication afterward. Job-level `GIT_CONFIG_*`
variables disable automatic line-ending conversion and enable long paths for
Git processes, including checkout, without changing the agent's global Git configuration. The job uses
the container's Python, as existing OneBranch jobs do; GitHub pins Python 3.12.

The settings select only the `cibuild` scope, `MsvmPkg`, `NO-TARGET`, and both
X64/AARCH64. All plugins are disabled except GuidCheck, LineEndingCheck, and
UncrustifyCheck. The workspace root is a package search root so internal sibling
packages participate in GUID comparisons; only MsvmPkg is selected for checks.
The upstream GUID exception list is unchanged. Source formatting and DSC
line-ending normalization landed in the preceding formatting PR. The package's
`.gitattributes` pins text files to CRLF on every platform. Package search paths
are workspace-relative and include Common/PATINA_EDK2, matching current upstream.
Including Patina in source-check setup does not enable Patina firmware builds.

The pinned GuidCheck and LineEndingCheck plugins report failures directly.
UncrustifyCheck has `AuditOnly: false`, so formatting findings also fail the
local command and JUnit test rather than being reported as skipped.

The Stuart invocation has no `continueOnError` override and test publication
uses `failTaskOnFailedTests: true`. Setup/update/check errors propagate as
nonzero exits and fail `SourceChecks`. Feed/repository authentication and
cleanup task failures are not suppressed. Each firmware matrix job depends on
`SourceChecks` succeeding, preventing both builds and their vpack/NuGet
publication after a failed check. This is the OneBranch counterpart to
upstream's release-job dependency; OneBranch packages inside each matrix job.

The check job does not publish NuGet or vpack packages. It publishes
`Build/TestSuites.xml` to ADO Test Results and stages the CI/setup/update logs and
`Build/MsvmPkg` reports even when checks fail. Log staging is best-effort so a
missing `Build/` after a setup failure does not add another blocking error.

At the pinned MU_BASECORE revision, Uncrustify is the SHA-256-pinned
`tianocore-uncrustify-release` 73.0.11 ZIP from GitHub, not a Project Mu NuGet
feed. Governed-container egress and Component Governance approval for the
CI-scoped binary dependencies still need confirmation; enabling enforcement
does not establish approval. No internal mirror is configured.

See [Running CI Checks Locally](../../../../MsvmPkg/Docs/BUILDING.md#running-ci-checks-locally)
for individual check commands and in-place formatting. That shared guide retains
upstream's GitHub Actions wording for mirroring; this repository enforces the
same checks through OneBranch as described here.

Local tests validate orchestration, failure propagation, and enforcement policy.
`verify_ci.py` validates YAML parsing and the Python scripts, but cannot expand
the governed OneBranch template. Queue an ADO PR build to validate checkout
environment propagation, private feed/submodule access, results publication,
and firmware jobs being skipped after a source-check failure on a hosted runner.
