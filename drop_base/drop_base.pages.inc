<?php
/**
 * @file
 * Page callbacks for drop_base module.
 */

/**
 * Settings dashboard — shows links to all Drop module settings pages.
 *
 * Invokes hook_drop_settings_info() to collect settings page definitions
 * from all enabled modules.
 */
function drop_base_settings_dashboard() {
  backdrop_set_title(t('Drop Suite Settings'));

  $items = module_invoke_all('drop_settings_info');

  // Sort by weight.
  uasort($items, function($a, $b) {
    $wa = isset($a['weight']) ? $a['weight'] : 0;
    $wb = isset($b['weight']) ? $b['weight'] : 0;
    return $wa - $wb;
  });

  // Filter by permission.
  global $user;
  $items = array_filter($items, function($item) use ($user) {
    if (empty($item['permission'])) {
      return TRUE;
    }
    return user_access($item['permission'], $user);
  });

  if (empty($items)) {
    return '<p>' . t('No settings pages are available.') . '</p>';
  }

  $output = '<div class="drop-settings-dashboard">';

  foreach ($items as $key => $item) {
    $classes = array('drop-settings-card');
    if (!empty($item['class'])) {
      foreach ((array) $item['class'] as $c) {
        $classes[] = $c;
      }
    }
    $output .= '<div class="' . implode(' ', array_map('check_plain', $classes)) . '">';
    $output .= '<h3>' . l(check_plain($item['title']), $item['path']) . '</h3>';
    if (!empty($item['description'])) {
      $output .= '<p>' . check_plain($item['description']) . '</p>';
    }
    $output .= '</div>';
  }

  $output .= '</div>';

  // Entity types section.
  $entity_types = _drop_base_fieldable_entity_types();
  if (!empty($entity_types) && user_access('administer drop entities')) {
    $output .= '<div class="drop-settings-footer">';
    $output .= '<h3>' . t('Entity Types') . '</h3>';
    $output .= '<p>' . t('Manage custom fields and display settings for Drop Suite entities.') . '</p>';
    $output .= '<div class="drop-settings-dashboard">';
    foreach ($entity_types as $type_info) {
      $output .= '<div class="drop-settings-card">';
      $link_path = $type_info['path'] . '/fields';
      if (menu_get_item($type_info['path'])) {
        $link_path = $type_info['path'];
      }
      $output .= '<h3>' . l(check_plain($type_info['label']), $link_path) . '</h3>';
      if (!empty($type_info['description'])) {
        $output .= '<p>' . check_plain($type_info['description']) . '</p>';
      }
      $output .= '</div>';
    }
    $output .= '</div>';
    $output .= '</div>';
  }

  // Also link to permissions.
  if (user_access('administer permissions')) {
    $output .= '<div class="drop-settings-footer">';
    $output .= '<p>' . l(t('Drop Suite permissions'), 'admin/drop/permissions') . '</p>';
    $output .= '</div>';
  }

  return $output;
}

/**
 * Discovers all fieldable Drop Suite entity types with bundle admin paths.
 *
 * @return array
 *   Array of entity type info, each with 'label', 'path', and 'description'.
 */
function _drop_base_fieldable_entity_types() {
  $all_info = entity_get_info();
  $drop_modules = array('drop_base', 'dropcrm', 'droptickets', 'droptasks', 'dropgoals', 'droptime', 'dropdocs');

  $types = array();
  foreach ($all_info as $entity_type => $info) {
    if (empty($info['fieldable'])) {
      continue;
    }
    // Only include entities owned by Drop Suite modules.
    if (empty($info['module']) || !in_array($info['module'], $drop_modules)) {
      continue;
    }

    // Collect unique bundle admin paths.
    if (!empty($info['bundles'])) {
      foreach ($info['bundles'] as $bundle_name => $bundle_info) {
        if (empty($bundle_info['admin']['path'])) {
          continue;
        }
        $path = $bundle_info['admin']['path'];
        // For entities with multiple bundles (like crm_contact), list each bundle.
        $label = $info['label'];
        if (count($info['bundles']) > 1) {
          $label = $bundle_info['label'];
        }
        $types[$entity_type . '_' . $bundle_name] = array(
          'label' => $label,
          'path' => $path,
          'description' => isset($info['description']) ? $info['description'] : '',
        );
      }
    }
  }

  // Sort alphabetically by label.
  uasort($types, function($a, $b) {
    return strcmp($a['label'], $b['label']);
  });

  return $types;
}

