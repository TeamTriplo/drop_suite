# DropDocs

Internal team documentation and wiki for the Drop Suite. Provides a
hierarchical document system with tree navigation, tagging, and reordering.

## Features

- **Hierarchical pages** — `doc` content type with parent-child relationships
  via `field_doc_parent`, enabling nested documentation trees
- **Tree navigation** — sidebar shows the full document hierarchy for easy
  browsing
- **Tagging** — `doc_tags` vocabulary with `field_doc_tags` for categorizing
  documents
- **Reorder interface** — drag-and-drop reordering for administrators
- **Add child pages** — local task on each doc page to quickly create sub-pages
- **Standard node features** — revisions, preview, text format support

## Pages

- `drop/docs` — document index listing
- `drop/docs/reorder` — drag-and-drop reorder interface (admin only)
- `drop/doc/{nid}` — view a document
- `drop/doc/{nid}/edit` — edit a document
- `drop/doc/{nid}/add-child` — create a child page

## Dependencies

- drop_suite

## Permissions

- `access dropdocs` — view documentation pages
- `administer dropdocs` — reorder documents and manage hierarchy
- Standard node permissions apply for creating and editing doc content
