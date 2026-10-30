# Git work and branch consolidation

Use this guide when creating commits or integrating branches. The shared
[workflow](workflow.md) owns task authorization, progression and shared-tree
safety. A review of branches remains read-only until integration is requested.

## Create coherent commits

Complete and commit coherent development units independently after the relevant
permitted checks. Commit only task-related changes that can be separated cleanly
from pre-existing work. Inspect status, affected diffs and the staged diff before
committing; do not stage unrelated or unfinished user changes.

If task work and pre-existing work cannot be separated safely, leave the overlap
uncommitted and explain the boundary. Do not rewrite the user's work to manufacture
a clean commit. A commit records the completed source change; state any outstanding
executable acceptance separately.

For agent-created commits, supply this identity on the Git command:

```sh
git -c user.name=cppcooper \
    -c user.email=cppcooper@users.noreply.github.com \
    commit --file /path/to/reviewed-commit-message
```

Do not change global or repository Git configuration. The message begins with a
third-person singular present-tense verb, such as `Adds`, `Updates`, `Revises`,
`Deletes`, `Fixes` or `Requires`. Use a detailed body appropriate to the development
unit: the resulting behavior, affected contracts and meaningful verification
limits. Preserve actual newlines when passing a message through a file or a
structured tool argument.

Preserve individual completed commits normally. Do not amend, squash or otherwise
rewrite them without an explicit request. Do not generate mailbox patches or
push commits/branches to a remote unless explicitly requested. A request to merge
locally does not authorize a push or branch deletion.

## Consolidate branches into fixes-develop

The established policy for a user request to merge completed branches down into
`fixes-develop` is a **squash consolidation**, with every integrated original
commit's full message retained in the aggregate commit body. The user explicitly
confirmed this strategy; it is the exception to the normal individual-commit rule
for this destination. Do not apply it automatically to another destination or to
an unrelated integration request.

The consolidation creates a new aggregate commit on `fixes-develop`. Keep the
original commits and source branch refs; do not rebase, amend, reset or delete
them as part of this operation. `develop` and `main` remain unchanged unless the
user separately requests their update.

### Establish the integration set

1. Inspect status and relevant diffs. Require a clean index and working tree for
   the consolidation; resolve or safely separate authorized task work first.
   Do not discard or automatically stash another person's work.
2. Inspect local branch tips, tracking refs and worktrees. Record the full
   destination/source commit IDs before switching branches. Preserve active
   worktrees and their outputs; do not force a checkout of a branch used elsewhere.
3. Check ancestry and divergence, including whether `develop` and `main` are
   already ancestors. A detached review worktree is not another branch to merge.
   Local tracking refs describe the last fetched state; do not claim they prove
   the current remote state.
4. Determine the distinct commits being integrated relative to the destination.
   For stacked branches, count each shared ancestor once. Do not concatenate a
   separate overlapping log for every branch. The union from
   `git rev-list --reverse --topo-order <source-tips> --not <destination>` identifies
   distinct reachable commits; retain their full messages, not just subjects.
5. If the stack is linear and all selected branch tips are ancestors of one final
   tip, integrate that final tip once. This includes the intermediate branches
   without repeated squash operations. Include an authorized user commit at that
   tip as part of the requested branch contents; do not silently drop it.

An older squash may represent changes without preserving ancestry from its
source branch. Establish whether those changes are already integrated rather
than replaying a historical snapshot merely because it is not an ancestor.
Resolve routine integration details within the requested scope; seek direction
when source histories or conflicting requirements leave a material choice unclear.

### Produce and verify the aggregate

Prepare the complete message in a temporary file before integrating. Start with a
summary such as `Merges completed branches into fixes-develop (squash)`, then include
each distinct integrated commit's full message in chronological/topological order.
Commit IDs may identify the entries. Preserve their text and original message
bodies; do not replace them with a newly written summary of the work.

Switch to `fixes-develop`, squash the reviewed integration tip or tips, and inspect
the staged result before creating the aggregate with the identity above. Do not
treat a merge conflict as permission to overwrite pre-existing work or silently
choose between incompatible contracts.

For a linear stack whose destination is an ancestor of the selected final tip,
the committed aggregate tree must equal that tip's tree. Compare their tree IDs
or use `git diff --exit-code <reviewed-tip> fixes-develop`, then confirm clean
status and that the source refs still point to their recorded commits. Squashing
does not make those original commit IDs ancestors of the destination, so
`git branch --merged` alone cannot establish consolidation success.

An unchanged final tree does not require another native build merely because the
commit shape changed. Report the tree comparison and relevant prior verification
accurately. If conflict resolution changes content, assess the affected contracts
and run the smallest checks permitted by the [testing instructions](testing.md);
do not infer executable acceptance from a clean Git merge.

Report the new destination commit, source tips included, preserved original refs,
tree-equivalence result and any unresolved verification. Keep the result local
unless the user requested a push.
