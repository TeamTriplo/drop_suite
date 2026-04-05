<?php
/**
 * @file
 * Admin pages for Drop Base: project management.
 */

// ---------------------------------------------------------------------------
// Project Field UI landing page
// ---------------------------------------------------------------------------

/**
 * Page callback: Field UI landing page for the project entity.
 *
 * Field UI attaches "Manage fields" and "Manage display" tabs automatically.
 */
function drop_base_project_manage_page() {
  $count = db_select('drop_project', 'p')
    ->countQuery()->execute()->fetchField();
  $active = db_select('drop_project', 'p')
    ->condition('active', 1)
    ->countQuery()->execute()->fetchField();

  $output = '<p>' . t('The Project entity is used across the Drop Suite for tickets, time tracking, and monitoring.') . '</p>';
  $output .= '<p>' . t('@total projects (@active active).', array(
    '@total' => $count,
    '@active' => $active,
  )) . '</p>';
  $output .= '<p>' . l(t('← Back to project list'), 'drop/projects') . '</p>';

  return $output;
}

// ---------------------------------------------------------------------------
// Project list
// ---------------------------------------------------------------------------

/**
 * Page callback: list all projects.
 */
function drop_base_project_list() {
  backdrop_set_title(t('Projects'));

  $projects = db_select('drop_project', 'p')
    ->fields('p')
    ->orderBy('p.active', 'DESC')
    ->orderBy('p.name', 'ASC')
    ->execute()
    ->fetchAll();

  if (empty($projects)) {
    return '<p>' . t('No projects yet. !link.', array(
      '!link' => l(t('Add your first project'), 'drop/projects/add'),
    )) . '</p>';
  }

  $has_crm = module_exists('dropcrm');

  $rows = array();
  foreach ($projects as $project) {
    $status = $project->active ? t('Active') : '<em>' . t('Archived') . '</em>';
    $label  = l(check_plain($project->name), 'drop/projects/' . $project->pid);

    $links = array(
      l(t('Edit'), 'drop/projects/' . $project->pid . '/edit'),
      l(t('Delete'), 'drop/projects/' . $project->pid . '/delete'),
    );

    $prefix_cell = !empty($project->prefix)
      ? '<code>' . check_plain($project->prefix) . '</code>'
      : '—';

    $row = array(
      array('data' => $label),
      array('data' => $prefix_cell),
    );

    // Contact column — only when CRM is enabled.
    if ($has_crm) {
      $contact_cell = '—';
      if (!empty($project->cid)) {
        $contacts = entity_load('crm_contact', array((int) $project->cid));
        if ($contacts) {
          $contact = reset($contacts);
          $contact_cell = l(check_plain($contact->name), 'drop/crm/contact/' . $project->cid);
        }
      }
      $row[] = array('data' => $contact_cell);
    }

    $row[] = array('data' => $status);
    $row[] = array('data' => implode(' · ', $links));
    $rows[] = $row;
  }

  $header = array(t('Project'), t('Prefix'));
  if ($has_crm) {
    $header[] = t('Contact');
  }
  $header[] = t('Status');
  $header[] = t('Actions');

  return theme('table', array(
    'header'     => $header,
    'rows'       => $rows,
    'attributes' => array('class' => array('droptime-project-table')),
  ));
}

// ---------------------------------------------------------------------------
// Project view
// ---------------------------------------------------------------------------

/**
 * Page callback: view a single project with its ticket list.
 */
