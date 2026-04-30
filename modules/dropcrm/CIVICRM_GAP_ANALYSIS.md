# CiviCRM to DropCRM Contact Schema Gap Analysis

**Ticket:** [tid 610](https://support.simplo.site/drop/tickets/610) — Phase 0
**Date:** 2026-04-21
**Source DB:** `tcog2_civicrm` (TCOG local clone at `~/sites/ddev/backdrop/claude-projects/tcog/`)
**Target module:** `modules/drop_suites/dropcrm/`
**Scope:** Contact-slice only. Memberships, events, contributions, and participant data are explicitly out of scope.

## Contact population at a glance

| Metric | Count |
|---|---|
| Contacts in `civicrm_contact` total | 2,021 |
| Active (`is_deleted = 0`) | 2,019 |
| &nbsp;&nbsp;&nbsp;Individuals | 1,978 |
| &nbsp;&nbsp;&nbsp;Organizations | 21 |
| &nbsp;&nbsp;&nbsp;Households | 20 |
| Contacts with at least one email | 922 (46%) |
| Contacts with at least one phone | 934 (46%) |
| Contacts with at least one address | 1,933 (96%) |
| Groups defined | 8 |
| `civicrm_uf_match` rows (CiviCRM ↔ CMS user links) | 9 |

---

## Table A — CiviCRM contact-scope fields that are populated

Counts are from active contacts only (`is_deleted = 0`). Sensitive values are redacted.

### A.1 `civicrm_contact` base-table fields

| Field | Non-null / 2,019 | Sample (redacted) |
|---|---:|---|
| `contact_type` | 2,019 | `Individual`, `Organization`, `Household` |
| `first_name` | 1,930 | (redacted) |
| `last_name` | 1,928 | (redacted) |
| `middle_name` | 252 | (redacted) |
| `sort_name`, `display_name` | ~2,019 | computed by CiviCRM |
| `prefix_id` | 123 | 1=Mrs., 2=Ms., 3=Mr., 4=Dr. |
| `suffix_id` | 38 | 1=Jr., 2=Sr., 3=II, 4=III, 5=IV, 6=V, 7=VI, 8=VII |
| `gender_id` | 96 | 1=Female, 2=Male, 3=Other |
| `birth_date` | 86 | date |
| `is_deceased` | 11 | flag |
| `deceased_date` | (not separately counted) | date |
| `organization_name` | 38 | denormalized on Individual rows for "current employer" |
| `employer_id` | 17 | Individual → Organization FK |
| `household_name` | 20 | on Household rows |
| `source` | 1,454 | `Sample Data`, `skylark 13`, `spcofriends19`, `mn opera donor 02`, `tour`, `Auditions 15`, `figaro 16` (event/campaign tag) |
| `do_not_email` | 49 | flag |
| `do_not_phone` | 48 | flag |
| `do_not_trade` | 37 | flag |
| `is_opt_out` | 26 | flag |
| `do_not_mail` | 0 | — |
| `do_not_sms` | 0 | — |
| `preferred_communication_method` | 89 | 1=Phone, 2=Email, 3=Postal Mail, 4=SMS, 5=Fax |
| `preferred_language` | 1,818 | always `en_US` |
| `preferred_mail_format` | 2,019 | always `Both` |
| `communication_style_id` | 1,818 | near-universal system default |
| `email_greeting_id`, `postal_greeting_id`, `addressee_id` | ~1,998 | system computed; no custom overrides in this data |
| `created_date`, `modified_date` | 2,019 | timestamps |
| `contact_sub_type`, `legal_identifier`, `legal_name`, `nick_name`, `image_URL`, `formal_title`, `job_title`, `sic_code`, `user_unique_id`, `external_identifier`, `api_key`, `email_greeting_custom`, `postal_greeting_custom`, `addressee_custom` | 0 or 1 | not used in TCOG data |

### A.2 `civicrm_email` (1,062 rows, 922 contacts)

| Field | Notes |
|---|---|
| `email` | 1,062 values |
| `location_type_id` | 881 Home (1), 13 Work (2), 14 Main (3), 154 Billing (5) |
| `is_primary` | 922 flagged primary |
| `is_bulkmail`, `on_hold`, `hold_date`, `signature_text`, `signature_html` | **all zero** |

### A.3 `civicrm_phone` (983 rows, 934 contacts)

| Field | Notes |
|---|---|
| `phone` | 983 values (all location_type 1 / Home) |
| `phone_type_id` | 902 Phone (1), 81 Mobile (2); no Fax/Pager/Voicemail |
| `phone_ext` | 0 non-null |

### A.4 `civicrm_address` (1,954 rows, 1,933 contacts)

| Field | Non-null / 1,954 | Notes |
|---|---:|---|
| `street_address` | 656 | one-line string |
| `city` | 1,912 | |
| `state_province_id` | 1,946 | CiviCRM state-province FK |
| `postal_code` | 652 | |
| `country_id` | 1,954 | CiviCRM country FK (always populated) |
| `supplemental_address_1` | 34 | |
| `supplemental_address_2/3` | 0 | |
| `geo_code_1/2` | 178 | lat/lon |
| Location split: 1,911 Home / 14 Work / 20 Main / 9 Billing | | |
| `street_number*`, `street_name`, `street_type`, `street_unit` | low | CiviCRM's parsed sub-fields; populated sporadically |

### A.5 `civicrm_group` / `civicrm_group_contact`

8 groups total. Group memberships (status='Added'):

| Group | Members |
|---|---:|
| Email List | 768 |
| Newsletter Subscribers | 60 |
| Summer Program Volunteers | 15 |
| Advisory Board | 8 |
| Test Emails – w/ TCOG | 5 |
| Administrators | 2 |
| Case Resources | 0 |
| Admin Group | 0 |

### A.6 Custom-field groups extending Contact/Individual

CiviCRM custom fields live in `civicrm_value_<name>_<id>` tables, one row per entity. Usage is thin:

| Custom group | Field(s) | Populated rows (active contacts) |
|---|---|---:|
| Constituent Information | Most Important Issue, Marital Status, Marriage Date | 0 populated (18 rows, all empty) |
| Guests | Guests (text) | 10 of 29 |
| Name | Name (text — alternate display name, mostly already duplicative of first+last) | 71 |
| SPECIAL DIETARY NEEDS | special_dietary_needs, please_seat_me_with_tim | 50 diet, 0 seating |
| Custom Seating | Guest Name, Meal Choice | 0 |

These are overwhelmingly event-driven (dietary, seating, guests) and are more naturally tied to events/memberships (out-of-scope for this phase) than to the contact itself. Recommendation: defer.

---

## Table B — Current DropCRM contact fields

### B.1 `crm_contact` base-table columns (from `dropcrm_schema()`)

| Column | Type | Purpose |
|---|---|---|
| `cid` | serial | Primary key |
| `type` | varchar(64) | Bundle: `crm_person`, `crm_organization`, `crm_household` |
| `name` | varchar(255) | Display name (computed for persons) |
| `status` | tinyint | 1 = active, 0 = inactive |
| `uid` | int | Creator user ID |
| `created` | int | Unix timestamp |
| `changed` | int | Unix timestamp |

### B.2 Field API instances on `crm_contact`

| Field name | Type | Bundles | Label | Notes |
|---|---|---|---|---|
| `field_crm_first_name` | text | crm_person | First Name | required |
| `field_crm_last_name` | text | crm_person | Last Name | required |
| `field_crm_middle_name` | text | crm_person | Middle Name | |
| `field_crm_job_title` | text | crm_person | Job Title | |
| `field_crm_employer` | entityreference | crm_person | Organization | unlimited, IEF widget; targets crm_organization |
| `field_crm_org_description` | text_long | crm_organization | Description | |
| `field_crm_website` | text | crm_organization | Website | |
| `field_crm_rating` | list_integer (1–5) | crm_person, crm_organization | Rating | star chooser |
| `field_crm_warmth` | list_text | crm_organization | Warmth | Hot/Warm/Cold |
| `field_crm_status` | text_long | crm_person, crm_organization, crm_household | Status | free-form note |
| `field_drop_tags` | taxonomy (drop_tags) | all 3 | Tags | free-form |
| `field_contact_type` | taxonomy (contact_types) | all 3 | Contact type | bounded: Client, Past Client, Prospect, Partner, Vendor, Personal, Internal |

### B.3 `crm_contact_method` (email / phone / address / url bundles)

Base-table columns: `cmid`, `type` (bundle), `cid`, `label` (e.g. Home/Work), `value`, `is_primary`, `created`, `changed`.

Field API instances on the `address` bundle: `field_crm_street`, `field_crm_city`, `field_crm_state_province`, `field_crm_postal_code`, `field_crm_country` (all plain text).

No Field API instances on `email`, `telephone`, or `url` — the base-table `value` column holds the address/number/URL and `label` holds Home/Work/etc.

### B.4 `crm_user_contact` (from dropcrm_user submodule)

One-to-one map of `uid` → `cid`. Analogue of `civicrm_uf_match`.

---

## Table C — Gap analysis

| CiviCRM field | DropCRM equivalent | Gap bucket | Rationale |
|---|---|---|---|
| `contact_type` (Individual/Org/Household) | `crm_contact.type` (crm_person/crm_organization/crm_household) | **Map** | Direct bundle mapping; trivial in importer. |
| `first_name` / `last_name` | `field_crm_first_name` / `field_crm_last_name` | **Map** | Direct. |
| `middle_name` | `field_crm_middle_name` | **Map** | Direct. |
| `job_title` | `field_crm_job_title` | **Map** | Direct (0 populated in TCOG anyway). |
| `organization_name` (Individual denorm) | `field_crm_employer` (entityref) | **Map via lookup** | Need to resolve to the crm_organization cid during import. `employer_id` FK is the authoritative value (17 rows); `organization_name` is CiviCRM's denormalized cache. |
| `household_name` | `crm_contact.name` on crm_household | **Map** | Direct. |
| `prefix_id` (Mr./Mrs./Dr./Ms.) | — | **Add** | 123 contacts use this. Low-cost addition: `field_crm_name_prefix` as `list_text` with the 4 values. |
| `suffix_id` (Jr./Sr./II/III…) | — | **Add** | 38 contacts. `field_crm_name_suffix` as `list_text`. |
| `gender_id` | — | **Consider** | Only 96 contacts (5%); usable but not blocking. If added, `field_crm_gender` list_text (Female/Male/Other). |
| `birth_date` | — | **Consider** | 86 contacts; useful for CRM segmentation. If added, `field_crm_birth_date` (date). |
| `is_deceased` / `deceased_date` | — | **Consider** | 11 deceased contacts. Could be modeled by archiving (set `status=0`) rather than a dedicated field; if the family/donor history matters, `field_crm_deceased` (boolean) + `field_crm_deceased_date` (date). |
| `do_not_email` | — | **Add** | 49 contacts opted out; must survive migration to avoid re-contacting. `field_crm_do_not_email` (boolean). |
| `do_not_phone` | — | **Add** | 48 contacts. `field_crm_do_not_phone` (boolean). |
| `do_not_mail` (postal) | — | **Add** | 0 in TCOG but defensible data-retention semantics; cheap to include. `field_crm_do_not_mail`. |
| `is_opt_out` (global bulk-mail opt-out) | — | **Add** | 26 contacts. Distinct from per-channel flags — overrides bulk mail. `field_crm_bulk_opt_out` (boolean). |
| `do_not_trade`, `do_not_sms` | — | **Skip** | TCOG-specific low/zero usage and Drop Suites has no SMS or list-trading workflow. |
| `source` (campaign/origin) | `field_drop_tags` or new field | **Add** | 1,454 populated (72%). This is crucial provenance data. Recommend a dedicated `field_crm_source` (text, single-value) rather than overloading tags — values like "mn opera donor 02" are free-form and don't benefit from taxonomy aggregation. Alternative: create a `contact_source` taxonomy if we want segmentation later, but text is lower risk for a first pass. |
| `preferred_communication_method` | — | **Consider** | 89 contacts. Could map to `field_crm_pref_contact` (list_text: Phone/Email/Postal Mail/SMS/Fax) — but very sparse. Low priority. |
| `preferred_language`, `preferred_mail_format`, `communication_style_id` | — | **Skip** | Essentially uniform defaults (`en_US`, `Both`) — zero information gain. |
| `email_greeting_id`, `postal_greeting_id`, `addressee_id` (+ custom/display variants) | — | **Skip** | CiviCRM system-computed salutations. DropCRM doesn't do mail-merge in Phase 1. |
| `contact_sub_type`, `legal_identifier`, `legal_name`, `nick_name`, `image_URL`, `formal_title`, `sic_code`, `user_unique_id`, `external_identifier`, `api_key`, `hash`, `employer_id` (raw), `primary_contact_id` | — | **Skip** | Zero or near-zero population in TCOG. If a use-case surfaces later we can add on demand. |
| `created_date` | `crm_contact.created` (Unix ts) | **Map** | Convert timestamp → int. |
| `modified_date` | `crm_contact.changed` | **Map** | Convert. |
| `is_deleted` | `crm_contact.status` | **Map** | `is_deleted=1` → `status=0` or skip row entirely. |
| — (CiviCRM has no explicit `uid` / creator) | `crm_contact.uid` | **Derive** | Set to importing user's uid (admin). |
| **Email: `email`** | `crm_contact_method.value` on `email` bundle | **Map** | Direct. |
| **Email: `location_type_id` (Home/Work/Main/Billing)** | `crm_contact_method.label` | **Map** | 1→Home, 2→Work, 3→Main, 5→Billing. |
| **Email: `is_primary`** | `crm_contact_method.is_primary` | **Map** | Direct. |
| **Email: `is_bulkmail`, `on_hold`, `signature_*`** | — | **Skip** | All zero in TCOG. |
| **Phone: `phone`** | `crm_contact_method.value` on `telephone` bundle | **Map** | Direct. |
| **Phone: `phone_type_id` (Phone/Mobile/Fax…)** | `crm_contact_method.label` (or new field) | **Map with widening** | DropCRM labels today are location-oriented (Home/Work). CiviCRM has both a *location* (Home) and a *type* (Mobile) on phones, independently. For the migration we can concatenate ("Home — Mobile") into `label` OR add a simple `field_crm_phone_type` list_text (Phone/Mobile/Fax) to the `telephone` bundle. Recommend the field addition — cleaner long-term. |
| **Phone: `phone_ext`** | — | **Skip** | 0 in data. |
| **Address: `street_address`** | `field_crm_street` | **Map** | Direct. Note CiviCRM also has parsed `street_number`, `street_name`, etc. — ignore those; use the assembled `street_address`. |
| **Address: `supplemental_address_1`** | — | **Add** | 34 contacts have it. `field_crm_street_2` (text). Line-2 is a well-known address pattern. |
| **Address: `city`** | `field_crm_city` | **Map** | |
| **Address: `state_province_id`** | `field_crm_state_province` (text) | **Map via lookup** | Resolve CiviCRM state FK → state name string (e.g. "Minnesota" or "MN" — pick one and stay consistent). CiviCRM ships a `civicrm_state_province` table. |
| **Address: `postal_code`** | `field_crm_postal_code` | **Map** | |
| **Address: `country_id`** | `field_crm_country` (text) | **Map via lookup** | Resolve from `civicrm_country` table. |
| **Address: `geo_code_1/2`** | — | **Skip** | 178 populated; DropCRM has no map display today. Can re-geocode later if needed. |
| **Address: `location_type_id`** | `crm_contact_method.label` | **Map** | Same mapping as email. |
| **Address: `is_primary`** | `crm_contact_method.is_primary` | **Map** | |
| `civicrm_group_contact` (status='Added') | `field_drop_tags` | **Map via seeding** | 856 active memberships across 8 groups. Seed the drop_tags vocabulary with the group names, add a tag per contact per Added row. Groups `Case Resources` and `Admin Group` have 0 members and can be skipped. |
| `civicrm_uf_match` (CMS user ↔ contact) | `crm_user_contact.uid` ↔ `cid` | **Map if accounts exist** | Only 9 rows — hand-resolve at import. |
| Custom: Constituent Information (Most Important Issue, Marital Status, Marriage Date) | — | **Skip** | 0 populated. |
| Custom: Guests (10 rows with text) | — | **Skip / defer** | Event-scoped; belongs to future events module. |
| Custom: Name (71 rows) | — | **Consider** | Mostly duplicative of first+last. If any don't match, append to `field_crm_status` as a note. |
| Custom: SPECIAL_DIETARY_NEEDS (50 rows) | — | **Skip / defer** | Event-scoped; append to status note if we want to preserve signal. |
| Custom: Custom Seating, Payment Details, Credit Card, Check For, Zip, Donor Information, Food Preference, Event Seating, Address-11 | — | **Skip** | Event/donation-scoped. Out of contact scope. |

---

## Recommended P1 work

Ordered by value-to-effort. All are simple `field_create_field` + `field_create_instance` calls — no new schema tables needed.

1. **`field_crm_do_not_email`** — `list_integer` / boolean — 49 TCOG contacts. *Blocker for sending any broadcast email post-migration.* Attach to all three contact bundles.
2. **`field_crm_do_not_phone`** — boolean — 48 TCOG contacts. Attach to all three bundles.
3. **`field_crm_do_not_mail`** — boolean — 0 in TCOG but same cost to add and fills out the "privacy trio."
4. **`field_crm_bulk_opt_out`** — boolean — 26 TCOG contacts; distinct semantics from per-channel flags.
5. **`field_crm_source`** — `text` (single-value, 255 chars) — 1,454 TCOG contacts (72% of the base). High-value provenance data. Attach to all three bundles.
6. **`field_crm_name_prefix`** — `list_text` with values `Mrs./Ms./Mr./Dr.` — 123 TCOG contacts. crm_person only.
7. **`field_crm_name_suffix`** — `list_text` with values `Jr./Sr./II/III/IV/V/VI/VII` — 38 TCOG contacts. crm_person only.
8. **`field_crm_birth_date`** — `date` — 86 TCOG contacts (5%). crm_person only. Useful for future segmentation; low cost.
9. **`field_crm_gender`** — `list_text` (`Female/Male/Other`) — 96 TCOG contacts. crm_person only. If adding, recommend a `settings` hook so Site Architects can extend/localize.
10. **`field_crm_street_2`** — `text` — 34 TCOG contacts. Attach to the `address` bundle on `crm_contact_method`. Line-2 is a standard real-world need.
11. **`field_crm_phone_type`** — `list_text` (`Phone/Mobile/Fax/Pager/Voicemail`) — Attach to the `telephone` bundle on `crm_contact_method`. Lets us preserve Home-landline vs Home-mobile distinction that CiviCRM tracks.

**Total: 11 new fields across 3 contact bundles + 2 contact-method bundles. No new schema tables.** All additions are idempotent field_create calls — one update hook (`dropcrm_update_1028`) plus matching mirroring in `_dropcrm_create_fields()`.

### Deferred for a later phase (or out of contact scope)

- Deceased flag + date (11 contacts — small; can be captured in status note until needed)
- Preferred-contact-method (89 contacts — very low value density)
- Geocoding (lat/lon) — no current consumer
- Custom-field group migration (event/dietary data belongs to a future events module)
- CiviCRM's parsed address sub-fields (`street_number`, `street_name`, etc.) — not worth the parsing complexity

---

## CiviCRM quirks worth flagging for the importer

1. **`location_type_id` is an FK, not a label.** The importer must resolve via `civicrm_location_type`: 1=Home, 2=Work, 3=Main, 4=Other, 5=Billing. Same pattern applies to `prefix_id`, `suffix_id`, `gender_id`, `phone_type_id`, `preferred_communication_method` — all live in `civicrm_option_value` keyed by `option_group_id`.
2. **`state_province_id` and `country_id`** are FKs into `civicrm_state_province` and `civicrm_country` (CiviCRM ships thousands of reference rows). Importer needs to JOIN — don't try to import the raw int.
3. **`organization_name` on Individual rows is a denormalized string** — the authoritative link is `employer_id` (FK to another contact row). Use the FK, fall back to the string only when FK is null.
4. **`display_name` / `sort_name` are computed by CiviCRM** — do not import them; DropCRM's entity save already composes `name` from first+last.
5. **`civicrm_group_contact.status` has three values** (`Added`, `Removed`, `Pending`). Only migrate rows where `status = 'Added'`.
6. **`civicrm_contact.is_deleted = 1`** means "in trash." Skip those rows entirely (2 of 2,021 in TCOG).
7. **Phone/address location fields overlap with phone *type*.** A "Home Mobile" in CiviCRM is location=Home + type=Mobile. DropCRM's current single `label` column collapses both. Adding `field_crm_phone_type` (item 11 above) resolves this cleanly.
8. **CiviCRM uses `created_date` as `TIMESTAMP`** — convert to Unix int for `crm_contact.created`.
9. **`preferred_language` is always `en_US`** and `preferred_mail_format` is always `Both` — skip unconditionally; they carry no signal.
10. **Custom-field tables use machine-mangled column names** (e.g. `most_important_issue_1`, `marriage_date_3`). The numeric suffix is the `civicrm_custom_field.id`. If we ever revisit custom-field migration, read `civicrm_custom_field` to map column → logical name.
11. **`civicrm_uf_match`** (9 rows) links Drupal/CMS users to contacts. If the import runs against a TCOG site where those accounts already exist, we can populate `crm_user_contact`. Otherwise skip — can be wired up post-migration.
