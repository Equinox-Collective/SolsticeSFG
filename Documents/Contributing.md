# Contributing to SolsticeSFG

Read these rules carefully before opening a pull request.

## 1. AI Code and Responsibility
You can use AI/LLM tools to write code, documentation, or anything related to this project. You own every single bug it creates. If the code corrupts memory, leaks resources, causes undefined behavior, or crashes, you take full responsibility. Know and verify every single line you submit.

## 2. Branching Strategy
The repository uses two main branches:
* `master` — **Stable releases only.** Direct pull requests to `master` are not allowed and will be rejected.
* `dev` — **Active development branch.** Always branch off `dev` and target your pull requests into `dev`.

## 3. Sign Off Your Commits
Sign off every commit using the `-s` flag (Developer Certificate of Origin):

```sh
git commit -s -m "your commit message"
```

Pull requests with unsigned commits will be rejected.

## 4. Code Style and Amalgamation Rules
* Follow the project's `.clang-format` and `.editorconfig` strictly (C99 standard, 4 spaces, K&R braces).
* All code must compile cleanly with `-std=c99 -Wall -Wextra -Werror -pedantic`.
* Do not edit generated single-header files in `dist/` manually; edit modular sources in `include/` and `src/` instead.
* Keep internal symbols properly prefixed or `static` so amalgamation into a single translation unit never causes symbol collisions.

## 5. Test Everything Before Submitting
Build both the modular library and the amalgamated single-header output. Run the full test suite in `tests/` and confirm all tests pass (ideally under AddressSanitizer / UndefinedBehaviorSanitizer as well). Submitting code that does not build, breaks amalgamation, introduces warnings, or lacks tests in `tests/` for new behavior will get your pull request closed on the spot.

## 6. Describe Your Changes
Write a clear description for your pull request. State what was changed, why you changed it, and how to test it. Empty pull request descriptions will be closed.

## 7. Keep Pull Requests Small
One pull request covers one bug fix, one feature, or one cleanup task. Keep diffs focused. Do not drop massive PRs that rewrite the entire codebase at once.

## 8. License
SolsticeSFG is dual-licensed under the **zlib License** OR the **MIT License**. By submitting a contribution, you agree to dual-license your work under both the zlib License and the MIT License (`Zlib OR MIT`).
