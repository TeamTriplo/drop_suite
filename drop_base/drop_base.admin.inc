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
  $display = field_extra_fields_get_display('drop_project', 'drop_project', 'default');

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

  // Build a single render array for all fields (base properties + Field API).
  // Each element gets a #weight from the Manage Display settings so they
  // interleave correctly when the site architect reorders them.
  $fields_output = array(
    '#prefix' => '<div class="drop-meta-section drop-meta-fields">',
    '#suffix' => '</div>',
  );

  // Prefix.
  if (!empty($project->prefix) && (!isset($display['prefix']) || $display['prefix']['visible'])) {
    $fields_output['prefix'] = array(
      '#markup' => '<div class="drop-meta-row">'
        . '<span class="drop-meta-label">' . t('Prefix') . '</span>'
        . '<span class="drop-meta-value"><code>' . check_plain($project->prefix) . '</code></span>'
        . '</div>',
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
    $fields_output['contact'] = array(
      '#markup' => '<div class="drop-meta-row">'
        . '<span class="drop-meta-label">' . t('Contact') . '</span>'
        . '<span class="drop-meta-value">' . $contact_html . '</span>'
        . '</div>',
      '#weight' => isset($display['contact']['weight']) ? $display['contact']['weight'] : 0,
    );
  }

  // Site URL.
  if (!empty($project->site_url) && (!isset($display['site_url']) || $display['site_url']['visible'])) {
    $fields_output['site_url'] = array(
      '#markup' => '<div class="drop-meta-row">'
        . '<span class="drop-meta-label">' . t('Website') . '</span>'
        . '<span class="drop-meta-value">' . l(check_plain($project->site_url), $project->site_url, array('external' => TRUE)) . '</span>'
        . '</div>',
      '#weight' => isset($display['site_url']['weight']) ? $display['site_url']['weight'] : 5,
    );
  }

  // Platform.
  if (!empty($project->platform) && (!isset($display['platform']) || $display['platform']['visible'])) {
    $platform_labels = drop_base_platform_options();
    $platform_text = isset($platform_labels[$project->platform]) ? $platform_labels[$project->platform] : check_plain($project->platform);
    $fields_output['platform'] = array(
      '#markup' => '<div class="drop-meta-row">'
        . '<span class="drop-meta-label">' . t('Platform') . '</span>'
        . '<span class="drop-meta-value">' . $platform_text . '</span>'
        . '</div>',
      '#weight' => isset($display['platform']['weight']) ? $display['platform']['weight'] : 6,
    );
  }

  // Site health — from Drop Monitor agent data.
  if (module_exists('dropmonitor') && !empty($project->server_sid) && !empty($project->agent_site_key)
    && (!isset($display['site_health']) || $display['site_health']['visible'])) {
    $health_html = _drop_base_project_health_card($project);
    if ($health_html) {
      $fields_output['site_health'] = array(
        '#markup' => $health_html,
        '#weight' => isset($display['site_health']['weight']) ? $display['site_health']['weight'] : 8,
      );
    }
  }

  // Uptime summary — from Drop Monitor uptime checks.
  if (module_exists('dropmonitor') && !empty($project->site_url)
    && (!isset($display['uptime']) || $display['uptime']['visible'])) {
    $uptime_html = _drop_base_project_uptime_summary($project);
    if ($uptime_html) {
      $fields_output['uptime'] = array(
        '#markup' => $uptime_html,
        '#weight' => isset($display['uptime']['weight']) ? $display['uptime']['weight'] : 9,
      );
    }
  }

  // Status (archived indicator).
  if (!$project->active && (!isset($display['status']) || $display['status']['visible'])) {
    $fields_output['status'] = array(
      '#markup' => '<div class="drop-meta-row">'
        . '<span class="drop-meta-label">' . t('Status') . '</span>'
        . '<span class="drop-meta-value"><span class="dropcrm-project-archived">' . t('Archived') . '</span></span>'
        . '</div>',
      '#weight' => isset($display['status']['weight']) ? $display['status']['weight'] : 10,
    );
  }

  // Merge in Field API fields so they sort alongside base properties.
  $field_output = field_attach_view('drop_project', $project, 'default');
  if (!empty($field_output)) {
    foreach (element_children($field_output) as $field_name) {
      $fields_output[$field_name] = $field_output[$field_name];
    }
  }

  $output['fields'] = $fields_output;

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

  $form['#attached']['css'][] = backdrop_get_path('module', 'drop_base') . '/drop_base.css';

  if ($is_edit) {
    backdrop_set_breadcrumb(array(
      l(t('Home'), '<front>'),
      l(t('Drop Suite'), 'drop'),
      l(t('Projects'), 'drop/projects'),
      l($project->name, 'drop/projects/' . $project->pid),
    ));
  }
  else {
    backdrop_set_breadcrumb(array(
      l(t('Home'), '<front>'),
      l(t('Drop Suite'), 'drop'),
      l(t('Projects'), 'drop/projects'),
    ));
  }

  $form['#project'] = $project;

  $form['name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Project name'),
    '#default_value' => $is_edit ? $project->name : '',
    '#required'      => TRUE,
    '#maxlength'     => 255,
    '#weight'        => -20,
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
      '#weight'            => -18,
    );
  }

  $form['site_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Website URL'),
    '#default_value' => $is_edit && !empty($project->site_url) ? $project->site_url : '',
    '#maxlength'     => 255,
    '#description'   => t('The primary URL for this project (e.g. https://example.com). Used for uptime monitoring.'),
    '#placeholder'   => 'https://',
    '#weight'        => -16,
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
    '#weight'        => -14,
  );

  $form['health_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Health check URL'),
    '#default_value' => $is_edit && !empty($project->health_url) ? $project->health_url : '',
    '#maxlength'     => 255,
    '#description'   => t('Optional. A URL that requires a live backend (not cached). Leave empty to auto-detect based on platform.'),
    '#placeholder'   => t('e.g. https://example.com/user/login'),
    '#weight'        => -12,
  );

  $form['gentle_monitor'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Gentle monitoring'),
    '#default_value' => $is_edit && !empty($project->gentle_monitor) ? $project->gentle_monitor : 0,
    '#description'   => t('Use a lighter check for sites with strict bot protection. Checks the homepage with a browser-like request instead of the health URL.'),
    '#weight'        => -10,
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
    '#weight'        => -8,
  );

  // Server monitoring link — only when Drop Monitor is enabled.
  if (module_exists('dropmonitor')) {
    $servers = dropmonitor_get_servers(TRUE);
    $server_options = array('' => t('— None —'));
    foreach ($servers as $srv) {
      $server_options[$srv->sid] = $srv->label;
    }

    $default_sid = $is_edit && !empty($project->server_sid) ? $project->server_sid : '';

    $form['monitoring'] = array(
      '#type' => 'fieldset',
      '#title' => t('Server monitoring'),
      '#collapsible' => TRUE,
      '#collapsed' => empty($default_sid),
      '#weight' => 40,
      '#tree' => TRUE,
    );

    $form['monitoring']['server_sid'] = array(
      '#type' => 'select',
      '#title' => t('Server'),
      '#options' => $server_options,
      '#default_value' => $default_sid,
      '#description' => t('Link this project to a monitored server to display site health data.'),
      '#ajax' => array(
        'callback' => 'drop_base_project_form_site_keys_ajax',
        'wrapper' => 'agent-site-key-wrapper',
      ),
    );

    // Determine which server to load site keys for.
    $selected_sid = isset($form_state['values']['monitoring']['server_sid'])
      ? $form_state['values']['monitoring']['server_sid']
      : $default_sid;

    $site_key_options = array('' => t('— None —'));
    if (!empty($selected_sid)) {
      $site_key_options += _drop_base_get_agent_site_keys($selected_sid);
    }

    $form['monitoring']['agent_site_key'] = array(
      '#type' => 'select',
      '#title' => t('Monitored site'),
      '#options' => $site_key_options,
      '#default_value' => $is_edit && !empty($project->agent_site_key) ? $project->agent_site_key : '',
      '#description' => t('Select the site on this server that corresponds to this project.'),
      '#prefix' => '<div id="agent-site-key-wrapper">',
      '#suffix' => '</div>',
    );
  }

  $form['active'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Active'),
    '#default_value' => $is_edit ? $project->active : 1,
    '#description'   => t('Inactive projects are hidden from the time entry form.'),
    '#weight'        => 50,
  );

  if ($is_edit) {
    $form['pid'] = array('#type' => 'hidden', '#value' => $project->pid);
  }

  // Attach Field API fields (custom fields added via Manage Fields).
  $entity = $is_edit ? $project : entity_create('drop_project', array());
  field_attach_form('drop_project', $entity, $form, $form_state);

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Save') : t('Add project'),
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

  // Server monitoring link.
  if (module_exists('dropmonitor') && isset($values['monitoring'])) {
    $project->server_sid = !empty($values['monitoring']['server_sid']) ? (int) $values['monitoring']['server_sid'] : NULL;
    $project->agent_site_key = !empty($values['monitoring']['agent_site_key']) ? $values['monitoring']['agent_site_key'] : '';
  }

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

