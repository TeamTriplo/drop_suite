# Drop Suite

A modular productivity and CRM platform for [Backdrop CMS](https://backdropcms.org).

Drop Suite is a collection of loosely-coupled Backdrop modules for managing
contacts, projects, tickets, time, tasks, goals, documentation, and team
communication. Each module can be enabled independently (respecting
dependencies), so you can start with just what you need and add capabilities
over time.

![Drop Suite Architecture](drop_suite_diagram.png)

## Modules

| Module | Description | Path | Dependencies |
|--------|-------------|------|--------------|
| [drop_base](drop_base/) | Shared CSS, helpers, nav bar, team directory | — | *none* |
| [DropBridge](dropbridge/) | Unified daily dashboard, Slack DMs, inbound email | `drop/` | drop_base |
| [DropCRM](dropcrm/) | Contacts, activities, projects, contact methods | `drop/crm` | drop_base, entity_plus, entity_ui, entityreference |
| [DropDocs](dropdocs/) | Hierarchical team wiki / documentation | `drop/docs` | drop_base |
| [DropEmail](dropemail/) | Outbound email from contacts & tickets, activity logging | — | dropcrm |
| [DropGoals](dropgoals/) | Daily habit / goal tracking with flexible frequency | `drop/goals` | drop_base |
| [DropNewsletter](dropnewsletter/) | Newsletter subscriptions on CRM contacts | — | dropcrm, simplenews |
| [DropStrategic](dropstrategic/) | OKR-style strategic goal tracking | `drop/strategic` | drop_base, entity_plus |
| [DropTasks](droptasks/) | Personal task management with tags and priorities | `drop/tasks` | drop_base |
| [DropTickets](droptickets/) | Team ticket tracker with kanban, watchers, Slack | `drop/tickets` | drop_base, dropcrm, entity_plus |
| [DropTime](droptime/) | Project time tracking, timesheets, reports | `drop/time` | drop_base, dropcrm |

### Architecture at a glance

- **drop_base** is the foundation. Every other Drop module depends on it (directly or transitively). It provides shared CSS custom properties, priority helpers, the Drop navigation bar, and the team directory.
- **DropCRM** is the core data module. It defines contacts, activities, contact methods, and the `drop_project` entity used by DropTickets and DropTime.
- **DropBridge** is the integration hub. It pulls from DropCRM, DropTasks, DropGoals, DropTickets, and DropTime via `module_exists()` checks — all are optional.
- **DropEmail**, **DropNewsletter**, and **DropTime** extend DropCRM with email, newsletter, and time tracking capabilities.
- **DropTasks**, **DropGoals**, **DropDocs**, and **DropStrategic** are standalone modules that depend only on drop_base (and entity_plus where needed).

## Requirements

- Backdrop CMS 1.x
- [Entity Plus](https://github.com/backdrop-contrib/entity_plus) (for DropCRM, DropTickets, DropStrategic)
- [Entity UI](https://github.com/backdrop-contrib/entity_ui) (for DropCRM)
- [Entity Reference](https://github.com/backdrop-contrib/entityreference) (for DropCRM)
- [SimpleNews](https://github.com/backdrop-contrib/simplenews) (only for DropNewsletter)

## Installation

1. Place the entire `drop/` directory in your site's `modules/` folder (e.g. `modules/drop/`).
2. Enable **drop_base** first, then enable whichever modules you need. Backdrop will resolve dependencies automatically.
3. Place the **Drop Nav** block in your layout to get the shared navigation bar across all Drop pages.
4. Grant permissions under **Administration > Configuration > People > Permissions**. Key roles:
   - `team_member` — basic access to tickets, tasks, time, and the team directory
   - `project_manager` — administrative access to tickets (flag, close, delete)

## Quick start

For a minimal CRM setup:
- Enable: `drop_base`, `dropcrm`

For a full team productivity suite:
- Enable: `drop_base`, `dropcrm`, `droptickets`, `droptime`, `droptasks`, `dropgoals`, `dropbridge`

For documentation and strategic planning:
- Add: `dropdocs`, `dropstrategic`

## Permissions overview

Each module defines its own permissions. Common patterns:

- `access <module>` — view and use basic features
- `administer <module>` — full administrative access
- Module-specific permissions (e.g. `flag droptickets`, `choose billable status`)

## Configuration

Most modules store configuration in Backdrop's config system (JSON files in `config/`). Settings pages are available at:

- **DropCRM**: `drop/crm/settings`
- **DropTickets**: `drop/tickets/settings`
- **DropBridge**: `drop/settings` (Slack, inbound email)
- **DropEmail**: `drop/admin/dropemail`

## License

This project is GPL v2 compatible, as required by Backdrop CMS.
