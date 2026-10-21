---
name: skill-reflection
description: "Use at the start and end of a gameengine2 session. Read each loaded skill's .lessons/ first; at the end record what the session learned that the skill does not say, prune stale lessons, and promote a lesson that keeps recurring into the SKILL.md itself."
---

# Skill reflection

Each skill has a `.lessons/` directory with one Markdown file per lesson:
`YYYY-MM-DD-<slug>.md`, starting with front matter:

```markdown
---
skill: determinism
scope: libm
---
What happened, what the skill did not say, and the rule that follows.
```

## At the start

For each skill you load: `ls .agents/skills/<skill>/.lessons/`, then read the lessons
whose names touch your task.

## At the end

- Record a lesson only if it is **new and would change what the next agent does**.
  A trap, a measurement, a wrong turn and its fix. Never record a diary entry.
- Merge it into an existing lesson rather than adding a near-duplicate.
- Delete a lesson the code or the skill has made obsolete.
- When a lesson has applied more than twice, **promote it**: write it into the
  SKILL.md and delete the lesson.
- Keep a lesson under 25 lines. Lessons are public like everything else.
