# Contributing to OpenMobile

Bug reports, fixes, examples, and testing on real phones are all welcome. You don't need to take on a big feature to help.

For a larger change, open an issue first so we can talk through the approach before you spend time on it.

## Reporting a bug

Include the plugin and Unreal versions, your device and OS version, and the steps that reproduce the problem. Explain what you expected and what happened instead.

A small example project or a screenshot of the Blueprint graph helps. Include the relevant error output rather than a full build log.

## Working on the code

Start with Unreal Engine 5.8 and the plugin dependencies listed in [Getting started](README.md#getting-started).

- Keep each pull request focused on one change.
- Follow the existing C++ style and naming. Add comments when the reason for the code would otherwise be hard to see.
- Keep Android, iOS, and vendor SDK code in their own modules. Public headers should use Unreal types.
- Declare plugin dependencies in the `.uplugin` file and module dependencies in `Build.cs`.
- Give Blueprint nodes clear names, useful pins, and errors people can act on. Consider cancellation and what happens when a level or app closes.
- Put new Project Settings under OpenMobile, with a distinct section for the plugin. Keep the section and display names in sync and cover them with an automation test.

## Checking your change

For a bug fix, start with the smallest test that demonstrates the problem. Fix it, then run the tests relevant to the code you touched. A small contract test is usually enough.

For native changes, build the affected platform and try the behavior on a real device when you can. Say which engine version, platform, and device you checked, and what you could not check. Please distinguish an editor check from a device test.

## Sending a pull request

Open it against `main`. Explain the problem, what changed, and how you checked it. Use a short, plain commit message and include any related issue.

Contributions use the project's [MIT license](LICENSE). Keep existing third-party license notices with their code and SDK files.
