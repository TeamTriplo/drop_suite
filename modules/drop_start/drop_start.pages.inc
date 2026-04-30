<?php
/**
 * @file
 * Page callbacks and forms for Drop Start.
 */

// ---------------------------------------------------------------------------
// Overview page
// ---------------------------------------------------------------------------

/**
 * Page callback: Drop Start overview.
 *
 * Renders one of three modes based on wizard state:
 *   - not_started: welcome splash + Begin setup button.
 *   - in_progress: resume card pointing at current step + step index.
 *   - complete:    summary of configured state + re-run links.
 */
function drop_start_overview_page() {
  module_load_include('inc', 'drop_start', 'drop_start.wizard');
  backdrop_add_css(backdrop_get_path('module', 'drop_start') . '/drop_start.wizard.css');
  $overall = drop_start_wizard_overall_state();

  if ($overall === 'not_started') {
    return _drop_start_overview_not_started();
  }
  if ($overall === 'in_progress') {
    return _drop_start_overview_in_progress();
  }
  return _drop_start_overview_complete();
}

/**
 * Overview renderer: never-started state.
 */
function _drop_start_overview_not_started() {
  backdrop_set_title(t('Set up Drop Suite'));

  $steps   = drop_start_get_steps();
  $first   = $steps ? reset($steps) : NULL;
  $entry   = $first ? 'drop/start/step/' . $first['_key'] : NULL;

  $output  = '<div class="drop-start-overview drop-start-overview--welcome">';
  $output .= '<p>' . t('This wizard walks you through the essentials for setting up a Drop Suite site: pick a theme, populate demo content, enable the modules you want, and configure your front page.') . '</p>';
  $output .= '<p>' . t('Each step is optional (except the bookends). You can skip anything, and you can re-run the wizard later from Settings.') . '</p>';

  if ($entry) {
    $output .= '<p>'
      . l(t('Begin setup'), $entry, array('attributes' => array('class' => array('button', 'button-primary'))))
      . '</p>';
  }

  $output .= _drop_start_overview_step_list();
  $output .= '</div>';
  return $output;
}

/**
 * Overview renderer: in-progress state.
 */
function _drop_start_overview_in_progress() {
  backdrop_set_title(t('Set up Drop Suite'));
  $state = drop_start_wizard_state_get();
  $steps = drop_start_get_steps();

  $current      = isset($steps[$state['current']]) ? $steps[$state['current']] : NULL;
  $resume_path  = $current ? 'drop/start/step/' . $current['_key'] : NULL;

  $output  = '<div class="drop-start-overview drop-start-overview--resume">';
  if ($current) {
    $output .= '<p>' . t('You left off at: %step.', array('%step' => $current['title'])) . '</p>';
    $output .= '<p>' . l(t('Resume setup'), $resume_path, array(
      'attributes' => array('class' => array('button', 'button-primary')),
    )) . ' ' . l(t('Restart wizard'), 'drop/start/restart') . '</p>';
  }
  else {
    $output .= '<p>' . t('Setup is in progress. Jump to any step below.') . '</p>';
  }

  $output .= _drop_start_overview_step_list();
  $output .= '</div>';
  return $output;
}

/**
 * Overview renderer: complete state.
 */
function _drop_start_overview_complete() {
  backdrop_set_title(t('Drop Suite setup'));

  $output  = '<div class="drop-start-overview drop-start-overview--complete">';
  $output .= '<p>' . t('Your Drop Suite site is set up. You can re-run any step below, or restart the whole wizard.') . '</p>';
  $output .= '<p>' . l(t('Restart wizard'), 'drop/start/restart', array(
    'attributes' => array('class' => array('button')),
  )) . '</p>';
  $output .= _drop_start_overview_step_list();

  // Preserve the original sample-content management surface at the bottom.
  $total  = drop_start_content_total();
  $counts = drop_start_content_counts();
  if ($total > 0) {
    $output .= '<h3>' . t('Sample content on this site') . '</h3>';
    $rows = array();
    $type_labels = _drop_start_entity_type_labels();
    foreach ($counts as $type => $count) {
      $label = isset($type_labels[$type]) ? $type_labels[$type] : $type;
      $rows[] = array(check_plain($label), $count);
    }
    $output .= theme('table', array(
      'header' => array(t('Type'), t('Count')),
      'rows'   => $rows,
    ));
    $output .= '<p>'
      . l(t('Delete sample content'), 'drop/start/delete', array('attributes' => array('class' => array('button', 'button-danger'))))
      . ' '
      . l(t('Generate more'), 'drop/start/generate', array('attributes' => array('class' => array('button'))))
      . '</p>';
  }

  $output .= '</div>';
  return $output;
}

/**
 * Renders a table of all wizard steps with their status + jump links.
 */
