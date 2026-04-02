# DropTime

Project time tracking for Backdrop CMS. Daily timesheets, billable hour
tracking, personal and team reports, and an admin entry reassignment tool.

## Features

- **Time entries** (`drop_time_entry`) — log time by project with start/end
  times, duration, notes, and billable flag
- **Daily timesheet** (`drop/time`) — log and review today's entries
- **Personal reports** (`drop/time/reports`) — time summaries for the current
  person
- **Team reports** (`drop/time/team`) — cross-team time reports (admin only)
- **Reassign entries** (`drop/time/reassign`) — admin tool to move time entries
  between accounts with date range filtering, preview, and confirmation step.
  Actions are logged to watchdog for audit trail.
- **Billable tracking** — optional billable checkbox controlled by a separate
  permission

## Entity

`drop_time_entry` with columns: project ID, user ID, entry date (YYYY-MM-DD),
start/end times (HH:MM), calculated duration in minutes, notes, billable flag.

## Dependencies

- drop_base
- dropcrm (for `drop_project` entity)

## Permissions

- `access droptime` — log and view own time entries
- `administer droptime` — full access, team reports, reassign tool
- `choose billable status` — shows the billable checkbox on the entry form
