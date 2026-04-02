# DropGoals

Daily goal and habit tracking for Backdrop CMS. Define recurring goals with
flexible frequency options and track daily completion with stats and streaks.

## Features

- **Goal definitions** — create goals with label, description, and frequency
- **Flexible frequency**:
  - Daily — every day
  - Every other day — alternates based on creation date
  - Specific days — pick which days of the week (Mon-Sun)
  - Weekly — once per Mon-Sun period
  - Monthly — once per calendar month
- **Daily dashboard** (`drop/goals`) — check off today's goals with one click
- **Completion tracking** — per-goal history with rolling completion rate
  (30-day window once enough data exists)
- **Goal management** (`drop/goals/manage`) — create, edit, activate/deactivate
- **Goal history** (`drop/goals/manage/{gid}`) — 5-week history grid and stats

## Entities

- `drop_goal` — goal definition (label, frequency, days_of_week, active, owner)
- `drop_goal_log` — daily completion records (one per goal per day)

## Dependencies

- drop_base

## Permissions

- `administer dropgoals` — full access to all goals
- `access dropgoals` — view own goals and dashboard
- `manage dropgoals` — create and edit own goals