function _drop_start_overview_step_list() {
  $steps = drop_start_get_steps();
  if (empty($steps)) {
    return '';
  }

  $status_label = array(
    'configured'  => t('Done'),
    'partial'     => t('Partial'),
    'not_started' => t('Not started'),
  );

  $rows = array();
  foreach ($steps as $full_key => $step) {
    $status = drop_start_step_status($full_key);
    $link   = l($step['title'], 'drop/start/step/' . $step['_key']);
    $desc   = !empty($step['description']) ? check_plain($step['description']) : '';
    $rows[] = array(
      array('data' => $link . ($desc ? '<br /><small>' . $desc . '</small>' : ''), 'class' => array('drop-start-step-title')),
      array('data' => $status_label[$status], 'class' => array('drop-start-step-status', 'drop-start-step-status--' . $status)),
    );
  }

  return '<h3>' . t('Setup steps') . '</h3>'
    . theme('table', array(
      'header' => array(t('Step'), t('Status')),
      'rows'   => $rows,
    ));
}

/**
 * Returns human-readable labels for tracked entity types.
 */
function _drop_start_entity_type_labels() {
  return array(
    'user'                   => t('People'),
    'drop_project'           => t('Projects'),
    'drop_ticket'            => t('Tickets'),
    'crm_contact'            => t('Contacts'),
    'crm_membership'         => t('Memberships'),
    'crm_membership_term'    => t('Membership terms'),
    'crm_membership_payment' => t('Membership payments'),
    'drop_task'              => t('Tasks'),
    'drop_goal'              => t('Goals'),
    'drop_time_entry'        => t('Time entries'),
    'node'                   => t('Docs'),
  );
}

// ---------------------------------------------------------------------------
// Generate form
// ---------------------------------------------------------------------------

/**
 * Form: generate sample content.
 */
function drop_start_generate_form($form, &$form_state) {
  $available = drop_start_available_modules();
  $total     = drop_start_content_total();

  if (empty($available)) {
    $form['empty'] = array(
      '#markup' => '<p>' . t('No Drop Suite modules are enabled. Enable some modules first.') . '</p>',
    );
    return $form;
  }

  if ($total > 0) {
    $form['warning'] = array(
      '#markup' => '<div class="messages warning"><p>'
        . t('Sample content already exists (@count items). Generating again will create additional content.', array('@count' => $total))
        . '</p></div>',
    );
  }

  $form['intro'] = array(
    '#markup' => '<p>' . t('Generate sample content using an Olympics event planning theme. Content will be created for the modules selected below.') . '</p>',
  );

  $options = array();
  foreach ($available as $module => $label) {
    $options[$module] = $label;
  }

  $form['modules'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Generate content for'),
    '#options'       => $options,
    '#default_value' => array_keys($options),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Generate sample content'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'drop/start'),
  );

  return $form;
}

/**
 * Submit handler: generate sample content.
 */
function drop_start_generate_form_submit($form, &$form_state) {
  $selected = array_filter($form_state['values']['modules']);
  global $user;
  $uid = $user->uid;

  // Create sample team members first so their UIDs can be used elsewhere.
  $sample_uids = _drop_start_generate_users();

  // Projects are created first since tickets and time entries depend on them.
  $project_ids = array();
  if (!empty($selected['drop_suite'])) {
    $project_ids = _drop_start_generate_projects($uid);
  }

  // Contacts — needed before tickets if we want to link them.
  $contact_ids = array();
  if (!empty($selected['drop_crm'])) {
    $contact_ids = _drop_start_generate_contacts($uid);
  }

  // Memberships — need contact_ids to attach to.
  if (!empty($selected['drop_crm_membership']) && !empty($contact_ids)) {
    _drop_start_generate_memberships($uid, $contact_ids);
  }

  if (!empty($selected['droptickets']) && !empty($project_ids)) {
    _drop_start_generate_tickets($uid, $project_ids, $sample_uids);
  }

  if (!empty($selected['droptasks'])) {
    _drop_start_generate_tasks($uid);
  }

  if (!empty($selected['dropgoals'])) {
    _drop_start_generate_goals($uid);
  }

  if (!empty($selected['droptime']) && !empty($project_ids)) {
    _drop_start_generate_time_entries($uid, $project_ids);
  }

  if (!empty($selected['drop_docs'])) {
    _drop_start_generate_docs($uid);
  }

  $total = drop_start_content_total();
  backdrop_set_message(t('Sample content generated successfully. @count items created.', array('@count' => $total)));
  $form_state['redirect'] = 'drop/start';
}

// ---------------------------------------------------------------------------
// Delete form
// ---------------------------------------------------------------------------

/**
 * Form: delete sample content.
 */
function drop_start_delete_form($form, &$form_state) {
  $total  = drop_start_content_total();
  $counts = drop_start_content_counts();

  if ($total === 0) {
    $form['empty'] = array(
      '#markup' => '<p>' . t('No sample content to delete.') . '</p>'
        . '<p>' . l(t('Back to Drop Start'), 'drop/start') . '</p>',
    );
    return $form;
  }

  $type_labels = _drop_start_entity_type_labels();
  $items = array();
  foreach ($counts as $type => $count) {
    $label = isset($type_labels[$type]) ? $type_labels[$type] : $type;
    $items[] = t('@count @type', array('@count' => $count, '@type' => $label));
  }

  return confirm_form(
    $form,
    t('Delete all sample content?'),
    'drop/start',
    '<p>' . t('This will permanently delete the following sample content:') . '</p>'
      . theme('item_list', array('items' => $items))
      . '<p><strong>' . t('This action cannot be undone.') . '</strong></p>',
    t('Delete all sample content'),
    t('Cancel')
  );
}