// ---------------------------------------------------------------------------
// Meeting agenda page
// ---------------------------------------------------------------------------

/**
 * Page callback for drop/meeting.
 *
 * Shows all tasks and tickets tagged "Meeting" so the user can review agenda
 * items before a meeting. Each item has actions to remove the tag (addressed)
 * or keep it for a future meeting.
 */
function drop_base_meeting_page() {
  backdrop_set_title(t('Meeting Agenda'));

  $meeting_tid = _drop_base_meeting_tag_tid();
  if (!$meeting_tid) {
    return '<p>' . t('No "Meeting" tag exists yet. Tag a task or ticket with "Meeting" to start building your agenda.') . '</p>';
  }

  // Load postponed items for the current user.
  $postponed = state_get('drop_meeting_postponed', array());
  $now = REQUEST_TIME;

  $items = array();

  // Gather tasks tagged "Meeting".
  if (module_exists('droptasks')) {
    $query = db_select('drop_task', 't')
      ->fields('t', array('tid', 'title', 'status', 'priority', 'due_date'));
    $query->join('field_data_field_drop_tags', 'ft', "ft.entity_id = t.tid AND ft.entity_type = 'drop_task' AND ft.deleted = 0");
    $query->condition('ft.field_drop_tags_tid', $meeting_tid);
    $query->condition('t.status', 'pending');
    $query->orderBy('t.priority', 'DESC');
    $query->orderBy('t.due_date', 'ASC');
    $result = $query->execute();
    foreach ($result as $row) {
      $key = 'drop_task-' . $row->tid;
      if (isset($postponed[$key]) && $postponed[$key] > $now) {
        continue;
      }
      $items[] = array(
        'type'        => 'task',
        'id'          => $row->tid,
        'entity_type' => 'drop_task',
        'title'       => $row->title,
        'priority'    => $row->priority,
        'due_date'    => $row->due_date,
        'path'        => 'drop/tasks',
        'status'      => $row->status,
        'label'       => '',
      );
    }
  }

  // Gather tickets tagged "Meeting".
  if (module_exists('droptickets')) {
    $active_statuses = function_exists('droptickets_active_statuses')
      ? droptickets_active_statuses()
      : array();
    $query = db_select('drop_ticket', 'tk')
      ->fields('tk', array('tid', 'title', 'status', 'priority', 'pid', 'ticket_number'));
    $query->join('field_data_field_drop_tags', 'ft', "ft.entity_id = tk.tid AND ft.entity_type = 'drop_ticket' AND ft.deleted = 0");
    $query->condition('ft.field_drop_tags_tid', $meeting_tid);
    if (!empty($active_statuses)) {
      $query->condition('tk.status', $active_statuses, 'IN');
    }
    $query->orderBy('tk.priority', 'DESC');
    $query->orderBy('tk.tid', 'DESC');
    $result = $query->execute();

    // Load project prefixes for ticket labels.
    $pids = array();
    $rows = array();
    foreach ($result as $row) {
      $rows[] = $row;
      if ($row->pid) {
        $pids[$row->pid] = $row->pid;
      }
    }
    $prefixes = array();
    if ($pids) {
      $projects = db_select('drop_project', 'p')
        ->fields('p', array('pid', 'prefix'))
        ->condition('pid', $pids, 'IN')
        ->execute()
        ->fetchAllKeyed();
      $prefixes = $projects;
    }

    foreach ($rows as $row) {
      $key = 'drop_ticket-' . $row->tid;
      if (isset($postponed[$key]) && $postponed[$key] > $now) {
        continue;
      }
      $prefix = isset($prefixes[$row->pid]) ? $prefixes[$row->pid] : '';
      $ticket_label = $prefix ? $prefix . '-' . $row->ticket_number : '#' . $row->ticket_number;
      $items[] = array(
        'type'        => 'ticket',
        'id'          => $row->tid,
        'entity_type' => 'drop_ticket',
        'title'       => $row->title,
        'priority'    => $row->priority,
        'due_date'    => NULL,
        'path'        => 'drop/tickets/' . $row->tid,
        'status'      => $row->status,
        'label'       => $ticket_label,
      );
    }
  }

  if (empty($items)) {
    return '<p>' . t('No agenda items. Tag a task or ticket with "Meeting" and it will appear here.') . '</p>';
  }

  // Render the agenda.
  $output = '';

  // Separate by type.
  $tasks   = array_filter($items, function($i) { return $i['type'] === 'task'; });
  $tickets = array_filter($items, function($i) { return $i['type'] === 'ticket'; });
  $token = backdrop_get_token('drop_meeting');

  if ($tasks) {
    $output .= '<h3>' . t('Tasks') . '</h3>';
    $output .= '<div class="drop-meeting-items">';
    foreach ($tasks as $item) {
      $output .= _drop_base_meeting_item_row($item, $token);
    }
    $output .= '</div>';
  }

  if ($tickets) {
    $output .= '<h3>' . t('Tickets') . '</h3>';
    $output .= '<div class="drop-meeting-items">';
    foreach ($tickets as $item) {
      $output .= _drop_base_meeting_item_row($item, $token);
    }
    $output .= '</div>';
  }

  backdrop_add_css(backdrop_get_path('module', 'drop_base') . '/drop_base.meeting.css');

  return '<div class="drop-meeting-agenda">' . $output . '</div>';
}