function drop_base_project_view($project) {
  backdrop_set_title(check_plain($project->name));

  $css = array(backdrop_get_path('module', 'drop_base') . '/drop_base.css');
  if (module_exists('droptickets')) {
    $css[] = backdrop_get_path('module', 'droptickets') . '/droptickets.css';
  }
  $output = array(
    '#attached' => array('css' => $css),
  );

  // Get extra field display settings.
  $display = field_extra_fields_get_display('drop_project', 'drop_project', 'full');

  // Actions bar.
  if (user_access('administer projects')) {
    $output['actions'] = array(
      '#markup' => '<div class="drop-page-actions">'
        . l(t('Edit project'), 'drop/projects/' . $project->pid . '/edit')
        . l(t('Delete'), 'drop/projects/' . $project->pid . '/delete')
        . '</div>',
      '#weight' => -20,
    );
  }

  // Prefix.
  if (!empty($project->prefix) && (!isset($display['prefix']) || $display['prefix']['visible'])) {
    $output['prefix'] = array(
      '#markup' => '<div class="dropcrm-project-meta-item">' . t('Prefix: <code>@p</code>', array('@p' => $project->prefix)) . '</div>',
      '#weight' => isset($display['prefix']['weight']) ? $display['prefix']['weight'] : -5,
    );
  }

  // Contact — only when CRM is enabled.
  if (module_exists('dropcrm') && (!isset($display['contact']) || $display['contact']['visible'])) {
    $contact_html = '—';
    if (!empty($project->cid)) {
      $contacts = entity_load('crm_contact', array((int) $project->cid));
      if ($contacts) {
        $contact = reset($contacts);
        $contact_html = l(check_plain($contact->name), 'drop/crm/contact/' . $project->cid);
      }
    }
    $output['contact'] = array(
      '#markup' => '<div class="dropcrm-project-meta-item">' . t('Contact: !c', array('!c' => $contact_html)) . '</div>',
      '#weight' => isset($display['contact']['weight']) ? $display['contact']['weight'] : 0,
    );
  }

  // Site URL.
  if (!empty($project->site_url) && (!isset($display['site_url']) || $display['site_url']['visible'])) {
    $output['site_url'] = array(
      '#markup' => '<div class="dropcrm-project-meta-item">' . t('Website: !url', array('!url' => l(check_plain($project->site_url), $project->site_url, array('external' => TRUE)))) . '</div>',
      '#weight' => isset($display['site_url']['weight']) ? $display['site_url']['weight'] : 5,
    );
  }

  // Status (archived indicator).
  if (!$project->active && (!isset($display['status']) || $display['status']['visible'])) {
    $output['status'] = array(
      '#markup' => '<div class="dropcrm-project-meta-item"><span class="dropcrm-project-archived">' . t('Archived') . '</span></div>',
      '#weight' => isset($display['status']['weight']) ? $display['status']['weight'] : 10,
    );
  }

  // Tickets for this project.
  if (module_exists('droptickets') && (!isset($display['tickets']) || $display['tickets']['visible'])) {
    $status_options = droptickets_status_options();
    $type_options   = droptickets_type_options();

    // Status filter.
    $valid_statuses = array_merge(array('all'), array_keys($status_options));
    $status_filter  = (isset($_GET['status']) && in_array($_GET['status'], $valid_statuses))
      ? $_GET['status'] : 'all';

    // Filter tabs.
    $tab_options = array('all' => t('All')) + $status_options;
    $tabs = '';
    foreach ($tab_options as $key => $label) {
      $active = ($key === $status_filter) ? ' class="droptickets-tab-active"' : '';
      $tabs .= '<a href="' . url('drop/projects/' . $project->pid, array('query' => array('status' => $key))) . '"' . $active . '>' . check_plain($label) . '</a> ';
    }

    $query = db_select('drop_ticket', 't')
      ->fields('t')
      ->condition('t.pid', $project->pid)
      ->orderBy('t.priority', 'DESC')
      ->orderBy('t.created', 'ASC');

    if ($status_filter !== 'all') {
      $query->condition('t.status', $status_filter);
    }

    $tickets = $query->extend('PagerDefault')->limit(25)->execute()->fetchAll();

    $assignee_uids = array_filter(array_unique(array_column((array) $tickets, 'assignee_uid')));
    $assignees     = $assignee_uids ? user_load_multiple($assignee_uids) : array();

    $rows = array();
    foreach ($tickets as $ticket) {
      $tlabel       = droptickets_ticket_label($ticket, $project);
      $type_label   = isset($type_options[$ticket->type]) ? $type_options[$ticket->type] : $ticket->type;
      $status_label = isset($status_options[$ticket->status]) ? $status_options[$ticket->status] : $ticket->status;
      $assignee     = (isset($assignees[$ticket->assignee_uid]) && $ticket->assignee_uid)
        ? check_plain($assignees[$ticket->assignee_uid]->name) : '—';
      $priority_class = 'drop-priority-' . drop_base_priority_class($ticket->priority);

      $rows[] = array(
        'data' => array(
          array('data' => '<span class="droptickets-ticket-id">' . check_plain($tlabel) . '</span>'),
          array('data' => l($ticket->title, 'drop/tickets/' . $ticket->tid)),
          array('data' => '<span class="droptickets-type droptickets-type--' . check_plain($ticket->type) . '">' . check_plain($type_label) . '</span>'),
          array('data' => '<span class="droptickets-status droptickets-status--' . check_plain($ticket->status) . '">' . check_plain($status_label) . '</span>'),
          array('data' => $assignee),
          array('data' => l(t('Edit'), 'drop/tickets/' . $ticket->tid . '/edit')),
        ),
        'class' => array($priority_class),
      );
    }

    $ticket_count_q = db_select('drop_ticket', 't')
      ->condition('t.pid', $project->pid);
    $ticket_count = $ticket_count_q->countQuery()->execute()->fetchField();

    $add_link = l(t('+ New ticket'), 'drop/tickets/add',
      array('query' => array('pid' => $project->pid), 'attributes' => array('class' => array('drop-section-add'))));

    $tickets_weight = isset($display['tickets']['weight']) ? $display['tickets']['weight'] : 20;
    $output['ticket_tabs'] = array(
      '#markup' => '<div class="drop-section-header">'
        . '<h2>' . t('Tickets (@total)', array('@total' => $ticket_count)) . '</h2>'
        . '<div class="drop-section-add-group">' . $add_link . '</div>'
        . '</div>'
        . '<div class="dropcrm-project-status-tabs">' . $tabs . '</div>',
      '#weight' => $tickets_weight,
    );

    if (empty($rows)) {
      $rows[] = array(
        array('data' => t('No tickets yet.'), 'colspan' => 6, 'class' => array('droptickets-empty')),
      );
    }

    $output['ticket_table'] = array(
      '#theme'      => 'table',
      '#header'     => array(t('ID'), t('Title'), t('Type'), t('Status'), t('Assignee'), t('Actions')),
      '#rows'       => $rows,
      '#attributes' => array('class' => array('droptickets-list')),
      '#weight'     => $tickets_weight + 1,
    );
    $output['pager'] = array('#theme' => 'pager', '#weight' => $tickets_weight + 2);
  }

  $output['back'] = array(
    '#markup' => '<p>' . l(t('← All projects'), 'drop/projects') . '</p>',
    '#weight' => 20,
  );

  return $output;
}

