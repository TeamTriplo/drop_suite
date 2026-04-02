# DropBridge

Integration hub for the Drop Suite. Provides a unified daily dashboard,
Slack DM notifications, and inbound email-to-ticket processing.

## Features

### Unified dashboard (`drop/`)

Pulls today's items from all enabled Drop modules:
- **DropGoals** — incomplete goals due today
- **DropTasks** — pending tasks due today or overdue
- **DropCRM** — planned activities with due dates
- **DropTickets** — assigned/owned tickets

Items are sorted: overdue first, then due today. Type badges distinguish
the source (Goal, Task, Activity, Ticket). "Done" actions redirect back to
the dashboard.

### Slack integration

- `dropbridge_notify($uid, $message)` — central notification dispatcher
- Time entry reminders via `hook_cron()` — reminds team members who haven't
  logged time, respecting per-user work schedules and timezones
- Debug mode for testing notifications without schedule restrictions
- Per-user preferences: Slack member ID, work days, work hours, reminder offsets

### Inbound email (`drop/settings/inbound`)

Processes SendGrid Inbound Parse webhooks to create or update tickets from
incoming email:
- Reply threading via `[DROP-{pid}-{num}]` subject tags
- Sender allowlist with verified/unverified ticket handling
- Automatic reply chain stripping
- Forwarding copies to a configured support address

## Dependencies

- drop_base (required)
- dropcrm, droptasks, dropgoals, droptickets, droptime (all optional — checked via `module_exists()`)

## Permissions

- `access dropbridge` — view the unified daily dashboard

## Configuration

- `drop/settings` — Slack bot token, debug mode
- `drop/settings/inbound` — inbound email toggle, webhook token, allowlist, default project
- Per-user preferences are configured on each person's Backdrop account edit page
