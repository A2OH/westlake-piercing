---
name: no-co-author-trailer-in-commits
description: "Do NOT add `Co-Authored-By: Claude` trailer to git commits. User explicit preference 2026-05-21."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

**Do NOT add `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>` (or any Claude/Anthropic co-author trailer) to git commit messages.**

User explicit preference confirmed 2026-05-21. Applies to:
- All future commits in any repo for this user (`westlake <[REDACTED-EMAIL]>` / [REDACTED-EMAIL])
- All agent-authored commits dispatched in future sessions
- Both this orchestrator and any dispatched sub-agent

## Why

User preference. They don't want the Co-Authored-By trailer on commits.

## How to apply

- When constructing commit messages via `git commit -m "..."` or HEREDOC: stop before adding any "Co-Authored-By:" line
- When briefing dispatched agents to commit, explicitly include in the brief: "DO NOT add Co-Authored-By trailer per user preference"
- For existing commits (already in history with the trailer): leave as-is unless user explicitly asks to rewrite history (force-push is destructive)

## Exceptions

If user explicitly says "add a co-author" or names a specific co-author for a specific commit, honor that. Default is no trailer.

## Cross-references

- Existing commit history before 2026-05-21 has the trailer — leave alone unless rewrite-history is explicitly requested
- The Co-Authored-By trailer was added by my own commit conventions (CLAUDE.md guidance) but user preference supersedes
