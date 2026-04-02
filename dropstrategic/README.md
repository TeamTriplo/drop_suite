# Drop Strategic

Strategic goals and OKR tracking for Drop Suite teams. Track quarterly, annual,
or open-ended objectives with progress percentages and team/personal scoping.

## Features

- **Strategic goal entity** (`drop_goal_strategic`) with:
  - Title and rich-text body
  - Type: personal or team
  - Status: active, achieved, paused, abandoned
  - Horizon: quarterly, annual, or open-ended
  - Optional due date
  - Manual progress tracking (0-100%)
  - Private notes field
- **Dashboard** (`drop/strategic`) — all team and personal goals
- **My goals** — filtered view showing only the current person's goals
- **Add/edit/delete** goals with full form UI

## Pages

- `drop/strategic` — goals dashboard (default tab: all goals)
- `drop/strategic/mine` — personal goals only
- `drop/strategic/add` — create a new goal

## Dependencies

- drop_base
- entity_plus

## Permissions

- `administer dropstrategic` — create, edit, and delete any strategic goal
- `create strategic goals` — create personal strategic goals
- `view strategic goals` — view the goals dashboard
