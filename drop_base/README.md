# Drop Base

Shared foundation module for the Drop Suite. Provides CSS custom properties,
utility classes, PHP helpers, the Drop navigation bar, and the team directory.

Every other Drop module depends on drop_base (directly or transitively).

## Features

- **CSS custom properties** for priority colors, due-date colors, and shared
  design tokens — loaded globally via `hook_init()`
- **Utility CSS classes**: `.drop-priority-high`, `.drop-overdue`,
  `.drop-due-soon`, `.drop-page-actions`, `.drop-section-header`, etc.
- **PHP helpers**:
  - `drop_base_priority_labels()` — 5-level priority array (None, Low, Normal, High, Critical)
  - `drop_base_priority_class($priority)` — returns CSS class suffix
  - `drop_base_section_header($title, $add_link)` — section heading markup
  - `drop_base_page_actions(array $links)` — action bar renderer (pill-shaped links)
- **Drop navigation bar** — aggregates links from all enabled Drop modules via
  `hook_drop_nav_links()`
- **Team directory** at `drop/team` — lists active team members with last-seen
  time; links to per-member ticket pages when DropTickets is enabled

## Dependencies

None.

## Permissions

- `access drop team` — view the team directory page

## Installation

Enable this module before any other Drop module. Place the **Drop Nav** block
in your site's layout to display the shared navigation bar.