// ---------------------------------------------------------------------------
// Project add/edit form
// ---------------------------------------------------------------------------

/**
 * Form: add or edit a project.
 *
 * @param DropProject|null $project
 *   Existing project for editing, or NULL to add.
 */
function drop_base_project_form($form, &$form_state, $project = NULL) {
  $is_edit = !empty($project);

  $form['#project'] = $project;

  $form['name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Project name'),
    '#default_value' => $is_edit ? $project->name : '',
    '#required'      => TRUE,
    '#maxlength'     => 255,
  );

  // Contact autocomplete — only when CRM is enabled.
  if (module_exists('dropcrm')) {
    $default_contact = '';
    if ($is_edit && !empty($project->cid)) {
      $contacts = entity_load('crm_contact', array((int) $project->cid));
      if ($contacts) {
        $contact = reset($contacts);
        $default_contact = $contact->name . ' [cid:' . $project->cid . ']';
      }
    }
    $form['cid_autocomplete'] = array(
      '#type'              => 'textfield',
      '#title'             => t('Contact'),
      '#default_value'     => $default_contact,
      '#autocomplete_path' => 'drop/crm/autocomplete/contacts',
      '#description'       => t('Optionally link this project to a CRM contact.'),
      '#maxlength'         => 300,
    );
  }

  $form['site_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Website URL'),
    '#default_value' => $is_edit && !empty($project->site_url) ? $project->site_url : '',
    '#maxlength'     => 255,
    '#description'   => t('The primary URL for this project (e.g. https://example.com). Used for uptime monitoring.'),
    '#placeholder'   => 'https://',
  );

  $form['platform'] = array(
    '#type'          => 'select',
    '#title'         => t('Platform'),
    '#options'       => array(
      ''          => t('— Select —'),
      'backdrop'  => t('Backdrop CMS'),
      'drupal'    => t('Drupal'),
      'wordpress' => t('WordPress'),
      'other'     => t('Other'),
    ),
    '#default_value' => $is_edit && !empty($project->platform) ? $project->platform : '',
    '#description'   => t('Determines which health check URL is used for uptime monitoring.'),
  );

  $form['health_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Health check URL'),
    '#default_value' => $is_edit && !empty($project->health_url) ? $project->health_url : '',
    '#maxlength'     => 255,
    '#description'   => t('Optional. A URL that requires a live backend (not cached). Leave empty to auto-detect based on platform.'),
    '#placeholder'   => t('e.g. https://example.com/user/login'),
  );

  $form['gentle_monitor'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Gentle monitoring'),
    '#default_value' => $is_edit && !empty($project->gentle_monitor) ? $project->gentle_monitor : 0,
    '#description'   => t('Use a lighter check for sites with strict bot protection. Checks the homepage with a browser-like request instead of the health URL.'),
  );

  $form['check_interval'] = array(
    '#type'          => 'select',
    '#title'         => t('Check interval'),
    '#options'       => array(
      5  => t('Every 5 minutes'),
      10 => t('Every 10 minutes'),
      15 => t('Every 15 minutes'),
    ),
    '#default_value' => $is_edit && !empty($project->check_interval) ? $project->check_interval : 5,
    '#description'   => t('How often to check uptime. Gentle-monitored sites may benefit from longer intervals.'),
  );

  $form['active'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Active'),
    '#default_value' => $is_edit ? $project->active : 1,
    '#description'   => t('Inactive projects are hidden from the time entry form.'),
  );

  if ($is_edit) {
    $form['pid'] = array('#type' => 'hidden', '#value' => $project->pid);
  }

  // Attach Field API fields (custom fields added via Manage Fields).
  $entity = $is_edit ? $project : entity_create('drop_project', array());
  field_attach_form('drop_project', $entity, $form, $form_state);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Update project') : t('Add project'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'drop/projects'),
  );

  return $form;
}

