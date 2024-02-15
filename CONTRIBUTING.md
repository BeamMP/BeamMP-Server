# Contributing to BeamMP-Server

Unlike other parts of BeamMP, the BeamMP-Server does not have any dependency to the BeamNG.drive game. 

To contribute *C++ code*, you'll need a MacOS, Linux or Windows PC, and intermediate to advanced knowledge of C++.
For reference, you should know be reasonably comfortable with the STL, the concept of RAII, templates, and generally know how to read & write post-C++17 code. To contribute anything else, you won't need most of this (though it'd be helpful to have some vocabulary about computers).

# Ways to Contribute

## Bug Reports

If you work with BeamMP-Server, either by simply using it, or even writing plugins for it, and you run into any issues, we definitely want to know about it. Please use [GitHub issues](https://github.com/BeamMP/BeamMP-Server/issues) and select the "Bug" template, read it, and fill it out accordingly.

## Bug Fixes

If you are interested in fixing bugs, check out the [GitHub issues](https://github.com/BeamMP/BeamMP-Server/issues). There, you can pick any issue that has nobody assigned to it. For example, some bugs which we definitely need some help with are marked with the "help wanted" tag.

Once you picked a bug, you need to reproduce it. Start by following the instructions in the bug report, and don't be afraid to ask for more information or clarification on the issue itself.

Refer to [getting started with the codebase](#getting-started-with-the-codebase) for more information on how to build the server. You can also ask on our [Discord server](https://discord.gg/beammp), or on IRC ([irc.libera.chat](https://web.libera.chat/), join `#beammp`).

## Features

If you want to add new features, please make an issue for it first or ask on our [Discord server](https://discord.gg/beammp), or on IRC ([irc.libera.chat](https://web.libera.chat/), join `#beammp`).

You need to make sure the feature isn't being worked on by someone else, and aligns with the vision we have for the server.

# Git Guidelines

**Read this carefully. Failing to follow these rules results in your changes not being accepted**. This applies for outside contributors, members of the BeamMP development team ("BeamMP Developers"), project owners, maintainers, frequent contributors, and literally everyone else. **It applies to everyone**.

## How to Commit

Commit messages **MUST** (mandatory):

- start with a **lower case action verb in present tense**, for example `add`, `fix`, `implement`, `refactor`, `remove`, `rename`. *Counter examples (these are bad): ~~`Fixed`, `fixing`, `added`, `removing`~~*.
- not have a first line much longer than 70 characters.
- explain briefly the changes made.
- reference the issue by number, if there is an issue the commit addresses, like so: `#<number>`. Example: `#123`.

If any of these are not followed, **your changes will not be accepted.**

Commit messages **SHOULD** (optional, "nice to have"):

- only address one "atomic" change.
- have an empty second line, and the subsequent lines explaining the changes in more detail (if more detail is available).

Commits may be squashed (via a Git "interactive rebase") in order to satisfy these rules, but history that is >1h old should not be rewritten if possible. Force pushes are ugly ;)

## Pulling, Merging

Do **NOT** pull with merge. This is the default git behavior for `git pull`, but creates ugly and unnecessary commit messages like `"merge origin/master into master"`. Instead, pull with rebase, for example via `git pull -r`. If you get conflicts, resolve them properly.

The only acceptable merge commits are those which actually merge functionally different branches into each other, for example for merging one feature branch into another.

## Workflow

### Making an issue and fixing it

1. Create an issue detailing the feature or bug.
2. Assign a milestone to the issue, or wait for a maintainer to add a milestone to your issue.
3. Fork the repository and base your work on the branch mentioned in the milestone attached to your issue (e.g. `v3.0.0 (develop)` -> `develop`).
4. Program your feature or bug fix.
5. Open a PR that references the issue by number in the format: `#12345`.
6. Someone will review your PR and merge it, or ask for changes.

### Fixing an existing issue

1. Fork the repository and base your work on the branch mentioned in the milestone attached to your issue (e.g. `v3.0.0 (develop)` -> `develop`).
2. Program your feature or bug fix.
3. Open a PR that references the issue by number in the format: `#12345`.
4. Someone will review your PR and merge it, or ask for changes.

## Branches

- `minor`: Minor releases, like `v1.2.3` -> `v1.3.0` or `v1.2.3` -> `v1.2.4`.
- `develop`: Major releases, like `v1.2.3` -> `v2.0.0`, and larger feature/minor releases.

## Unit tests & CI/CD

We use GitHub Actions, which runs our unit-tests. PR's which fail these tests, or even fail any of our actions (which run automatically), will not be merged and require further changes until they compile, link, and all tests pass properly. If you have issues with this, feel free to ask in our [Discord server](https://discord.gg/beammp), or on IRC ([irc.libera.chat](https://web.libera.chat/), join `#beammp`)

### What should I call by branch?

Keep branch names **unique**, **descriptive**, and **shorter than 30 characters**. Names must be in present-tense, such as `fix-xyz`, **not** ~~`fixing-xyz`~~.

We generally use *feature branches*, so we keep one branch per feature or fix.

For example:
- You want to fix issue number #123? You could call the branch `fix-123`.
- You want to add a feature described in issue #456? You could call the branch `implement-456`.
- You want to add a feature or fix a bug that has no issue? You should probably make an issue for it first! Or, if you're not ready for that yet, you could call it by the feature name or bug description, for example for a bug that makes cars disappear: `fix-disappearing-cars`.

## Pull Requests, Code Review

Once you are ready to show what you did, and get feedback on it, you open a Pull-Request on GitHub. Please make sure to pick the right branches, and a descriptive title. Mention any related issues with `#<issue number>`, for example `#123`.

Make sure to explain what the PR does, what it fixes, and what needs to still be done (if anything).

A BeamMP-Developer must review your code in detail, and leave a review. If this takes too long, feel free to @ a maintainer/developer, or leave another comment on the PR. It helps to say something like "Ready for review", for example.

# Getting Started with the Codebase

1. Look at current Pull-Requests, look at the git branches, or ask in our [Discord server](https://discord.gg/beammp), or on IRC ([irc.libera.chat](https://web.libera.chat/), join `#beammp`), in order to find out which branch you should work on to minimize conflict. See [this section on branches](#branches) for more info.
2. Fork the repository (with that branch) on GitHub. GitHub, by default, gives you only the `master` branch when forking, so make sure you fork with other branches enabled when you want to work on a branch that isn't master (it's a checkbox when you fork).
3. Clone the fork to your local machine.
4. Check out the branch you want to work on (see 1.).
5. Run `git submodule update --init --recursive`.
6. Make a new branch for your feature or fix from the branch you are on. You can do this via `git checkout -b <branch name>`. See [this section on branches](#branches) for more info on branch naming.
7. Install all dependencies. Those are usually listed in the `README.md` in the branch you're in, or, more reliably, in one of the files in `.github/workflows` (if you can read `yaml`).
8. Run CMake to configure the project. You can find tutorials on this online. You will want to tell CMake to build with `CMAKE_BUILD_TYPE=Debug`, for example by passing it to CMake via the commandline switch `-DCMAKE_BUILD_TYPE=Debug`. You may also want to turn off sentry by setting `SENTRY_BACKEND=none` (for example via commandline switch `-DSENTRY_BACKEND=none`). An example invocation on Linux with GNU make would be 
`cmake . -DCMAKE_BUILD_TYPE=Debug -DSENTRY_BACKEND=none` . 
9. Build the `BeamMP-Server` target to build the BeamMP-Server, or the `BeamMP-Server-tests` target to build the unit-tests (does not include a working server). In the example from 8. (on Linux), you could build with  `make BeamMP-Server`, `make -j BeamMP-Server` or `cmake --build . --parallel --target BeamMP-Server` . Or, on Windows, (in Visual Studio), you would just press some big green "run" or "debug" button.
10. When making changes, refer to [this section on how to commit properly](#how-to-commit). Not following those guidelines will result in your changes being rejected, so please take a look.
11. Make sure to add Unit-tests with `doctest` if you build new stuff. You can find examples all over the latest version of the codebase (search for `TEST_CASE`).

# Code Guidelines

## Formatting

1. Use `clang-format` to format your code before committig. A `.clang-format` file is provided in the root of the repository.
2. All identifiers, type names, function names, etc. should be `PascalCase`. Type names may also have the `T` prefix, although this is not enforced (for example `TNetwork`).

## Modular code

Write code that is modular and testable. Generally, if you can write a good unit-test for it, it's modular. If you can't, it's not.

Don't overdo it though - sometimes its okay to just write code, do the job, be done with it. You'll get feedback on this in the code review for your PR.
