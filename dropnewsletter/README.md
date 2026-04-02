# Drop Newsletter

Integrates DropCRM contacts with SimpleNews newsletter subscriptions. Adds
a newsletter management section to the CRM contact view page.

## Features

- **Newsletter section on contacts** — appears in the contact view accordion,
  showing subscription status for all available newsletters
- **Subscribe/unsubscribe** — admin can toggle subscriptions for any contact
  that has a primary email address
- **Immediate confirmation** — subscriptions are confirmed without sending an
  opt-in email (admin-initiated action)
- **Primary email lookup** — uses the contact's primary `crm_contact_method`
  of type "email"

## Dependencies

- dropcrm
- simplenews

## How it works

Drop Newsletter hooks into the contact view page via
`hook_dropcrm_contact_view_alter()` and injects a form showing all available
SimpleNews newsletters with the contact's current subscription status. Changes
are applied immediately using SimpleNews's subscriber API.

No custom database tables are created — all subscription data is managed by
SimpleNews.
