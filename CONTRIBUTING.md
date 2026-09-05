# Contributing to OpenMobile

Bug fixes and example projects are welcome. If something breaks on your phone, please report it, even if you haven't found the cause.

If you're planning a larger change, please open an issue first so we can agree on the approach.

## Reporting a bug

Include the plugin and Unreal versions, your device and OS version, and the steps that reproduce the problem. Explain what you expected and what happened instead.

A small example project or a screenshot of the Blueprint graph helps. For errors, include the log lines around the failure.

## Working on the code

Start with Unreal Engine 5.8 and the plugin dependencies listed in [Getting started](README.md#getting-started).

- Keep each pull request focused on one change.
- Follow the existing C++ style and naming. Use comments to explain decisions that aren't obvious from the code.
- Keep Android, iOS, and vendor SDK code in their own modules. Public headers should use Unreal types.
- Declare plugin dependencies in the `.uplugin` file and module dependencies in `Build.cs`.
- Give Blueprint nodes clear names and useful pins. Return an error that explains what failed. Handle cancellation and what happens when a level or app closes.
- Put new Project Settings under OpenMobile, with a distinct section for the plugin. Keep the section and display names in sync and cover them with an automation test.

## Checking your change

For a bug fix, add a small test that fails because of the bug, then make it pass. Run the tests for the code you changed.

For native changes, build for the affected platform and try it on a phone when you can. Include the engine version, platform, and device you tested. If you only checked in the editor or couldn't test a platform, say so in the pull request.

## Sending a pull request

Open the pull request against `main`. Explain what you changed and how you checked it, and link the issue if there is one. Keep commit messages short and plain.

Contributions use the project's [MIT license](LICENSE). Keep existing third-party license notices with their code and SDK files.