/**
 * Submit handler: delete sample content.
 */
function drop_start_delete_form_submit($form, &$form_state) {
  $tracked = db_select('drop_start_content', 'c')
    ->fields('c', array('entity_type', 'entity_id'))
    ->orderBy('c.id', 'DESC')
    ->execute()
    ->fetchAll();

  $deleted = 0;
  foreach ($tracked as $row) {
    $success = _drop_start_delete_entity($row->entity_type, (int) $row->entity_id);
    if ($success) {
      $deleted++;
    }
    // Remove tracking regardless — if the entity is already gone, clean up.
    db_delete('drop_start_content')
      ->condition('entity_type', $row->entity_type)
      ->condition('entity_id', $row->entity_id)
      ->execute();
  }

  backdrop_set_message(t('@count items of sample content deleted.', array('@count' => $deleted)));
  $form_state['redirect'] = 'drop/start';
}

/**
 * Delete a single tracked entity.
 *
 * @return bool
 *   TRUE if the entity was deleted, FALSE if it was not found.
 */
function _drop_start_delete_entity($entity_type, $entity_id) {
  if ($entity_type === 'node') {
    $node = node_load($entity_id);
    if ($node) {
      node_delete($entity_id);
      return TRUE;
    }
    return FALSE;
  }

  if ($entity_type === 'user') {
    $account = user_load($entity_id);
    if ($account && $account->uid > 1) {
      user_delete($entity_id);
      return TRUE;
    }
    return FALSE;
  }

  $entities = entity_load($entity_type, array($entity_id));
  if (!empty($entities)) {
    $entity = reset($entities);
    $entity->delete();
    return TRUE;
  }
  return FALSE;
}

// ---------------------------------------------------------------------------
// Content generators — Olympics theme
// ---------------------------------------------------------------------------

/**
 * Generate sample team member accounts.
 *
 * @return array
 *   Associative array keyed by role shorthand ('pm', 'supervisor') with UIDs.
 */
function _drop_start_generate_users() {
  $users = array(
    'pm' => array(
      'name'  => 'sarah.martinez',
      'mail'  => 'sarah.martinez@example.com',
      'label' => 'Sarah Martinez',
      'roles' => array('project_manager', 'team_member'),
    ),
    'supervisor' => array(
      'name'  => 'david.oconnor',
      'mail'  => 'david.oconnor@example.com',
      'label' => 'David O\'Connor',
      'roles' => array('team_member'),
    ),
    'venue_pm' => array(
      'name'  => 'amir.patel',
      'mail'  => 'amir.patel@example.com',
      'label' => 'Amir Patel',
      'roles' => array('project_manager', 'team_member'),
    ),
    'broadcaster' => array(
      'name'  => 'keiko.tanaka',
      'mail'  => 'keiko.tanaka@example.com',
      'label' => 'Keiko Tanaka',
      'roles' => array('team_member'),
    ),
    'athlete_services' => array(
      'name'  => 'lena.novak',
      'mail'  => 'lena.novak@example.com',
      'label' => 'Lena Novak',
      'roles' => array('team_member'),
    ),
    'security' => array(
      'name'  => 'marcus.johnson',
      'mail'  => 'marcus.johnson@example.com',
      'label' => 'Marcus Johnson',
      'roles' => array('team_member'),
    ),
    'logistics' => array(
      'name'  => 'yasmin.khalid',
      'mail'  => 'yasmin.khalid@example.com',
      'label' => 'Yasmin Khalid',
      'roles' => array('team_member'),
    ),
    'media' => array(
      'name'  => 'emma.lindqvist',
      'mail'  => 'emma.lindqvist@example.com',
      'label' => 'Emma Lindqvist',
      'roles' => array('team_member'),
    ),
  );

  $uids = array();
  foreach ($users as $key => $data) {
    // Skip if username already exists.
    if (user_load_by_name($data['name'])) {
      $existing = user_load_by_name($data['name']);
      $uids[$key] = $existing->uid;
      continue;
    }

    // Assign roles.
    $role_names = array('authenticated');
    foreach ($data['roles'] as $role_name) {
      $role = user_role_load($role_name);
      if ($role) {
        $role_names[] = $role->name;
      }
    }

    $account = entity_create('user', array(
      'name'   => $data['name'],
      'mail'   => $data['mail'],
      'pass'   => 'DropSuite2026!',
      'status' => 1,
      'roles'  => $role_names,
    ));
    $account->save();
    drop_start_track('user', $account->uid);
    $uids[$key] = $account->uid;
  }

  return $uids;
}

/**
 * Generate sample projects.
 *
 * @return array
 *   Array of created project IDs.
 */
