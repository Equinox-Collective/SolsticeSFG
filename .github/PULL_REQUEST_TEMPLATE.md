## Summary
<!-- Briefly describe what was changed and why. -->

## Related Issue
<!-- Link to the issue if applicable, e.g., Closes #12 -->

## How Was This Tested?
<!-- Describe the tests added or updated in `tests/`, and which compilers/platforms you tested on. -->

## Checklist
- [ ] Target branch is `dev` (PRs targeting `master` will be rejected).
- [ ] All commits are signed off (`git commit -s`).
- [ ] Code adheres to `.clang-format` and `.editorconfig`.
- [ ] Compiles with **zero warnings** under `-std=c99 -Wall -Wextra -Werror -pedantic`.
- [ ] Both modular (`src/*.c`) and amalgamated (`dist/solsticesfg.h`) builds succeed.
- [ ] Added/updated unit or pixel tests in `tests/`, and all tests pass.
- [ ] If AI/LLM tools were used, I manually reviewed every line and take full responsibility for memory safety and correctness.
- [ ] I agree to license my contribution under the project's dual license (`Zlib OR MIT`).
