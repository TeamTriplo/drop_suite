# DropCRM

A lightweight, flexible CRM for Backdrop CMS. Tracks contacts, activities,
contact methods, and projects with full entity and field support.

## Features

### Contacts (`drop/crm`)

Three contact types (bundles):
- **Person** — individual people
- **Organization** — companies, groups
- **Household** — family units

Contacts are fieldable entities with inline entity forms for contact methods.
Rating field, attendee tracking, and organization relationships are built in.

### Contact methods

Separate entity for structured contact data:
- Email addresses
- Phone numbers
- Physical addresses

Each contact can have multiple methods with a primary flag.

### Activities (`drop/crm/activity/{aid}`)

Log interactions and plan future ones:
- Types: meeting, phone call, email, note, and more
- Direction: inbound/outbound
- Status: planned, completed, cancelled
- Linked to one or more contacts via entity reference
- Due dates for planned activities

### Projects

The `drop_project` entity is defined here and shared with DropTickets and
DropTime. Projects include:
- Title, color coding, active/archived status
- Optional client contact link
- Prefix for ticket numbering (e.g. PROJ-42)
- Per-project ticket counters

### Submodules

- **dropcrm_user** — bridges Backdrop user accounts to CRM contacts, allowing
  team members to have linked contact records

## Dependencies

- drop_base
- entity_plus
- entity_ui
- entityreference

## Permissions

- `access dropcrm` — view contacts and activities
- `create dropcrm` — create new contacts and activities
- `edit dropcrm` — edit existing records
- `delete dropcrm` — delete contacts and activities
- `administer dropcrm` — full administrative access and settings

## Configuration

- `drop/crm/settings` — CRM settings and field configuration
