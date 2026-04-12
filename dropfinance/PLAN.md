# Drop Finance: Budgeting & Cash Flow Projection

## Context

Tim runs a consulting business with ~15 support clients (quarterly billing),
some monthly add-on services, occasional one-off projects, and 1-2 larger
active projects at any time. He needs a local projection tool to see where
his cash flow will be over the next 6 months. This is a personal tool, not
cloud-hosted. Future expansion into basic ledger tracking is possible but
out of scope for now.

## Entities

### 1. Vendor (`drop_vendor`)

Simple entity, local to Drop Finance. Not synced from support site.

| Column      | Type         | Notes                    |
|-------------|-------------|--------------------------|
| vid         | serial PK   |                          |
| name        | varchar 255 |                          |
| notes       | text        | Optional                 |
| uid         | int         | Creator                  |
| created     | int         | Timestamp                |
| changed     | int         | Timestamp                |

### 2. Revenue Line (`drop_revenue`)

An expected income stream — recurring or one-time.

| Column      | Type         | Notes                              |
|-------------|-------------|--------------------------------------|
| rid         | serial PK   |                                      |
| label       | varchar 255 | e.g. "ACME quarterly support"        |
| amount      | numeric 10,2| Dollar amount per occurrence          |
| frequency   | varchar 20  | monthly / quarterly / annual / once  |
| cid         | int NULL    | Linked CRM org (client)              |
| pid         | int NULL    | Linked project (optional)            |
| start_date  | int         | When billing starts (Unix timestamp) |
| end_date    | int NULL    | When it ends (NULL = ongoing)        |
| active      | int tiny    | 1 = included in projections          |
| notes       | text        | Optional                             |
| uid         | int         |                                      |
| created     | int         |                                      |
| changed     | int         |                                      |

### 3. Expense Line (`drop_expense`)

An expected cost — recurring or one-time.

| Column      | Type         | Notes                              |
|-------------|-------------|--------------------------------------|
| eid         | serial PK   |                                      |
| label       | varchar 255 | e.g. "Linode servers" or "Insurance"  |
| amount      | numeric 10,2|                                      |
| frequency   | varchar 20  | monthly / quarterly / annual / once  |
| vid         | int NULL    | Linked vendor (optional)             |
| category    | varchar 100 | For non-vendor expenses              |
| start_date  | int         |                                      |
| end_date    | int NULL    |                                      |
| active      | int tiny    |                                      |
| notes       | text        |                                      |
| uid         | int         |                                      |
| created     | int         |                                      |
| changed     | int         |                                      |

## Pages

### Dashboard (`drop/finance`)
- **Current balance** — stored in config, editable inline or via settings
- **6-month projection table** — rows for each month, columns:
  - Expected income | Expected expenses | Net | Running balance
- **Chart.js graphs** — projection line chart (income/expenses/balance),
  monthly bar chart (income vs expenses side by side)
- **Revenue breakdown** — grouped by client, showing monthly effective amount
- **Expense breakdown** — grouped by vendor/category

### Revenue list (`drop/finance/revenue`)
- Table of all revenue lines with client name, amount, frequency, status
- Add Revenue action link

### Expense list (`drop/finance/expenses`)
- Table of all expense lines with vendor/category, amount, frequency, status
- Add Expense action link

### Vendor list (`drop/finance/vendors`)
- Simple list, Add Vendor action link

### Add/Edit forms
- Revenue: label, amount, frequency, client (autocomplete), project
  (autocomplete), start/end date, active checkbox, notes
- Expense: label, amount, frequency, vendor (autocomplete) OR category
  (text), start/end date, active checkbox, notes
- Vendor: name, notes

## Projection Logic

For each month in the 6-month window:
1. Sum all active revenue lines whose date range includes that month,
   converting to monthly equivalent (quarterly / 3, annual / 12, one-time
   only in its start month)
2. Sum all active expense lines the same way
3. Net = income - expenses
4. Running balance = previous month balance + net

Starting balance comes from config (`dropfinance.settings` -> `balance`),
with a "last updated" date so you know how stale it is.

## Section Nav Links

| Link       | Path                   | Weight |
|------------|------------------------|--------|
| Dashboard  | drop/finance           | 0      |
| Revenue    | drop/finance/revenue   | 5      |
| Expenses   | drop/finance/expenses  | 10     |
| Vendors    | drop/finance/vendors   | 15     |
| Import     | drop/finance/import    | 20     |
| Settings   | drop/finance/settings  | 25     |

## Charts

- Bundle Chart.js (MIT licensed) in the module — no CDN dependency
- Projection line chart: 6-month forward view with income, expenses,
  and running balance lines
- Monthly bar chart: income vs expenses side by side
- Future: once ledger entity exists, projected vs actual overlay

## Future: Ledger Entity (`drop_transaction`)

Not in current scope, but the plan accommodates it:
- Revenue/Expense lines stay as projection templates
- A future `drop_transaction` entity records actual money in/out
- Dashboard can then show projected vs actual comparisons
- No changes needed to the current entities

## Implementation order

1. Schema + entity classes + entity info (all 3 entities)
2. Vendor CRUD (simplest entity — good foundation test)
3. Revenue CRUD with client/project linking
4. Expense CRUD with vendor/category
5. Dashboard projection logic + table + Chart.js graphs
6. Section nav + polish

## Design notes

- Use the DropGoals entity pattern (lightweight, no entity_ui dependency)
- Client/project links are plain DB columns (like project.cid), not entity
  reference fields — keeps it simple
- Vendor is optional on expenses — if vid is set, the vendor name displays;
  if not, the category string displays
- One-time items only appear in their start_date month in the projection
- Active flag lets you "pause" a line without deleting it (e.g. a project
  that's on hold)