function _drop_start_generate_projects($uid) {
  $projects = array(
    array('name' => 'Venue Construction', 'prefix' => 'VEN'),
    array('name' => 'Athlete Services', 'prefix' => 'ATH'),
    array('name' => 'Broadcasting', 'prefix' => 'BC'),
    array('name' => 'Security Operations', 'prefix' => 'SEC'),
  );

  $ids = array();
  foreach ($projects as $data) {
    $project = entity_create('drop_project', array(
      'name'   => $data['name'],
      'prefix' => $data['prefix'],
      'active' => 1,
    ));
    $project->save();
    drop_start_track('drop_project', $project->pid);
    $ids[$data['prefix']] = $project->pid;
  }

  return $ids;
}

/**
 * Generate sample CRM contacts.
 *
 * @return array
 *   Array of created contact IDs.
 */
function _drop_start_generate_contacts($uid) {
  // Ensure the tag taxonomy terms we reference below exist. Returns a map
  // of tag name => tid so we can attach them to contacts.
  $tag_tids = _drop_start_ensure_tags(array(
    'Client',
    'Prospect',
    'Partner',
    'Vendor',
  ));

  $orgs = array(
    array('name' => 'International Olympic Committee', 'tags' => array('Client')),
    array('name' => 'Global Sports Broadcasting Inc.', 'tags' => array('Partner')),
    array('name' => 'Atlas Venue Construction',        'tags' => array('Vendor')),
    array('name' => 'SecurePoint Solutions',           'tags' => array('Client', 'Vendor')),
  );

  $ids = array();
  $org_ids = array();
  $has_tag_field = field_info_field('field_drop_tags') !== NULL;

  // Create organizations first so people can reference them.
  $bundles = field_info_bundles('crm_contact');
  if (isset($bundles['crm_organization'])) {
    foreach ($orgs as $org) {
      $contact = entity_create('crm_contact', array(
        'type'   => 'crm_organization',
        'name'   => $org['name'],
        'status' => 1,
      ));
      if ($has_tag_field && !empty($org['tags'])) {
        _drop_start_attach_tags($contact, $org['tags'], $tag_tids);
      }
      $contact->save();
      drop_start_track('crm_contact', $contact->cid);
      $org_ids[] = $contact->cid;
      $ids[] = $contact->cid;
    }
  }

  // People — each linked to an organization via field_crm_employer, plus
  // a varied mix of tags.
  $ioc_id    = isset($org_ids[0]) ? $org_ids[0] : NULL;
  $gsb_id    = isset($org_ids[1]) ? $org_ids[1] : NULL;
  $atlas_id  = isset($org_ids[2]) ? $org_ids[2] : NULL;
  $secure_id = isset($org_ids[3]) ? $org_ids[3] : NULL;

  $people = array(
    array('first' => 'Helena',  'last' => 'Kowalski',  'org' => $ioc_id,    'tags' => array('Client')),
    array('first' => 'Marcus',  'last' => 'Chen',      'org' => $atlas_id,  'tags' => array('Vendor')),
    array('first' => 'Fatima',  'last' => 'Al-Rashid', 'org' => $gsb_id,    'tags' => array('Partner')),
    array('first' => 'James',   'last' => 'Okafor',    'org' => $secure_id, 'tags' => array('Prospect')),
    array('first' => 'Sofia',   'last' => 'Petrov',    'org' => $ioc_id,    'tags' => array('Client')),
    array('first' => 'Kenji',   'last' => 'Tanaka',    'org' => $gsb_id,    'tags' => array()),
  );

  foreach ($people as $person) {
    $contact = entity_create('crm_contact', array(
      'type'   => 'crm_person',
      'status' => 1,
    ));
    $contact->field_crm_first_name[LANGUAGE_NONE][0]['value'] = $person['first'];
    $contact->field_crm_last_name[LANGUAGE_NONE][0]['value']  = $person['last'];
    if (!empty($person['org']) && field_info_field('field_crm_employer')) {
      $contact->field_crm_employer[LANGUAGE_NONE][0]['target_id'] = $person['org'];
    }
    if ($has_tag_field && !empty($person['tags'])) {
      _drop_start_attach_tags($contact, $person['tags'], $tag_tids);
    }
    $contact->save();
    drop_start_track('crm_contact', $contact->cid);
    $ids[] = $contact->cid;
  }

  return $ids;
}

/**
 * Ensure each given tag name exists in the drop_tags vocabulary. Creates
 * missing terms. Returns name => tid map for every tag.
 */
function _drop_start_ensure_tags(array $names) {
  $vocab = taxonomy_vocabulary_machine_name_load('drop_tags');
  if (!$vocab) {
    return array();
  }
  $map = array();
  foreach ($names as $name) {
    $existing = taxonomy_get_term_by_name($name, 'drop_tags');
    if (!empty($existing)) {
      $term = reset($existing);
      $map[$name] = (int) $term->tid;
      continue;
    }
    $term = new TaxonomyTerm(array(
      'name'         => $name,
      'vocabulary'   => 'drop_tags',
      'parent'       => array(0),
    ));
    taxonomy_term_save($term);
    $map[$name] = (int) $term->tid;
  }
  return $map;
}