/**
 * Validation handler for drop_base_project_form().
 */
function drop_base_project_form_validate($form, &$form_state) {
  $name = trim($form_state['values']['name']);
  if ($name === '') {
    return;
  }
  $pid = !empty($form_state['values']['pid']) ? (int) $form_state['values']['pid'] : 0;

  $query = db_select('drop_project', 'p')
    ->fields('p', array('pid', 'name'))
    ->condition('p.name', $name);
  if ($pid) {
    $query->condition('p.pid', $pid, '!=');
  }
  $existing = $query->execute()->fetchObject();
  if ($existing) {
    $link = l($existing->name, 'drop/projects/' . $existing->pid);
    form_set_error('name', t('A project named "!name" already exists: !link', array(
      '!name' => check_plain($name),
      '!link' => $link,
    )));
  }
}

/**
 * Submit handler for drop_base_project_form().
 */
function drop_base_project_form_submit($form, &$form_state) {
  $values = $form_state['values'];

  if (!empty($values['pid'])) {
    $projects = entity_load('drop_project', array($values['pid']));
    $project  = $projects ? reset($projects) : entity_create('drop_project', array());
  }
  else {
    $project = entity_create('drop_project', array());
  }

  $project->name            = trim($values['name']);
  $project->site_url        = trim($values['site_url']);
  $project->platform        = $values['platform'];
  $project->health_url      = trim($values['health_url']);
  $project->gentle_monitor  = (int) $values['gentle_monitor'];
  $project->check_interval  = (int) $values['check_interval'];
  $project->active          = (int) $values['active'];

  // Parse cid from autocomplete value (only present when CRM is enabled).
  if (isset($values['cid_autocomplete'])) {
    $raw = trim($values['cid_autocomplete']);
    if ($raw && preg_match('/\[cid:(\d+)\]/', $raw, $m)) {
      $project->cid = (int) $m[1];
    }
    else {
      $project->cid = NULL;
    }
  }

  // Save Field API values.
  field_attach_submit('drop_project', $project, $form, $form_state);

  $project->save();

  backdrop_set_message(t('Project %name saved.', array('%name' => $project->name)));
  $form_state['redirect'] = 'drop/projects';
}

// ---------------------------------------------------------------------------
// Project delete
// ---------------------------------------------------------------------------

/**
 * Form: confirm deletion of a project.
 */
function drop_base_project_delete_form($form, &$form_state, DropProject $project) {
  $form['#project'] = $project;
  return confirm_form(
    $form,
    t('Delete the project %name?', array('%name' => $project->name)),
    'drop/projects',
    t('All time entries for this project will remain but will show as unknown project. This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for drop_base_project_delete_form().
 */
function drop_base_project_delete_form_submit($form, &$form_state) {
  $project = $form['#project'];
  $project->delete();
  backdrop_set_message(t('Project %name deleted.', array('%name' => $project->name)));
  $form_state['redirect'] = 'drop/projects';
}