/**
 * Renders a single meeting agenda item row.
 *
 * @param array $item
 *   Item array with keys: type, id, entity_type, title, priority, path, label.
 * @param string $token
 *   CSRF token for the remove action.
 *
 * @return string
 *   HTML for the row.
 */
function _drop_base_meeting_item_row($item, $token) {
  $priority_labels = drop_base_priority_labels();
  $priority_class = '';
  $priority_text = '';
  if (isset($item['priority']) && $item['priority'] > 0) {
    $priority_names = array(1 => 'low', 2 => 'normal', 3 => 'high', 4 => 'critical');
    $pname = isset($priority_names[$item['priority']]) ? $priority_names[$item['priority']] : 'normal';
    $priority_class = 'drop-priority-' . $pname;
    $priority_text = isset($priority_labels[$item['priority']]) ? $priority_labels[$item['priority']] : '';
  }

  $title_link = l($item['title'], $item['path']);
  $type_badge = '<span class="drop-meeting-type drop-meeting-type--' . $item['type'] . '">'
    . ($item['type'] === 'ticket' ? check_plain($item['label']) : t('Task'))
    . '</span>';

  $due_html = '';
  if (!empty($item['due_date'])) {
    $due_html = ' <span class="drop-meeting-due">' . t('Due @date', array('@date' => format_date(strtotime($item['due_date']), 'short'))) . '</span>';
  }

  $entity_key = $item['entity_type'] . '-' . $item['id'];
  $remove_url = url('drop/meeting/remove-tag/' . $entity_key, array(
    'query' => array('token' => $token, 'destination' => 'drop/meeting'),
  ));
  $postpone_url = url('drop/meeting/postpone/' . $entity_key, array(
    'query' => array('token' => $token, 'destination' => 'drop/meeting'),
  ));
  $actions = '<span class="drop-meeting-actions">'
    . '<a href="' . $postpone_url . '" class="drop-meeting-action drop-meeting-action--postpone" title="' . t('Postpone — hide until next meeting') . '">'
    . icon('clock', array('attributes' => array('width' => 18, 'height' => 18)))
    . '</a>'
    . '<a href="' . $remove_url . '" class="drop-meeting-action drop-meeting-action--done" title="' . t('Addressed — remove from agenda') . '">'
    . icon('check-circle', array('attributes' => array('width' => 18, 'height' => 18)))
    . '</a>'
    . '</span>';

  $row_class = 'drop-meeting-item';
  if ($priority_class) {
    $row_class .= ' ' . $priority_class;
  }

  return '<div class="' . $row_class . '">'
    . $type_badge
    . '<span class="drop-meeting-title">' . $title_link . '</span>'
    . ($priority_text ? ' <span class="drop-meeting-priority ' . $priority_class . '">' . check_plain($priority_text) . '</span>' : '')
    . $due_html
    . $actions
    . '</div>';
}

/**
 * Finds the taxonomy term ID for the "Meeting" tag, or NULL if it doesn't exist.
 *
 * @return int|null
 */
function _drop_base_meeting_tag_tid() {
  $vocab = taxonomy_vocabulary_machine_name_load('drop_tags');
  if (!$vocab) {
    return NULL;
  }
  $tid = db_select('taxonomy_term_data', 't')
    ->fields('t', array('tid'))
    ->condition('t.vocabulary', $vocab->machine_name)
    ->condition('t.name', 'Meeting')
    ->execute()
    ->fetchField();
  return $tid ?: NULL;
}