/**
 * Sets field_drop_tags on a contact to the given tag names (resolved via
 * the name => tid map from _drop_start_ensure_tags).
 */
function _drop_start_attach_tags($contact, array $tag_names, array $tag_tids) {
  $items = array();
  foreach ($tag_names as $name) {
    if (!empty($tag_tids[$name])) {
      $items[] = array('tid' => $tag_tids[$name]);
    }
  }
  if ($items) {
    $contact->field_drop_tags[LANGUAGE_NONE] = $items;
  }
}

/**
 * Generate sample memberships across a subset of contacts. Covers the
 * full state-machine surface: active / expiring-soon / grace / lapsed
 * / cancelled / lifetime, plus one multi-term history with payments.
 *
 * Relies on the default "individual" and (when configured) "lifetime"
 * membership types being present — both come with the module's shipped
 * settings.
 */
function _drop_start_generate_memberships($uid, $contact_ids) {
  // Need at least a handful of contacts to attach memberships to.
  if (count($contact_ids) < 6) {
    return;
  }

  // Pick a membership type key that actually exists in settings.
  $types = config_get('drop_crm_membership.settings', 'types');
  if (empty($types) || !is_array($types)) {
    return;
  }
  $default_type = isset($types['individual']) ? 'individual' : key($types);
  $lifetime_type = isset($types['lifetime']) ? 'lifetime' : $default_type;

  // Scenarios: [cid_index, type, end_offset_days, status_override, note]
  $scenarios = array(
    array(2, $default_type,   5,    NULL,                                     'expiring in 5 days'),
    array(3, $default_type,   15,   NULL,                                     'expiring in 15 days'),
    array(4, $default_type,   25,   NULL,                                     'expiring in 25 days'),
    array(5, $default_type,   180,  NULL,                                     'active, mid-term'),
    array(6, $default_type,   -10,  DROPCRM_MEMBERSHIP_STATUS_GRACE,          'in grace'),
    array(7, $default_type,   -45,  DROPCRM_MEMBERSHIP_STATUS_LAPSED,         'lapsed'),
    array(8, $default_type,   365,  DROPCRM_MEMBERSHIP_STATUS_CANCELLED,      'cancelled'),
  );

  $today_ts = strtotime('today');
  foreach ($scenarios as $s) {
    list($i, $type, $offset_days, $status_override, $note) = $s;
    if (!isset($contact_ids[$i])) {
      continue;
    }
    $cid        = $contact_ids[$i];
    $start_ts   = strtotime(($offset_days - 365) . ' days', $today_ts);
    $end_ts     = strtotime($offset_days . ' days', $today_ts);
    $status     = isset($status_override) ? $status_override : DROPCRM_MEMBERSHIP_STATUS_ACTIVE;

    $m = entity_create('crm_membership', array(
      'cid'    => $cid,
      'type'   => $type,
      'status' => $status,
    ));
    $m->save();
    // Auto-assign member_id now that mid is known.
    if (empty($m->member_id) && function_exists('_drop_crm_membership_generate_member_id')) {
      $m->member_id = _drop_crm_membership_generate_member_id($m->mid);
      $m->save();
    }
    drop_start_track('crm_membership', $m->mid);

    $term = entity_create('crm_membership_term', array(
      'mid'        => $m->mid,
      'start_date' => $start_ts,
      'end_date'   => $end_ts,
      'status'     => DROPCRM_MEMBERSHIP_STATUS_ACTIVE,
      'notes'      => $note,
    ));
    $term->save();
    drop_start_track('crm_membership_term', $term->tid);

    // Payment on the active/expiring-soon scenarios to demo total_paid.
    if (in_array($offset_days, array(5, 15, 25, 180), TRUE)) {
      $dues = isset($types[$type]['default_dues']) ? (float) $types[$type]['default_dues'] : 50.00;
      $p = entity_create('crm_membership_payment', array(
        'mid'       => $m->mid,
        'tid'       => $term->tid,
        'amount'    => $dues,
        'paid_date' => $start_ts,
        'method'    => 'Check',
        'reference' => 'chk sample',
      ));
      $p->save();
      drop_start_track('crm_membership_payment', $p->pid);
    }
  }

  // One lifetime membership with 2-term renewal history + 2 payments, on
  // the first org. Demos the term-history + payment-history panels on
  // the membership view.
  if (isset($contact_ids[0]) && isset($types['lifetime'])) {
    $cid = $contact_ids[0];
    $m = entity_create('crm_membership', array(
      'cid'    => $cid,
      'type'   => $lifetime_type,
      'status' => DROPCRM_MEMBERSHIP_STATUS_ACTIVE,
    ));
    $m->save();
    if (empty($m->member_id) && function_exists('_drop_crm_membership_generate_member_id')) {
      $m->member_id = _drop_crm_membership_generate_member_id($m->mid);
      $m->save();
    }
    drop_start_track('crm_membership', $m->mid);

    $t1_start = strtotime('-700 days', $today_ts);
    $t1_end   = strtotime('-335 days', $today_ts);
    $t2_start = strtotime('-334 days', $today_ts);
    $t2_end   = strtotime('+31 days',  $today_ts);

    foreach (array(
      array($t1_start, $t1_end, 'initial term'),
      array($t2_start, $t2_end, 'renewal'),
    ) as $term_info) {
      $term = entity_create('crm_membership_term', array(
        'mid'        => $m->mid,
        'start_date' => $term_info[0],
        'end_date'   => $term_info[1],
        'status'     => DROPCRM_MEMBERSHIP_STATUS_ACTIVE,
        'notes'      => $term_info[2],
      ));
      $term->save();
      drop_start_track('crm_membership_term', $term->tid);

      $dues = isset($types[$lifetime_type]['default_dues']) ? (float) $types[$lifetime_type]['default_dues'] : 100.00;
      $p = entity_create('crm_membership_payment', array(
        'mid'       => $m->mid,
        'tid'       => $term->tid,
        'amount'    => $dues,
        'paid_date' => $term_info[0],
        'method'    => ($term_info[2] === 'initial term') ? 'Credit Card' : 'Check',
        'reference' => 'sample',
      ));
      $p->save();
      drop_start_track('crm_membership_payment', $p->pid);
    }
  }
}