/**
 * AJAX callback: returns the agent_site_key dropdown for the selected server.
 */
function drop_base_project_form_site_keys_ajax($form, $form_state) {
  return $form['monitoring']['agent_site_key'];
}

/**
 * Returns available site keys from a server's latest agent check data.
 *
 * @param int $sid
 *   Server ID.
 *
 * @return array
 *   Associative array of site_key => site_key for use as #options.
 */
function _drop_base_get_agent_site_keys($sid) {
  $options = array();

  $check = db_select('dropmonitor_check', 'c')
    ->fields('c', array('data'))
    ->condition('sid', $sid)
    ->condition('check_type', 'agent')
    ->execute()
    ->fetchField();

  if ($check) {
    $data = json_decode($check, TRUE);
    if (!empty($data['sites'])) {
      foreach (array_keys($data['sites']) as $key) {
        $options[$key] = $key;
      }
      ksort($options);
    }
  }

  return $options;
}

/**
 * Renders a site health card from Drop Monitor agent data for a project.
 *
 * @param object $project
 *   The project object with server_sid and agent_site_key set.
 *
 * @return string|FALSE
 *   HTML string, or FALSE if no data available.
 */
function _drop_base_project_health_card($project) {
  $check = db_select('dropmonitor_check', 'c')
    ->fields('c', array('data', 'checked'))
    ->condition('sid', $project->server_sid)
    ->condition('check_type', 'agent')
    ->execute()
    ->fetchObject();

  if (!$check) {
    return FALSE;
  }

  $data = json_decode($check->data, TRUE);
  if (empty($data['sites'][$project->agent_site_key])) {
    return FALSE;
  }

  $site = $data['sites'][$project->agent_site_key];
  $items = array();

  // HTTP status.
  if (isset($site['http']['status'])) {
    $class = $site['http']['status'] === 'up' ? 'dropmonitor-up' : 'dropmonitor-down';
    $value = '<span class="' . $class . '">' . strtoupper($site['http']['status']) . '</span>';
    if (isset($site['http']['response_ms'])) {
      $value .= ' (' . $site['http']['response_ms'] . 'ms)';
    }
    $items[] = '<span class="drop-meta-label">' . t('HTTP') . '</span> ' . $value;
  }

  // Core version and update status.
  if (!empty($site['core_update'])) {
    $cu = $site['core_update'];
    $version = isset($cu['current']) ? check_plain($cu['current']) : '';
    if (!empty($cu['update_available']) && !empty($cu['latest'])) {
      $latest = check_plain($cu['latest']);
      if (!empty($cu['security'])) {
        $version .= ' &rarr; ' . $latest . ' <span class="dropmonitor-site-security">(' . t('security update') . ')</span>';
      }
      else {
        $version .= ' &rarr; ' . $latest . ' <span class="dropmonitor-site-update">(' . t('update available') . ')</span>';
      }
    }
    $items[] = '<span class="drop-meta-label">' . t('Core') . '</span> ' . $version;
  }

  // Database size.
  if (isset($site['db_size_mb'])) {
    $items[] = '<span class="drop-meta-label">' . t('Database') . '</span> ' . $site['db_size_mb'] . ' MB';
  }

  // Watchdog errors.
  if (isset($site['watchdog_errors_24h'])) {
    $value = $site['watchdog_errors_24h'];
    if ($value > 0) {
      $value = '<span class="dropmonitor-site-errors">' . $value . '</span>';
    }
    $items[] = '<span class="drop-meta-label">' . t('Errors (24h)') . '</span> ' . $value;
  }

  // Cron.
  if (isset($site['cron_last'])) {
    $age = REQUEST_TIME - $site['cron_last'];
    $cron_text = format_interval($age, 1) . ' ' . t('ago');
    if ($age > 86400) {
      $cron_text = '<span class="dropmonitor-site-errors">' . $cron_text . '</span>';
    }
    $items[] = '<span class="drop-meta-label">' . t('Last cron') . '</span> ' . $cron_text;
  }

  if (empty($items)) {
    return FALSE;
  }

  // Determine if there's an actionable issue that warrants auto-opening.
  $has_issue = FALSE;
  if (!empty($site['core_update']['security'])) {
    $has_issue = TRUE;
  }
  if (isset($site['watchdog_errors_24h']) && $site['watchdog_errors_24h'] > 0) {
    $has_issue = TRUE;
  }
  if (isset($site['http']['status']) && $site['http']['status'] !== 'up') {
    $has_issue = TRUE;
  }
  if (isset($site['cron_last']) && (REQUEST_TIME - $site['cron_last']) > 86400) {
    $has_issue = TRUE;
  }

  // Build summary text for the collapsed state — focus on update status.
  if (!empty($site['core_update']['security'])) {
    $summary_status = ' &mdash; <span class="dropmonitor-site-security">' . t('Needs security update') . '</span>';
  }
  elseif (!empty($site['core_update']['update_available'])) {
    $summary_status = ' &mdash; <span class="dropmonitor-site-update">' . t('Update available') . '</span>';
  }
  else {
    $summary_status = ' &mdash; ' . t('Up to date');
  }

  // Link to server detail page.
  $server = dropmonitor_server_load($project->server_sid);
  $server_link = $server ? l(check_plain($server->label), 'drop/monitor/server/' . $project->server_sid) : '';

  $age_text = format_interval(REQUEST_TIME - $check->checked, 1) . ' ' . t('ago');

  $open_attr = $has_issue ? ' open' : '';
  $output = '<details class="drop-details"' . $open_attr . '>';
  $output .= '<summary class="drop-details-summary">' . t('Site health') . $summary_status . '</summary>';
  $output .= '<div class="drop-details-body">' . implode('<br>', $items) . '</div>';
  $output .= '<div class="drop-details-meta">';
  if ($server_link) {
    $output .= t('Server: !server', array('!server' => $server_link)) . ' &middot; ';
  }
  $output .= t('Updated @time', array('@time' => $age_text));
  $output .= '</div>';
  $output .= '</details>';

  return $output;
}

