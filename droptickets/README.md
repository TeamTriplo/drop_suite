# DropTickets

Lightweight ticket tracking for small teams. Per-project ticket numbering,
kanban board, watcher notifications, and Slack integration.

## Features

### Tickets

- **Per-project numbering** — tickets get labels like `PROJ-42` based on
  project prefix
- **Types**: bug, feature, task, improvement, discussion
- **Statuses**: icebox, open, to_do, in_progress, in_review, blocked, done, closed
- **5-level priority** with colored dot indicators
- **Owner** (long-term responsibility) + **Assignee** with three modes:
  - `user` — specific team member
  - `team` — "The Team" (green badge)
  - `on_hold` — "On Hold" (orange badge)
- **Due dates** with overdue/due-soon highlighting
- **File attachments** (private files, multi-value, with descriptions)
- **Tags** via shared `drop_tags` vocabulary
- **Collapsible body** with expand/collapse toggle
- **Emoji reactions** on tickets and comments
- **Image lightbox** — click any image in ticket/comment body for fullscreen
  view with zoom

### Views and reports

- **Ticket list** with status tabs, search, project filter, tag filter pills
- **Kanban board** (`drop/tickets/kanban`) — 5-lane drag-free board with
  project filter
- **Per-member page** (`drop/tickets/member/{uid}`) — owned and assigned tickets
- **Stale tickets report** — configurable staleness threshold
- **Flagged tickets** — PM-set priority flags visible to the whole team
- **Discussion tickets** — separate tab for team discussions

### Notifications

- **Watchers** — per-ticket subscriber list
- **Bell icon** with unread count badge
- **Notification page** listing tickets with unread activity
- **Auto-purge** via cron with configurable lifetime

### Slack integration

- New ticket notifications with project and assignee info
- Reassignment notifications on ticket update
- Silence checkbox on create form (when webhook is configured)
- Emoji reaction notifications (throttled: 1 per actor per ticket per 5 min)

### Project Mode

Context-preserving navigation — when viewing tickets from a report page,
"Back to [report]" links maintain the filter context.

## Dependencies

- drop_base
- dropcrm (for `drop_project` entity)
- entity_plus

## Permissions

- `access droptickets` — view and create tickets
- `administer droptickets` — full access to all tickets
- `flag droptickets` — flag/unflag tickets as priority (project_manager role)
- `close droptickets` — mark tickets as done (project_manager role)
- `delete droptickets` — delete tickets (project_manager role)

## Roles

- `team_member` — basic ticket access; appears in assignee dropdowns
- `project_manager` — administrative ticket access (flag, close, delete)

## Configuration

- `drop/tickets/settings` — notification lifetime, stale days, flag limits,
  default watchers, emoji reactions toggle, Slack webhook URL

## Experimental features (via Drop Labs)

Some features are gated on Drop Labs and off by default. Install the
`drop_labs` module to expose the toggle UI at `drop/settings/labs`.

- **CSV import (JIRA format)** — adds `drop/tickets/import` for importing
  JIRA CSV exports. Maps only Summary → title and Description → body; does
  not import status, priority, assignee, dates, comments, or attachments.
  All rows land as open tasks at priority 3.