/**
 * Generate sample tickets across projects.
 */
function _drop_start_generate_tickets($uid, $project_ids, $sample_uids = array()) {
  $pm_uid  = isset($sample_uids['pm']) ? $sample_uids['pm'] : NULL;
  $sup_uid = isset($sample_uids['supervisor']) ? $sample_uids['supervisor'] : NULL;

  $tickets = array(
    // Venue Construction
    array('title' => 'Finalize stadium seating plan', 'type' => 'task', 'status' => 'todo', 'priority' => 2, 'project' => 'VEN', 'assignee' => $pm_uid,
      'body' => 'Review the seating layout for the main stadium. Capacity must meet IOC minimum of 60,000. Include VIP and accessibility sections.'),
    array('title' => 'Inspect swimming pool filtration system', 'type' => 'task', 'status' => 'in_progress', 'priority' => 3, 'project' => 'VEN', 'assignee' => $sup_uid,
      'body' => 'The aquatics center filtration system must meet FINA standards. Schedule inspection with the engineering team.'),
    array('title' => 'Roof leak in velodrome reported', 'type' => 'bug', 'status' => 'open', 'priority' => 3, 'project' => 'VEN', 'assignee' => $sup_uid,
      'body' => 'Maintenance crew reported water intrusion near section C of the velodrome roof during last week\'s rain test.'),
    array('title' => 'Install solar panels on athlete village', 'type' => 'feature', 'status' => 'icebox', 'priority' => 1, 'project' => 'VEN',
      'body' => 'Sustainability initiative — explore solar panel installation on athlete village rooftops for partial energy self-sufficiency.'),

    // Athlete Services
    array('title' => 'Dietary requirements database', 'type' => 'feature', 'status' => 'todo', 'priority' => 2, 'project' => 'ATH', 'assignee' => $pm_uid,
      'body' => 'Build a system to track athlete dietary restrictions and preferences. Must handle halal, kosher, vegan, gluten-free, and allergy-specific requirements.'),
    array('title' => 'Medical staff shift schedule', 'type' => 'task', 'status' => 'in_progress', 'priority' => 2, 'project' => 'ATH', 'assignee' => $sup_uid,
      'body' => 'Create the rotation schedule for medical staff across all venues. Need 24/7 coverage at the main village and peak-hours coverage at competition sites.'),
    array('title' => 'Translation service for opening ceremony', 'type' => 'task', 'status' => 'open', 'priority' => 1, 'project' => 'ATH',
      'body' => 'Arrange simultaneous translation in 12 languages for the opening ceremony proceedings and athlete briefings.'),
    array('title' => 'Anti-doping testing protocol update', 'type' => 'improvement', 'status' => 'blocked', 'priority' => 4, 'project' => 'ATH', 'assignee' => $pm_uid,
      'body' => 'WADA has issued new guidelines effective this quarter. Our testing protocols need to be updated before competition begins. Blocked waiting for official documentation.'),

    // Broadcasting
    array('title' => 'Camera placement plan for athletics', 'type' => 'task', 'status' => 'todo', 'priority' => 2, 'project' => 'BC', 'assignee' => $sup_uid,
      'body' => 'Map out camera positions for the athletics stadium. Include high-speed cameras at the finish line, wide-angle for field events, and overhead drone shots.'),
    array('title' => 'Streaming infrastructure load test', 'type' => 'task', 'status' => 'in_progress', 'priority' => 3, 'project' => 'BC', 'assignee' => $pm_uid,
      'body' => 'Simulate 50 million concurrent viewers across all streaming endpoints. Must maintain 4K quality with less than 3 seconds latency.'),
    array('title' => 'Commentary booth wiring incomplete', 'type' => 'bug', 'status' => 'open', 'priority' => 2, 'project' => 'BC',
      'body' => 'Three commentary booths in the aquatics center are missing audio return feeds. Electricians need to complete wiring before tech rehearsal.'),

    // Security
    array('title' => 'Credential badge printing system', 'type' => 'task', 'status' => 'todo', 'priority' => 2, 'project' => 'SEC', 'assignee' => $sup_uid,
      'body' => 'Set up the on-site credential printing system. Must support photo ID badges with QR codes, RFID chips, and zone-access encoding.'),
    array('title' => 'Perimeter drone surveillance proposal', 'type' => 'feature', 'status' => 'icebox', 'priority' => 1, 'project' => 'SEC',
      'body' => 'Evaluate drone-based perimeter surveillance for the athlete village and main stadium complex. Requires airspace coordination with aviation authority.'),
    array('title' => 'Emergency evacuation drill', 'type' => 'task', 'status' => 'open', 'priority' => 4, 'project' => 'SEC', 'assignee' => $pm_uid,
      'body' => 'Schedule and execute a full-scale evacuation drill at the main stadium. All security, medical, and volunteer teams must participate.'),
  );

  foreach ($tickets as $data) {
    $pid = isset($project_ids[$data['project']]) ? $project_ids[$data['project']] : 0;
    $values = array(
      'title'    => $data['title'],
      'body'     => $data['body'],
      'type'     => $data['type'],
      'status'   => $data['status'],
      'priority' => $data['priority'],
      'pid'      => $pid,
      'uid'      => $uid,
    );
    if (!empty($data['assignee'])) {
      $values['assignee_uid']  = $data['assignee'];
      $values['assignee_mode'] = 'user';
    }
    $ticket = entity_create('drop_ticket', $values);
    $ticket->save();
    drop_start_track('drop_ticket', $ticket->tid);
  }
}

