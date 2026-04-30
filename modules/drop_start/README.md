# Drop Start

Onboarding module for Drop Suite sites. Provides two things:

1. A **setup wizard** that walks a new site admin through the essentials
   (theme, sample content, modules, home-page navigation) one step at a
   time.
2. A **sample content generator** that populates the site with an
   Olympics event-planning demo so there's something real to explore
   while evaluating Drop Suite.

## Setup wizard

Lives at `drop/start`. Renders one of three modes based on state:

- **Not started** — welcome splash with a Begin button.
- **In progress** — resume card pointing at the step you left off on.
- **Complete** — summary of configured state with per-step re-run links.

Each step is its own URL at `drop/start/step/{key}`, with universal
Continue / Skip / Exit actions. State is tracked in the Backdrop state
API (`drop_start_wizard`) as `{current, completed[], skipped[]}`.

### Steps shipped out of the box

| Key | Title | Notes |
|---|---|---|
| `welcome` | Welcome | Required. Short intro. |
| `theme` | Theme | Recommends Opera. Handles installed+active, installed+inactive, and missing-entirely states. |
| `sample_content` | Sample content | Wraps the legacy generator. |
| `modules` | Modules | Snapshots enabled Drop modules and links to `drop/settings/modules`. |
| `home_nav` | Home page navigation | Absorbed from drop_suite. Adds Drop Suite nav blocks to the home layout. |
| `done` | Done | Required. Summary + next-step links. |

Welcome and Done are required; the others can be skipped.

### Contributing a step from your own module

Implement `hook_drop_start_steps()`:

```php
function mymodule_drop_start_steps() {
  return array(
    'my_step' => array(
      'title'           => t('My step'),
      'weight'          => -30,
      'callback'        => 'mymodule_step_my_step',
      'status_callback' => 'mymodule_step_my_step_status',
      'description'     => t('One-line description for the overview card.'),
    ),
  );
}
```

The step's callback returns form elements (the wizard frame wraps them
with navigation + progress). An optional `{callback}_submit` runs before
the wizard advances state — set `$form_state['drop_start_no_advance']`
to keep the user on the same step (e.g. after a validation failure).

See `drop_start.api.php` for the full field reference and
`drop_start.wizard.inc` for working examples.

### Page widths

Wizard pages use the narrow page width (same as settings). Set via
`_drop_suite_is_narrow_page()` in drop_suite.

## Sample content generator

Independent of the wizard. Reachable directly at `drop/start/generate`
and `drop/start/delete`, or through the Sample content wizard step.
Creates an Olympics event-planning dataset across any enabled Drop
modules: projects, tickets, CRM contacts, tasks, goals, time entries,
docs. All generated items are tracked in `drop_start_content` so
`drop/start/delete` can remove them later without touching user content.

## Dependencies

- drop_suite (for the Settings dashboard hook and page-width helpers)

## Permissions

- `administer drop_start` — run the wizard, generate or delete sample
  content. Granted to admin roles via
  `drop_suite_grant_admin_permissions()` on install.

## Post-install

Emits a `backdrop_set_message()` on install pointing at the wizard so
fresh admins see the entry point immediately.
