# Drop Monitor — Handoff Document

## What is this?

Drop Monitor (`dropmonitor`) is a Backdrop CMS module that monitors the health
of remote Linode servers and the sites running on them. It lives within the
Drop Suite collection of modules.

## Where things are

### Local development sites

- **Dev site (dropsuites):** `~/sites/ddev/backdrop/claude-projects/dropsuites/`
  - URL: https://dropsuites.ddev.site
  - Module: `docroot/modules/drop_suites/dropmonitor/`
  - This is the canonical source for all Drop Suite module development

- **Support site:** `~/sites/ddev/backdrop/claude-projects/supportsite/`
  - URL: https://supportsite.ddev.site
  - Module is enabled here with 7 Linode servers already configured
  - Has a Linode API token configured in settings
  - This site is a local copy of the live support.simplo.site

### Remote (live)

- **support.simplo.site** — the production support site on the `simplo` Linode (104.237.157.18)
- Module is deployed and running at `support.simplo.site/drop/monitor`
- Push code with: `~/bin/push-to-support.sh dropmonitor` (or no args for all modules)
- Cache must be cleared manually after push (bee not available at expected path on remote)
- Database updates must be run manually after schema changes

## Module architecture

Drop Monitor collects data from three sources per server:

1. **Linode API** — server status, region, specs, monthly transfer usage. Requires a read-only API token from cloud.linode.com stored in module config.

2. **HTTP checks** — hits the site URL, measures response code + time, checks SSL certificate expiry via `stream_socket_client()`.

3. **Status agent script** (`agent/status.php`) — a standalone PHP file deployed to each remote server. Protected by a shared secret token. Collects: disk space (free/total/percent), pending OS updates (apt), pending security updates, PHP version, Backdrop version, last cron run, database size, server uptime.

### Data flow

- `hook_cron()` runs checks on a configurable interval (default: 12 hours)
- Manual refresh via "Refresh all" button on dashboard or per-server refresh links
- Results stored in `dropmonitor_check` table (one row per server per check type, upserted on each run)

### Limitation: Linode API does not provide disk usage