/**
 * Renders an uptime summary for a project.
 *
 * Shows current status, 14-day uptime percentage, and a link to the detail
 * page. Uses the same data as the Uptime dashboard.
 *
 * @param object $project
 *   The project object (must have pid and site_url).
 *
 * @return string|FALSE
 *   HTML string, or FALSE if no uptime data available.
 */
function _drop_base_project_uptime_summary($project) {
  module_load_include('inc', 'dropmonitor', 'dropmonitor.uptime');

  $pct = dropmonitor_uptime_percentage($project->pid, 14);

  // Get the latest check.
  $latest = db_select('dropmonitor_uptime', 'u')
    ->fields('u')
    ->condition('pid', $project->pid)
    ->orderBy('checked', 'DESC')
    ->range(0, 1)
    ->execute()
    ->fetchObject();

  if (!$latest) {
    return FALSE;
  }

  $items = array();

  // Current status.
  $is_up = (int) $latest->status;
  if ($is_up) {
    $status = '<span class="dropmonitor-up">' . t('UP') . '</span>';
    $status .= ' (' . $latest->response_code . ', ' . $latest->response_ms . 'ms)';
  }
  else {
    $status = '<span class="dropmonitor-down">' . t('DOWN') . '</span>';
    $status .= ' (' . $latest->response_code . ')';
  }
  $items[] = '<span class="drop-meta-label">' . t('Status') . '</span> ' . $status;

  // 14-day uptime.
  if ($pct !== NULL) {
    $pct_class = $pct >= 99.5 ? 'dropmonitor-up' : ($pct >= 95 ? 'dropmonitor-site-update' : 'dropmonitor-down');
    $items[] = '<span class="drop-meta-label">' . t('Uptime (14d)') . '</span> '
      . '<span class="' . $pct_class . '">' . $pct . '%</span>';
  }

  // Last checked.
  $items[] = '<span class="drop-meta-label">' . t('Last check') . '</span> '
    . format_interval(REQUEST_TIME - $latest->checked, 1) . ' ' . t('ago');

  // Summary status for the collapsed state.
  $summary_parts = array();
  if ($is_up) {
    $summary_parts[] = '<span class="dropmonitor-up">' . t('UP') . '</span>';
  }
  else {
    $summary_parts[] = '<span class="dropmonitor-down">' . t('DOWN') . '</span>';
  }
  if ($pct !== NULL) {
    $summary_parts[] = $pct . '%';
  }
  $summary_status = ' &mdash; ' . implode(' &middot; ', $summary_parts);

  // Auto-open if site is down or uptime is poor.
  $has_issue = !$is_up || ($pct !== NULL && $pct < 99.5);
  $open_attr = $has_issue ? ' open' : '';

  $output = '<details class="drop-details"' . $open_attr . '>';
  $output .= '<summary class="drop-details-summary">' . t('Uptime') . $summary_status . '</summary>';
  $output .= '<div class="drop-details-body">' . implode('<br>', $items) . '</div>';
  $output .= '<div class="drop-details-meta">';
  $output .= l(t('View uptime log'), 'drop/monitor/uptime/' . $project->pid);
  $output .= '</div>';
  $output .= '</details>';

  return $output;
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

// ---------------------------------------------------------------------------
// Drop Base settings form
// ---------------------------------------------------------------------------

/**
 * Form callback for Drop Base settings.
 */
function drop_base_settings_form($form, &$form_state) {
  $config = config('drop_base.settings');

  $form['narrow_page_width'] = array(
    '#type'          => 'number',
    '#title'         => t('Narrow page width'),
    '#description'   => t('Maximum width in pixels for pages that work better at a narrower width (e.g. Meeting, Settings). Set to 0 for no limit.'),
    '#default_value' => $config->get('narrow_page_width'),
    '#min'           => 0,
    '#max'           => 2400,
    '#field_suffix'  => 'px',
  );

  $form['features'] = array(
    '#type' => 'fieldset',
    '#title' => t('Features'),
    '#collapsible' => TRUE,
  );
  $form['features']['meeting_enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable meeting agenda'),
    '#description' => t('Show the Meeting page in the navigation bar.'),
    '#default_value' => $config->get('meeting_enabled') !== FALSE ? $config->get('meeting_enabled') : TRUE,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save settings'),
  );

  return $form;
}

/**
 * Submit handler for drop_base_settings_form().
 */
function drop_base_settings_form_submit($form, &$form_state) {
  $config = config('drop_base.settings');
  $config->set('narrow_page_width', (int) $form_state['values']['narrow_page_width']);
  $config->set('meeting_enabled', (bool) $form_state['values']['meeting_enabled']);
  $config->save();
  backdrop_set_message(t('Settings saved.'));
}
