# DropEmail

Outgoing email from CRM contacts and support tickets, with automatic CRM
activity logging and a correspondence log.

## Features

- **Email from contacts** — "Send email" button on contact pages (appears when
  the contact has a primary email address). Form at
  `drop/crm/contact/{cid}/email`
- **Email from tickets** — "Send email to client" action on ticket pages
  (appears when the ticket's project has a linked contact with email). Form at
  `drop/tickets/{tid}/email`
- **CRM activity logging** — every sent email automatically creates a
  `crm_activity` of type "email" linked to the contact
- **Email log** — `drop_email_log` table records all outgoing messages with
  sender, recipient, subject, body, status, and links to originating
  contact/ticket
- **Correspondence log on tickets** — email history displayed below the comment
  thread on ticket view pages
- **Email-to-comment integration** — outbound emails create ticket comments
  with `source = 'email_out'`; inbound replies (via DropBridge) create comments
  with `source = 'email_in'`. Both are visually distinguished:
  - Outbound: teal border + "Email sent" badge
  - Inbound: purple border + "Email received" badge

## From/Reply-To logic

- Default From address is configurable (e.g. `support@example.com`)
- If the sender overrides From with their personal email, the default support
  address becomes the Reply-To header

## Dependencies

- dropcrm

## Configuration

- `drop/admin/dropemail` — default From address and From name
