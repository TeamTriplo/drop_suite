# DropTasks

Personal task management for Backdrop CMS. A simple, focused task list with
priorities, tags, due dates, and per-person ownership.

## Features

- **Task entity** (`drop_task`) — title, status, priority, due date, owner
- **Task list** (`drop/tasks`) — filterable list of pending and completed tasks
- **Tags** — shared `drop_tags` taxonomy vocabulary (also used by DropTickets)
  with Field UI support
- **Priorities** — 5-level scale via drop_base (None, Low, Normal, High, Critical)
- **Owner field** — admin can assign tasks to other team members
- **Per-person scoping** — people see only their own tasks unless they have
  admin permissions

## Pages

- `drop/tasks` — task list with status/priority filters
- Task add/edit forms

## Dependencies

- drop_base

## Permissions

- `administer droptasks` — full access to all tasks
- `access droptasks` — view own tasks
- `manage droptasks` — create and edit own tasks
