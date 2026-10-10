Never commit directly to main.
Keep main tracking upstream exactly.

Create a branch such as:

```
main           ← identical to upstream
my-changes     ← your commits
```

Then updating becomes:

```Shell
git checkout main
git fetch upstream
git reset --hard upstream/main
git checkout my-changes
git rebase main
```