The Linode API only reports disk *allocation* (how much of the plan's disk is assigned), not actual filesystem usage. Real disk space data requires the `status.php` agent on the server. The transfer quota from the API is returned in GB (not bytes), while transfer used is in bytes — this was a bug that was fixed.

## Files

| File | Purpose |
|------|---------|
| `dropmonitor.info` | Module definition, depends on drop_base |
| `dropmonitor.module` | Core hooks: menu, cron, permissions, nav link, helpers |
| `dropmonitor.install` | Schema: `dropmonitor_server` and `dropmonitor_check` tables |
| `dropmonitor.admin.inc` | Settings form (API token, thresholds, interval), server CRUD forms |
| `dropmonitor.pages.inc` | Dashboard (card grid), server detail page, refresh callbacks |
| `dropmonitor.checks.inc` | All data collection: HTTP, agent polling, Linode API |
| `dropmonitor.css` | Dashboard card layout, status badges, metric grid styling |
| `config/dropmonitor.settings.json` | Default config values |
| `agent/status.php` | Standalone agent script to deploy on remote servers |
| `~/bin/push-to-support.sh` | Deploys module code to live support.simplo.site |

## Database tables

### `dropmonitor_server`
Stores the list of servers to monitor.
- `sid` — primary key
- `label` — display name
- `linode_id` — Linode instance ID (nullable, for API checks)
- `site_url` — primary URL for HTTP checks
- `agent_url` — URL to the deployed status.php script
- `agent_token` — shared secret for agent authentication
- `active` — boolean, whether to include in checks
- `weight` — sort order on dashboard

### `dropmonitor_check`
Latest check results. One row per server per check type (upserted).
- `cid` — primary key
- `sid` — server reference
- `check_type` — `http`, `agent`, or `linode`
- `status` — `ok`, `warning`, `critical`, or `error`
- `data` — JSON blob of all collected metrics
- `checked` — timestamp of last check

## Pages

- `drop/monitor` — dashboard showing all servers as cards with metrics
- `drop/monitor/server/{sid}` — detail view with all raw data per server
- `drop/monitor/servers` — admin list of configured servers
- `drop/monitor/servers/add` — add a server
- `drop/monitor/servers/{sid}/edit` — edit a server
- `drop/monitor/servers/{sid}/delete` — delete a server
- `drop/monitor/settings` — API token, check interval, alert thresholds
- `drop/monitor/refresh` — triggers all checks (CSRF-protected)
- `drop/monitor/server/{sid}/refresh` — triggers checks for one server

## Permissions

- `access dropmonitor` — view the dashboard
- `administer dropmonitor` — manage servers, settings, run manual checks

## Current state (as of 2026-04-03)

### What's working
- Module is enabled on the local supportsite and deployed to live support.simplo.site
- 7 online Linode servers are configured (auto-populated from the API)
- Linode API checks work (status, region, specs, transfer)
- HTTP checks work (response code, time, SSL cert expiry)
- Agent checks working on 3 servers: simplo, Triplo-PROD (flatbushfood.coop), FMR
- NPT agent check works via origin_ip bypass (Cloudflare blocks direct requests)
- Status agent collects: disk space, pending updates, PHP version, OS version (Ubuntu), Backdrop version, DB size, uptime
- Dashboard renders server cards with color-coded status badges
- Server detail page shows all raw check data
- Manual refresh (all and per-server) works
- Push-to-support script (`~/bin/push-to-support.sh`) for deploying to live site

### Agent deployment status

| Server | status.php | Token | OS reported |
|--------|-----------|-------|-------------|
| simplo | Deployed | Yes | Ubuntu 16.04.7 LTS |
| Triplo-PROD | Deployed (flatbush docroot) | Yes | Ubuntu 16.04.7 LTS |
| FMR | Deployed | Yes | Ubuntu 22.04.5 LTS |
| NPT | Deployed (needs origin_ip) | Yes | Ubuntu 24.04.3 LTS |
| triplo-prod-three | Not deployed | — | — |
| triplo-prod-two | Not deployed | — | — |
| triplo-development | Not deployed | — | — |

### Origin IP (CDN bypass)

Servers behind Cloudflare (like NPT) block `backdrop_http_request()` with a JS
challenge, returning 403. The `origin_ip` field on the server record rewrites the
agent URL to hit the origin IP directly with a `Host` header matching the domain.
SSL verification uses the original hostname via `stream_context_create()`.

To configure: edit the server at `drop/monitor/servers/{sid}/edit`, enter the
server's real IP in the "Origin IP" field.

### What's not done yet
- **Agent deployment**: 3 servers still need status.php deployed (triplo-prod-three, triplo-prod-two, triplo-development). Each needs SSH access and appropriate file permissions.
- **Cron data**: The agent can't read cron_last from some servers' state tables (returns 0). May be a DB permissions issue or state table structure.
- **Historical data**: Currently only stores the latest check per server. No history/trending. Could add a `dropmonitor_check_history` table later.
- **Email/Slack alerts**: No notification system yet when thresholds are breached. Could integrate with DropBridge's existing `dropbridge_notify()` system.

### Future idea: standalone status app

Rather than deploying `status.php` into individual Backdrop site docroots (which
requires matching file permissions per site), build a standalone PHP app that runs
once per server:

- Lives at a fixed path (e.g. `/var/www/status/`) with its own nginx vhost
- One install per server covers all sites on that box
- Not tied to any Backdrop docroot — no permission issues
- Self-contained: single directory with `index.php` + `config.php`
- Could discover all Backdrop sites on the server and report on each
- Accessible via subdomain (e.g. `status.triplo-prod.triplo.co`) or port

This would replace the per-site status.php approach. Delegation candidate — does
not require deep Drop Monitor knowledge to build.

## Deploying the agent to a new server

1. Copy `agent/status.php` to the server's web-accessible docroot
2. Edit the file and set `DROPMONITOR_TOKEN` to a unique random string
3. If the file is NOT in the Backdrop docroot, set `DROPMONITOR_BACKDROP_ROOT` to the absolute path
4. Test: `curl "https://example.com/status.php?token=YOUR_TOKEN"`
5. In Drop Monitor, edit the server and fill in Agent URL + Agent token
6. Hit Refresh on the dashboard

## Backdrop conventions used

- Config via `config('dropmonitor.settings')` (JSON files)
- State via `state_get()`/`state_set()` for runtime data (last cron timestamp)
- `drop_base_page_actions()` for action bars — pass pre-rendered `l()` strings, NOT link arrays
- `hook_drop_nav_links()` to register in the Drop Suite nav bar
- CSRF protection on refresh links via `backdrop_get_token()`/`backdrop_valid_token()`
- Menu loader pattern: `%dropmonitor_server` with `dropmonitor_server_load()`

## Linode API reference

- Base URL: `https://api.linode.com/v4/`
- Auth: `Authorization: Bearer {token}` header
- Instances list: `GET linode/instances`
- Instance detail: `GET linode/instances/{id}`
- Transfer: `GET linode/instances/{id}/transfer` — `used` is in bytes, `quota` is in GB
- Docs: https://techdocs.akamai.com/linode-api/reference/api