/**
 * Generate sample tasks.
 */
function _drop_start_generate_tasks($uid) {
  $tasks = array(
    array('title' => 'Review venue inspection reports', 'priority' => 2, 'status' => 'pending',
      'notes' => 'Go through the latest batch of venue inspection reports and flag any issues that need immediate attention.'),
    array('title' => 'Update athlete welcome packets', 'priority' => 1, 'status' => 'pending',
      'notes' => 'Refresh the welcome packet content with updated schedules, maps, and Wi-Fi information.'),
    array('title' => 'Confirm catering contracts', 'priority' => 2, 'status' => 'pending',
      'notes' => 'Follow up with all catering vendors to confirm final headcounts and delivery schedules for the athlete village.'),
    array('title' => 'Order volunteer uniforms', 'priority' => 1, 'status' => 'complete',
      'notes' => 'Placed order for 5,000 volunteer uniforms. Expected delivery in 6 weeks.'),
    array('title' => 'Book interpreters for press conferences', 'priority' => 2, 'status' => 'pending',
      'notes' => 'Need interpreters for English, French, Spanish, Mandarin, Arabic, and Japanese for daily press briefings.'),
    array('title' => 'Test emergency PA system', 'priority' => 3, 'status' => 'pending',
      'notes' => 'Full test of the public address and emergency announcement systems across all competition venues.'),
  );

  foreach ($tasks as $data) {
    $task = entity_create('drop_task', array(
      'title'    => $data['title'],
      'priority' => $data['priority'],
      'status'   => $data['status'],
      'notes'    => $data['notes'],
      'uid'      => $uid,
    ));
    $task->save();
    drop_start_track('drop_task', $task->tid);
  }
}

/**
 * Generate sample goals.
 */
function _drop_start_generate_goals($uid) {
  $goals = array(
    array('label' => 'Review daily progress reports', 'frequency' => 'daily',
      'description' => 'Check progress dashboards for all venue construction projects every morning.'),
    array('label' => 'Team standup meeting', 'frequency' => 'specific_days', 'days' => '1,3,5',
      'description' => 'Brief standup with department leads on Monday, Wednesday, and Friday.'),
    array('label' => 'Venue walkthrough', 'frequency' => 'weekly',
      'description' => 'Physical walkthrough of one competition venue per week to verify readiness.'),
    array('label' => 'Budget reconciliation', 'frequency' => 'monthly',
      'description' => 'Monthly review of spending against budget for all departments.'),
  );

  foreach ($goals as $data) {
    $goal = entity_create('drop_goal', array(
      'label'       => $data['label'],
      'description' => $data['description'],
      'frequency'   => $data['frequency'],
      'days_of_week' => isset($data['days']) ? $data['days'] : '',
      'active'      => 1,
      'uid'         => $uid,
    ));
    $goal->save();
    drop_start_track('drop_goal', $goal->gid);
  }
}

/**
 * Generate sample time entries.
 */