/**
 * Page callback: removes the "Meeting" tag from an entity.
 *
 * Path: drop/meeting/remove-tag/{entity_type}-{entity_id}
 *
 * @param string $entity_key
 *   In the format "entity_type-entity_id", e.g. "drop_task-42".
 */
function drop_base_meeting_remove_tag($entity_key) {
  if (!backdrop_valid_token($_GET['token'], 'drop_meeting')) {
    backdrop_access_denied();
    return;
  }

  $parts = explode('-', $entity_key, 2);
  if (count($parts) !== 2) {
    backdrop_not_found();
    return;
  }
  list($entity_type, $entity_id) = $parts;
  $entity_id = (int) $entity_id;

  $meeting_tid = _drop_base_meeting_tag_tid();
  if (!$meeting_tid || !$entity_id) {
    backdrop_goto('drop/meeting');
    return;
  }

  // Remove the meeting tag from the field data table.
  db_delete('field_data_field_drop_tags')
    ->condition('entity_type', $entity_type)
    ->condition('entity_id', $entity_id)
    ->condition('field_drop_tags_tid', $meeting_tid)
    ->execute();

  // Also clean the revision table if it exists.
  if (db_table_exists('field_revision_field_drop_tags')) {
    db_delete('field_revision_field_drop_tags')
      ->condition('entity_type', $entity_type)
      ->condition('entity_id', $entity_id)
      ->condition('field_drop_tags_tid', $meeting_tid)
      ->execute();
  }

  // Clear the entity cache so the change is visible.
  entity_get_controller($entity_type)->resetCache(array($entity_id));

  backdrop_set_message(t('Removed from meeting agenda.'));
  backdrop_goto('drop/meeting');
}

/**
 * Page callback: postpones a meeting agenda item for 6 hours.
 *
 * Path: drop/meeting/postpone/{entity_type}-{entity_id}
 *
 * @param string $entity_key
 *   In the format "entity_type-entity_id", e.g. "drop_task-42".
 */
function drop_base_meeting_postpone($entity_key) {
  if (!isset($_GET['token']) || !backdrop_valid_token($_GET['token'], 'drop_meeting')) {
    backdrop_access_denied();
    return;
  }

  // Validate the key format.
  $parts = explode('-', $entity_key, 2);
  if (count($parts) !== 2 || !in_array($parts[0], array('drop_task', 'drop_ticket'))) {
    backdrop_not_found();
    return;
  }

  $postponed = state_get('drop_meeting_postponed', array());

  // Clean up expired entries while we're here.
  $now = REQUEST_TIME;
  foreach ($postponed as $key => $expires) {
    if ($expires <= $now) {
      unset($postponed[$key]);
    }
  }

  // Postpone for 6 hours.
  $postponed[$entity_key] = $now + (6 * 3600);
  state_set('drop_meeting_postponed', $postponed);

  backdrop_set_message(t('Postponed until next meeting.'));
  backdrop_goto('drop/meeting');
}

/**
 * Wraps content in a narrow-width container using the configured max-width.
 *
 * @param string $content
 *   The HTML content to wrap.
 * @param string $extra_class
 *   Optional additional CSS class for the wrapper.
 *
 * @return string
 *   The wrapped HTML.
 */
function _drop_base_narrow_wrap($content, $extra_class = '') {
  $width = config_get('drop_base.settings', 'narrow_page_width');
  if (empty($width)) {
    $class = 'drop-narrow-page';
    if ($extra_class) {
      $class .= ' ' . $extra_class;
    }
    return '<div class="' . $class . '">' . $content . '</div>';
  }

  $class = 'drop-narrow-page';
  if ($extra_class) {
    $class .= ' ' . $extra_class;
  }
  return '<div class="' . $class . '" style="max-width: ' . (int) $width . 'px; margin: 0 auto;">' . $content . '</div>';
}

// ---------------------------------------------------------------------------
// Home page navigation setup
// ---------------------------------------------------------------------------
// (Home page navigation code moved to drop_start in phase 5 of the wizard
//  build. The /drop/settings/home-nav route now 301-redirects to the
//  corresponding wizard step. See drop_start.wizard.inc.)
// ---------------------------------------------------------------------------

