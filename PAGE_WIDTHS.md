# Drop Suite page widths

Reference list of which `drop/*` pages use the narrow wrapper vs the wide
wrapper. Use this to verify "did this page get wider/narrower?" questions
against intended behavior.

## How widths are controlled

- **Narrow pages** get the body class `drop-narrow-page` and a configurable
  `max-width` on `.l-wrapper` (default 1000px, set at `drop/settings/base`).
  The list of narrow paths is driven by the regex patterns in
  `_drop_suite_is_narrow_page()` in `drop_suite.module`.
- **Wide pages** use `.l-wrapper-inner { max-width: var(--drop-page-width, 1600px) }`,
  which reads from a per-user body class:
  - `drop-page-width-narrow` — 1200px
  - default — 1600px
  - `drop-page-width-wide` — 1920px
  - `drop-page-width-full` — 95vw
- **Primary nav bar** is independent of both. It's full-bleed via
  `.layout--drop-suite .block-drop-suite-drop-nav { margin: 0 calc(-50vw + 50%) }`
  with the inner `.drop-nav` capped at 1920px, so the nav width stays
  constant regardless of the viewer's page-width preference.

## Narrow pages (1000px default, configurable)

| Area | Paths |
|---|---|
| Tickets | `drop/tickets/{tid}` · `drop/tickets/add` · `drop/tickets/{tid}/edit` |
| CRM contact | `drop/crm/contact/{cid}` · `drop/crm/contacts/manage/crm_*/add` · `drop/crm/contacts/manage/{cid}/edit` · `drop/crm/contacts/{cid}/edit` |
| CRM activity | `drop/crm/activity/{aid}` · `drop/crm/activities/manage/add` · `drop/crm/activities/manage/{aid}/edit` |
| CRM contact method | `drop/crm/contact-methods/{cmid}` |
| Tasks | `drop/tasks/{tid}` · `drop/tasks/add` · `drop/tasks/{tid}/edit` |
| Goals | `drop/goals` (list) · `drop/goals/manage/{gid}` · `drop/goals/manage/add` · `drop/goals/manage/{gid}/edit` |
| Time | `drop/time/entry/{tid}/edit` |
| Docs | `drop/docs` (list) · `drop/doc/{did}` |
| Projects | `drop/projects/{pid}` · `drop/projects/add` · `drop/projects/{pid}/edit` |
| Files | `drop/files` (library index) · `drop/files/upload` · `drop/files/{fid}/view` · `drop/files/{fid}/edit` · `drop/files/{fid}/replace` · `drop/files/{fid}/delete` |
| Updates | `drop/updates` (feed) · `drop/updates/add` · `drop/updates/{uid}/edit` · `drop/updates/{uid}/delete` |
| Meeting | `drop/meeting` |
| Settings (all) | `drop/settings*` · `drop/tickets/settings*` · `drop/crm/settings*` · `drop/monitor/settings*` · `drop/docs/settings*` · `drop/time/settings*` · `drop/files/settings*` |
| Setup wizard | `drop/start*` (overview + all wizard steps) |

## Wide pages (per-user `--drop-page-width`, default 1600px)

Anything under `drop/*` NOT matched by the narrow patterns above. Notable
examples:

- `drop` (home dashboard)
- `drop/tickets` (list) · `drop/tickets/kanban` · `drop/tickets/reports/*`
- `drop/crm` · `drop/crm/contacts` · `drop/crm/activities` · `drop/crm/reports/*`
- `drop/tasks` (list) · `drop/tasks/kanban` · `drop/tasks/reports/*`
- `drop/time` (list, reports)
- `drop/projects` (list)
- `drop/monitor/*` (dashboards)
- `drop/finance/*`
- `drop/calendar`

## Changing the classification

The single source of truth is the `$patterns` array in
`_drop_suite_is_narrow_page()` in `drop_suite.module`. Add a regex to make a
path narrow; remove it to let it fall back to wide. Update this file when
the classification changes.