function _drop_start_generate_time_entries($uid, $project_ids) {
  $today = date('Y-m-d');
  $yesterday = date('Y-m-d', strtotime('-1 day'));
  $two_days_ago = date('Y-m-d', strtotime('-2 days'));

  $entries = array(
    array('project' => 'VEN', 'date' => $two_days_ago, 'start' => '09:00', 'end' => '11:30', 'notes' => 'Stadium seating plan review meeting', 'billable' => 1),
    array('project' => 'VEN', 'date' => $two_days_ago, 'start' => '13:00', 'end' => '15:00', 'notes' => 'Velodrome roof inspection', 'billable' => 1),
    array('project' => 'ATH', 'date' => $yesterday, 'start' => '08:30', 'end' => '10:00', 'notes' => 'Medical staff scheduling', 'billable' => 1),
    array('project' => 'BC',  'date' => $yesterday, 'start' => '10:30', 'end' => '12:00', 'notes' => 'Camera placement walkthrough', 'billable' => 1),
    array('project' => 'SEC', 'date' => $today, 'start' => '09:00', 'end' => '10:30', 'notes' => 'Credential system setup', 'billable' => 1),
    array('project' => 'ATH', 'date' => $today, 'start' => '11:00', 'end' => '12:00', 'notes' => 'Dietary requirements database planning', 'billable' => 0),
  );

  foreach ($entries as $data) {
    $pid = isset($project_ids[$data['project']]) ? $project_ids[$data['project']] : 0;
    if (!$pid) {
      continue;
    }

    // Calculate duration in minutes.
    $start_parts = explode(':', $data['start']);
    $end_parts   = explode(':', $data['end']);
    $duration    = ((int) $end_parts[0] * 60 + (int) $end_parts[1])
                 - ((int) $start_parts[0] * 60 + (int) $start_parts[1]);

    $entry = entity_create('drop_time_entry', array(
      'pid'        => $pid,
      'uid'        => $uid,
      'entry_date' => $data['date'],
      'start_time' => $data['start'],
      'end_time'   => $data['end'],
      'duration'   => $duration,
      'notes'      => $data['notes'],
      'billable'   => $data['billable'],
    ));
    $entry->save();
    drop_start_track('drop_time_entry', $entry->eid);
  }
}

/**
 * Generate sample doc pages (nodes of type 'doc').
 */
function _drop_start_generate_docs($uid) {
  // Check that the doc content type exists.
  $types = node_type_get_types();
  if (!isset($types['doc'])) {
    return;
  }

  $docs = array(
    array(
      'title' => 'Olympics Project Overview',
      'body'  => '<h2>Welcome to the Olympics Planning Hub</h2>'
        . '<p>This documentation space contains key information for the organizing committee. '
        . 'Use the sidebar navigation to browse topics by department.</p>'
        . '<h3>Key dates</h3>'
        . '<ul>'
        . '<li>Venue completion deadline: 3 months before opening</li>'
        . '<li>Tech rehearsal: 6 weeks before opening</li>'
        . '<li>Full dress rehearsal: 2 weeks before opening</li>'
        . '</ul>',
      'children' => array(
        array(
          'title' => 'Venue Standards',
          'body'  => '<p>All competition venues must meet international federation standards. '
            . 'Key requirements include:</p>'
            . '<ul>'
            . '<li>Minimum seating capacity per sport</li>'
            . '<li>Lighting levels for broadcast (minimum 2000 lux)</li>'
            . '<li>Accessibility compliance</li>'
            . '<li>Emergency evacuation routes for full capacity</li>'
            . '</ul>',
        ),
        array(
          'title' => 'Volunteer Handbook',
          'body'  => '<p>Guidelines for the 45,000 volunteers supporting the games.</p>'
            . '<h3>Shift structure</h3>'
            . '<p>Volunteers work 6-hour shifts with mandatory 30-minute breaks. '
            . 'No volunteer may work more than two consecutive shifts without a minimum 8-hour rest period.</p>'
            . '<h3>Code of conduct</h3>'
            . '<p>All volunteers represent the organizing committee. Professional behavior, '
            . 'punctuality, and a positive attitude are expected at all times.</p>',
        ),
      ),
    ),
  );

  foreach ($docs as $doc_data) {
    $parent = _drop_start_create_doc($doc_data['title'], $doc_data['body'], $uid);
    if (!empty($doc_data['children'])) {
      foreach ($doc_data['children'] as $child_data) {
        _drop_start_create_doc($child_data['title'], $child_data['body'], $uid, $parent->nid);
      }
    }
  }
}

/**
 * Helper: create a single doc node.
 *
 * @return object
 *   The saved node object.
 */
function _drop_start_create_doc($title, $body, $uid, $parent_nid = NULL) {
  $node         = new Node();
  $node->type   = 'doc';
  $node->title  = $title;
  $node->status = 1;
  $node->uid    = $uid;
  $node->body[LANGUAGE_NONE][0] = array(
    'value'  => $body,
    'format' => 'filtered_html',
  );
  // Attach children to the parent's book outline. Top-level docs (no parent)
  // fall through to drop_docs_node_presave, which auto-roots them as new books.
  if ($parent_nid) {
    $parent = node_load($parent_nid);
    if ($parent && !empty($parent->book['bid']) && !empty($parent->book['mlid'])) {
      $node->book = array(
        'bid'  => $parent->book['bid'],
        'plid' => $parent->book['mlid'],
      );
    }
  }
  node_save($node);
  drop_start_track('node', $node->nid);
  return $node;
}
