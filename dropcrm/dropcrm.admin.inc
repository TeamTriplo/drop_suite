<?php
/**
 * @file
 * Admin page callbacks, entity forms, and settings for DropCRM.
 *
 * Form functions follow the entity_ui convention: the callback name is
 * {entity_type}_form and the function receives the entity as the third
 * argument (after $form and $form_state).
 */

// ---------------------------------------------------------------------------
// Per-bundle contact list pages
// ---------------------------------------------------------------------------

/**
 * Page callback: list CRM contacts filtered to a single bundle.
 *
 * Provides the People, Organizations, and Households tab pages on drop/crm.
 *
 * @param string $bundle
 *   One of: crm_person, crm_organization, crm_household.
 *
 * @return array
 *   A render array.
 */
function dropcrm_contact_list_page($bundle) {
  $bundle_info = array(
    'crm_person'       => array('label' => t('Person'),       'plural' => t('People'),        'add_path' => 'drop/crm/contacts/add/crm_person'),
    'crm_organization' => array('label' => t('Organization'), 'plural' => t('Organizations'), 'add_path' => 'drop/crm/contacts/add/crm_organization'),
    'crm_household'    => array('label' => t('Household'),    'plural' => t('Households'),    'add_path' => 'drop/crm/contacts/add/crm_household'),
  );

  $info = isset($bundle_info[$bundle]) ? $bundle_info[$bundle] : array(
    'label'    => $bundle,
    'plural'   => $bundle,
    'add_path' => 'drop/crm/contacts/add/' . $bundle,
  );

  backdrop_set_title($info['plural'], PASS_THROUGH);

  // Search filter — read from GET so it survives pager navigation.
  $search = isset($_GET['search']) ? trim($_GET['search']) : '';
  $tag_tid = isset($_GET['tag']) ? (int) $_GET['tag'] : 0;

  // Build sortable header. Name is the default ascending sort for all bundles.
  // Organization is sortable on the People list via a MIN(org name) expression
  // added to the query (so the DB can ORDER BY it with paging intact).
  $header = array(
    array('data' => t('Name'), 'field' => 'c.name', 'sort' => 'asc'),
  );
  if ($bundle === 'crm_person') {
    // Sortable: uses the first_org_name expression added to the query below.
    $header[] = array('data' => t('Organization'), 'field' => 'first_org_name');
  }
  if ($bundle === 'crm_organization') {
    $header[] = t('People');  // Not sortable — count comes from a separate query.
  }
  $header[] = array('data' => t('Rating'), 'field' => 'rating');
  $header[] = array('data' => t('Status'), 'field' => 'c.status');
  $header[] = array('data' => t('Operations'), 'colspan' => 2);

  // Build the base query, then add an org-sort expression for persons.
  $query = db_select('crm_contact', 'c');
  $query->fields('c', array('cid', 'name', 'status'));
  $query->condition('c.type', $bundle);

  if ($search !== '') {
    $query->condition('c.name', '%' . db_like($search) . '%', 'LIKE');
  }
  if ($tag_tid && $bundle === 'crm_person') {
    $query->join('field_data_field_crm_tags', 'ft', 'ft.entity_id = c.cid AND ft.field_crm_tags_tid = :tid', array(':tid' => $tag_tid));
  }

  // LEFT JOIN rating field for all bundles (sortable expression).
  $query->leftJoin('field_data_field_crm_rating', 'rat', 'rat.entity_id = c.cid');
  $query->addExpression('MAX(rat.field_crm_rating_value)', 'rating');

  if ($bundle === 'crm_person') {
    // LEFT JOIN so people with no org still appear. GROUP BY prevents
    // duplicate rows when a person belongs to more than one org.
    // MIN(o.name) gives us a stable sort key (alphabetically first org).
    $query->leftJoin('field_data_field_crm_employer', 'e', 'e.entity_id = c.cid');
    $query->leftJoin('crm_contact', 'o', 'o.cid = e.field_crm_employer_target_id');
    $query->addExpression('MIN(o.name)', 'first_org_name');
    $query->groupBy('c.cid');
    $query->groupBy('c.name');
    $query->groupBy('c.status');
  }
  else {
    // For non-person bundles we still need GROUP BY for the MAX() expression.
    $query->groupBy('c.cid');
    $query->groupBy('c.name');
    $query->groupBy('c.status');
  }

  $contacts = $query
    ->extend('TableSort')
    ->orderByHeader($header)
    ->extend('PagerDefault')
    ->limit(50)
    ->execute()
    ->fetchAll();

  // For organizations, count affiliated people in one query.
  $people_count_by_cid = array();
  if ($bundle === 'crm_organization' && $contacts) {
    $org_cids = array_column((array) $contacts, 'cid');
    $count_result = db_select('field_data_field_crm_employer', 'e')
      ->fields('e', array('field_crm_employer_target_id'))
      ->condition('field_crm_employer_target_id', $org_cids, 'IN')
      ->execute();
    foreach ($count_result as $row) {
      $cid = $row->field_crm_employer_target_id;
      $people_count_by_cid[$cid] = isset($people_count_by_cid[$cid]) ? $people_count_by_cid[$cid] + 1 : 1;
    }
  }

  // For persons, load affiliated organization names in one query.
  $org_by_cid = array();
  if ($bundle === 'crm_person' && $contacts) {
    $cids = array_column((array) $contacts, 'cid');
    $emp_result = db_select('field_data_field_crm_employer', 'e')
      ->fields('e', array('entity_id', 'field_crm_employer_target_id'))
      ->condition('entity_id', $cids, 'IN')
      ->execute();
    $org_cids = array();
    $emp_map = array();
    foreach ($emp_result as $row) {
      $emp_map[$row->entity_id][] = $row->field_crm_employer_target_id;
      $org_cids[] = $row->field_crm_employer_target_id;
    }
    if ($org_cids) {
      $orgs = db_select('crm_contact', 'o')
        ->fields('o', array('cid', 'name'))
        ->condition('cid', $org_cids, 'IN')
        ->execute()
        ->fetchAllAssoc('cid');
      foreach ($emp_map as $person_cid => $ocids) {
        $links = array();
        foreach ($ocids as $org_cid) {
          if (isset($orgs[$org_cid])) {
            $links[] = l($orgs[$org_cid]->name, 'drop/crm/contact/' . $org_cid);
          }
        }
        if ($links) {
          $org_by_cid[$person_cid] = implode(', ', $links);
        }
      }
    }
  }

  $show_org_col     = ($bundle === 'crm_person');
  $show_people_col  = ($bundle === 'crm_organization');

  $rows = array();
  foreach ($contacts as $contact) {
    $row = array(
      l($contact->name, 'drop/crm/contact/' . $contact->cid),
    );
    if ($show_org_col) {
      $row[] = isset($org_by_cid[$contact->cid])
        ? $org_by_cid[$contact->cid]
        : array('data' => t('—'), 'class' => array('crm-no-org'));
    }
    if ($show_people_col) {
      $count = isset($people_count_by_cid[$contact->cid]) ? $people_count_by_cid[$contact->cid] : 0;
      $row[] = $count ?: array('data' => '0', 'class' => array('crm-no-org'));
    }
    $stars = !empty($contact->rating) ? _dropcrm_render_stars((int) $contact->rating) : '';
    $row[] = array('data' => $stars, 'class' => array('crm-rating-cell'));
    $row[] = $contact->status ? t('Active') : t('Inactive');
    $row[] = l(t('Edit'), 'drop/crm/contacts/' . $contact->cid . '/edit');
    $row[] = l(t('Delete'), 'drop/crm/contacts/' . $contact->cid . '/delete');
    $rows[] = $row;
  }

  $output['add_link'] = array(
    '#markup' => l(t('+ Add @type', array('@type' => $info['label'])), $info['add_path']),
    '#prefix' => '<p class="crm-add-link">',
    '#suffix' => '</p>',
  );

  $clear_link = ($search !== '' || $tag_tid)
    ? ' <a href="' . check_plain(url(current_path())) . '">' . t('Clear') . '</a>'
    : '';
  $output['search'] = array(
    '#markup' => '<form class="crm-search-form" method="get" action="">'
      . '<input type="text" name="search" value="' . check_plain($search) . '"'
      . ' placeholder="' . t('Search by name…') . '">'
      . '<button type="submit">' . t('Search') . '</button>'
      . $clear_link
      . '</form>',
    '#prefix' => '<div class="crm-filter-row">',
    '#suffix' => '</div>',
    '#weight' => -1,
  );

  if ($tag_tid && $bundle === 'crm_person') {
    $tag_term = taxonomy_term_load($tag_tid);
    if ($tag_term) {
      $output['tag_filter'] = array(
        '#markup' => '<p class="crm-filter-notice">'
          . t('Filtered by tag: <strong>@tag</strong>', array('@tag' => $tag_term->name))
          . ' &nbsp; <a href="' . check_plain(url(current_path())) . '">' . t('Remove filter') . '</a>'
          . '</p>',
        '#weight' => -1,
      );
    }
  }

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => $header,
    '#rows'   => $rows,
    '#empty'  => t('No @type found.', array('@type' => strtolower($info['plural']))),
  );

  $output['pager'] = array('#theme' => 'pager', '#weight' => 10);

  return $output;
}

// ---------------------------------------------------------------------------
// Reports
// ---------------------------------------------------------------------------

/**
 * Page callback: Reports landing page.
 */
function dropcrm_reports_page() {
  // The default local task (last-contact report) renders automatically.
  // This callback is only hit if someone navigates directly to drop/crm/reports
  // without a sub-path, which the MENU_DEFAULT_LOCAL_TASK handles.
  return dropcrm_report_last_contact();
}

/**
 * Page callback: People sorted by date of last completed activity (oldest first).
 *
 * People with no activities at all appear at the very top so they are the
 * most urgent to reach out to.
 *
 * @return array
 *   A render array.
 */
function dropcrm_report_last_contact() {
  // Fetch all persons with their most recent completed activity date.
  // LEFT JOIN so people with no activities still appear.
  $query = db_select('crm_contact', 'c');
  $query->leftJoin('field_data_field_crm_contacts', 'fc', 'fc.field_crm_contacts_target_id = c.cid AND fc.entity_type = :etype', array(':etype' => 'crm_activity'));
  $query->leftJoin('crm_activity', 'a', 'a.aid = fc.entity_id AND a.status = :status', array(':status' => 'completed'));
  $query->fields('c', array('cid', 'name'));
  $query->addExpression('MAX(a.activity_date)', 'last_contact');
  $query->condition('c.type', 'crm_person');
  $query->condition('c.status', 1);
  $query->groupBy('c.cid');
  $query->groupBy('c.name');
  $query->orderBy('last_contact', 'ASC');

  $people = $query->execute()->fetchAll();

  // Also fetch org name for each person in one query.
  $cids = array_column((array) $people, 'cid');
  $org_names = array();
  if ($cids) {
    $emp = db_select('field_data_field_crm_employer', 'e')
      ->fields('e', array('entity_id', 'field_crm_employer_target_id'))
      ->condition('entity_id', $cids, 'IN')
      ->execute();
    $org_cids = array();
    $emp_map = array();
    foreach ($emp as $row) {
      $emp_map[$row->entity_id][] = $row->field_crm_employer_target_id;
      $org_cids[] = $row->field_crm_employer_target_id;
    }
    if ($org_cids) {
      $orgs = db_select('crm_contact', 'o')
        ->fields('o', array('cid', 'name'))
        ->condition('cid', $org_cids, 'IN')
        ->execute()->fetchAllAssoc('cid');
      foreach ($emp_map as $pcid => $ocids) {
        $links = array();
        foreach ($ocids as $ocid) {
          if (isset($orgs[$ocid])) {
            $links[] = l($orgs[$ocid]->name, 'drop/crm/contact/' . $ocid);
          }
        }
        if ($links) {
          $org_names[$pcid] = implode(', ', $links);
        }
      }
    }
  }

  $rows = array();
  foreach ($people as $person) {
    if ($person->last_contact) {
      $date_cell = format_date($person->last_contact, 'custom', 'M j, Y');
      // Highlight if more than 90 days ago.
      $overdue = ($person->last_contact < (REQUEST_TIME - 90 * 86400));
      $date_col = $overdue
        ? array('data' => $date_cell, 'class' => array('crm-overdue'))
        : $date_cell;
    }
    else {
      $date_col = array('data' => t('Never'), 'class' => array('crm-overdue'));
    }

    $rows[] = array(
      l($person->name, 'drop/crm/contact/' . $person->cid),
      isset($org_names[$person->cid]) ? $org_names[$person->cid] : '',
      $date_col,
    );
  }

  $output['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  $output['table'] = array(
    '#theme'   => 'table',
    '#caption' => t('People by Last Contact — those you haven\'t contacted recently appear first.'),
    '#header'  => array(t('Name'), t('Organization'), t('Last Contact')),
    '#rows'    => $rows,
    '#empty'   => t('No people found.'),
  );

  return $output;
}

// ---------------------------------------------------------------------------
// Activity list page
// ---------------------------------------------------------------------------

/**
 * Page callback: full list of all CRM activities.
 *
 * Replaces the entity_ui default overview so we can include the linked
 * contacts column.
 *
 * @return array
 *   A render array.
 */
function dropcrm_activity_list_page() {
  $type_options = _dropcrm_activity_type_options();

  $type_filter    = isset($_GET['type']) && isset($type_options[$_GET['type']]) ? $_GET['type'] : '';
  $contact_filter = isset($_GET['contact']) ? trim($_GET['contact']) : '';

  // Build filter form.
  $select_options = array('' => t('— All types —')) + $type_options;
  $select_html = '<select name="type" onchange="this.form.submit()">';
  foreach ($select_options as $val => $label) {
    $selected = ($val === $type_filter) ? ' selected="selected"' : '';
    $select_html .= '<option value="' . check_plain($val) . '"' . $selected . '>' . check_plain($label) . '</option>';
  }
  $select_html .= '</select>';

  $contact_input = '<input type="text" name="contact" value="' . check_plain($contact_filter) . '" placeholder="' . t('Filter by contact…') . '" />';

  $filter_form = '<form method="get" class="crm-search-form">'
    . $contact_input . ' '
    . $select_html
    . ' <button type="submit">' . t('Filter') . '</button>';
  if ($type_filter || $contact_filter) {
    $filter_form .= ' ' . l(t('Clear'), 'drop/crm/activities');
  }
  $filter_form .= '</form>';

  // Sortable headers — default sort is created DESC (most recently entered first).
  $header = array(
    array('data' => t('Subject'), 'field' => 'a.subject'),
    t('Type'),
    t('Contacts'),
    array('data' => t('Activity Date'), 'field' => 'a.activity_date'),
    array('data' => t('Created'), 'field' => 'a.created', 'sort' => 'desc'),
    t('Operations'),
  );

  $query = db_select('crm_activity', 'a')
    ->fields('a', array('aid', 'subject', 'type', 'status', 'activity_date', 'created'));

  if ($type_filter !== '') {
    $query->condition('a.type', $type_filter);
  }

  if ($contact_filter !== '') {
    $sub = db_select('field_data_field_crm_contacts', 'f');
    $sub->innerJoin('crm_contact', 'c', 'c.cid = f.field_crm_contacts_target_id');
    $sub->fields('f', array('entity_id'))
      ->condition('f.entity_type', 'crm_activity')
      ->condition('c.name', '%' . db_like($contact_filter) . '%', 'LIKE');
    $matching_aids = $sub->execute()->fetchCol();
    if ($matching_aids) {
      $query->condition('a.aid', $matching_aids, 'IN');
    }
    else {
      // No contacts match — force empty result.
      $query->condition('a.aid', array(0), 'IN');
    }
  }

  $activities = $query
    ->extend('TableSort')
    ->orderByHeader($header)
    ->execute()
    ->fetchAll();

  // Pre-load all contact links keyed by activity id.
  $aids = array_column((array) $activities, 'aid');
  $contact_links = _dropcrm_activity_contact_links($aids);

  // Single query: which activities have meeting notes?
  $aids_with_notes = array();
  if ($aids && db_table_exists('field_data_field_crm_meeting_notes')) {
    $notes_result = db_select('field_data_field_crm_meeting_notes', 'n')
      ->fields('n', array('entity_id'))
      ->condition('entity_id', $aids, 'IN')
      ->condition('field_crm_meeting_notes_value', '', '<>')
      ->execute();
    foreach ($notes_result as $nr) {
      $aids_with_notes[$nr->entity_id] = TRUE;
    }
  }

  $rows = array();
  foreach ($activities as $activity) {
    $subject_cell = l($activity->subject, 'drop/crm/activity/' . $activity->aid);
    if (!empty($aids_with_notes[$activity->aid])) {
      $subject_cell .= ' <span class="crm-notes-badge" title="' . t('Has meeting notes') . '">' . icon('note', array('attributes' => array('width' => 14, 'height' => 14))) . '</span>';
    }
    $rows[] = array(
      $subject_cell,
      isset($type_options[$activity->type]) ? $type_options[$activity->type] : $activity->type,
      isset($contact_links[$activity->aid]) ? implode(', ', $contact_links[$activity->aid]) : '',
      format_date($activity->activity_date, 'custom', 'M j, Y'),
      format_date($activity->created, 'custom', 'M j, Y'),
      l(t('Edit'), 'drop/crm/activities/manage/' . $activity->aid . '/edit')
        . ' · ' . l(t('Delete'), 'drop/crm/activities/manage/' . $activity->aid . '/delete'),
    );
  }

  $output['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  $output['filter'] = array(
    '#markup' => $filter_form,
    '#prefix' => '<div class="crm-filter-row">',
    '#suffix' => '</div>',
  );

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => $header,
    '#rows'   => $rows,
    '#empty'  => t('No activities recorded yet.'),
  );

  return $output;
}

/**
 * Loads contact name links for a set of activity IDs.
 *
 * Returns an array keyed by aid, each value an array of rendered link strings.
 *
 * @param array $aids
 *   Array of activity IDs.
 *
 * @return array
 */
function _dropcrm_activity_contact_links(array $aids) {
  if (empty($aids)) {
    return array();
  }

  $result = db_select('field_data_field_crm_contacts', 'f')
    ->fields('f', array('entity_id', 'field_crm_contacts_target_id'))
    ->condition('entity_id', $aids, 'IN')
    ->condition('entity_type', 'crm_activity')
    ->execute();

  $cids_by_aid = array();
  foreach ($result as $row) {
    $cids_by_aid[$row->entity_id][] = $row->field_crm_contacts_target_id;
  }

  $all_cids = array();
  foreach ($cids_by_aid as $cids) {
    $all_cids = array_merge($all_cids, $cids);
  }
  $contacts = $all_cids ? crm_contact_load_multiple(array_unique($all_cids)) : array();

  $links = array();
  foreach ($cids_by_aid as $aid => $cids) {
    $links[$aid] = array();
    foreach ($cids as $cid) {
      if (isset($contacts[$cid])) {
        $links[$aid][] = l(
          entity_label('crm_contact', $contacts[$cid]),
          'drop/crm/contact/' . $cid
        );
      }
    }
  }

  return $links;
}

// ---------------------------------------------------------------------------
// To Do list
// ---------------------------------------------------------------------------

/**
 * Page callback: planned activities ("Things to do") list.
 *
 * Shows all activities with status = 'planned', sorted by priority (high
 * first) then date (soonest first). Overdue items get a visual highlight.
 *
 * @return array
 *   A render array.
 */
function dropcrm_todo_page() {
  $type_options     = _dropcrm_activity_type_options();
  $priority_options = _dropcrm_activity_priority_options();

  $header = array(
    t('Contacts'),
    t('Subject'),
    t('Type'),
    array('data' => t('Priority'), 'field' => 'a.priority'),
    array('data' => t('Date Due'), 'field' => 'a.due_date', 'sort' => 'asc'),
    t('Operations'),
  );

  $query = db_select('crm_activity', 'a')
    ->fields('a', array('aid', 'subject', 'type', 'priority', 'activity_date', 'due_date'))
    ->condition('status', 'planned')
    ->extend('TableSort')
    ->orderByHeader($header);

  $activities = $query->execute()->fetchAll();

  $all_contact_links = _dropcrm_activity_contact_links(array_column((array) $activities, 'aid'));

  $rows = array();
  foreach ($activities as $activity) {
    $contact_links = isset($all_contact_links[$activity->aid]) ? $all_contact_links[$activity->aid] : array();

    $due_ts   = !empty($activity->due_date) ? $activity->due_date : NULL;
    $today    = strtotime('today midnight');
    $two_days = strtotime('+2 days midnight');
    if ($due_ts) {
      if ($due_ts < $today) {
        $due_class = 'crm-due-overdue';
      }
      elseif ($due_ts < $two_days) {
        $due_class = 'crm-due-soon';
      }
      else {
        $due_class = 'crm-due-future';
      }
      $date_cell = array(
        'data'  => format_date($due_ts, 'custom', 'M j, Y'),
        'class' => array($due_class),
      );
    }
    else {
      $due_class = '';
      $date_cell = array('data' => t('—'), 'class' => array('crm-no-org'));
    }

    $rows[] = array(
      'data' => array(
        implode(', ', $contact_links),
        l($activity->subject, 'drop/crm/activity/' . $activity->aid),
        isset($type_options[$activity->type]) ? $type_options[$activity->type] : $activity->type,
        array(
          'data'  => isset($priority_options[$activity->priority]) ? $priority_options[$activity->priority] : '',
          'class' => array('crm-priority-' . (($activity->priority == 2) ? 'high' : (($activity->priority == 0) ? 'low' : 'normal'))),
        ),
        $date_cell,
        l(t('Edit'), 'drop/crm/activities/manage/' . $activity->aid . '/edit')
        . ' · '
        . l(t('Mark done'), 'drop/crm/activities/' . $activity->aid . '/done'),
      ),
      'class' => ($due_class === 'crm-due-overdue') ? array('crm-row-overdue') : array(),
    );
  }

  $output['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  $output['add_link'] = array(
    '#markup' => l(
      t('+ Add task'),
      'drop/crm/activities/add',
      array('query' => array('status' => 'planned'))
    ),
    '#prefix' => '<p class="crm-add-link">',
    '#suffix' => '</p>',
  );

  $output['table'] = array(
    '#theme'      => 'table',
    '#header'     => $header,
    '#rows'       => $rows,
    '#empty'      => t("Nothing to do — you're all caught up!"),
    '#attributes' => array('class' => array('crm-todo-table')),
  );

  return $output;
}

/**
 * Page callback: mark a planned activity as completed.
 *
 * Called from the "Mark done" link on the To Do list. Saves the activity
 * with status = 'completed' and redirects back to the list.
 *
 * @param CrmActivity $activity
 *   The activity entity to mark done.
 */
function dropcrm_activity_mark_done(CrmActivity $activity) {
  $activity->status = 'completed';
  $activity->save();
  $link = l($activity->subject, 'drop/crm/activity/' . $activity->aid);
  backdrop_set_message(t('!link marked as done.', array('!link' => $link)));
  backdrop_goto('drop/crm/todo');
}

// ---------------------------------------------------------------------------
// Overview dashboard
// ---------------------------------------------------------------------------

/**
 * Page callback: CRM admin overview.
 *
 * @return array
 *   A render array.
 */
function dropcrm_admin_overview() {
  $contact_count  = db_query('SELECT COUNT(*) FROM {crm_contact}')->fetchField();
  $activity_count = db_query('SELECT COUNT(*) FROM {crm_activity}')->fetchField();
  $planned_count  = db_query("SELECT COUNT(*) FROM {crm_activity} WHERE status = 'planned'")->fetchField();

  $output['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  $output['summary'] = array(
    '#theme' => 'item_list',
    '#title' => t('CRM Summary'),
    '#items' => array(
      t('@count total contacts', array('@count' => $contact_count)),
      t('@count total activities', array('@count' => $activity_count)),
      t('@count planned / to-do activities', array('@count' => $planned_count)),
    ),
  );

  // Outreach goal widget — only shown when a goal is configured.
  $config        = config('dropcrm.settings');
  $goal_amount   = (int) $config->get('outreach_goal_amount');
  $goal_period   = $config->get('outreach_goal_period') ?: 'day';
  $window_weeks  = (int) ($config->get('outreach_goal_window_weeks') ?: 4);

  if ($goal_amount > 0) {
    $window_start  = strtotime('-' . $window_weeks . ' weeks', REQUEST_TIME);
    $start_over_ts = (int) state_get('dropcrm.outreach_goal_start', 0);
    $measure_start = max($window_start, $start_over_ts);

    $elapsed_days = max(0, (REQUEST_TIME - $measure_start) / 86400);
    $expected     = round($goal_period === 'week'
      ? $goal_amount * ($elapsed_days / 7)
      : $goal_amount * $elapsed_days, 1);

    $actual = (int) db_query(
      'SELECT COUNT(*) FROM {crm_activity}
       WHERE is_outreach = 1 AND status = :status AND activity_date >= :start',
      array(':status' => 'completed', ':start' => $measure_start)
    )->fetchField();

    $delta       = $actual - $expected;
    $pct         = $expected > 0 ? min(100, round(($actual / $expected) * 100)) : ($actual > 0 ? 100 : 0);
    $ahead       = $delta >= 0;
    $delta_label = $ahead
      ? t('@n ahead of goal', array('@n' => abs(round($delta, 1))))
      : t('@n behind goal',   array('@n' => abs(round($delta, 1))));
    $status_class = $ahead ? 'crm-goal-ahead' : 'crm-goal-behind';
    $period_label = $goal_period === 'week' ? t('per week') : t('per day');

    $output['outreach_goal'] = array(
      '#type'   => 'container',
      '#prefix' => '<h3>' . t('Outreach Goal') . '</h3>',
      'goal_line' => array(
        '#markup' => '<p>' . t('Goal: <strong>@amount @period</strong>', array('@amount' => $goal_amount, '@period' => $period_label)) . '</p>',
      ),
      'bar' => array(
        '#markup' => '<div class="crm-goal-bar-wrap"><div class="crm-goal-bar" style="width:' . $pct . '%"></div></div>'
          . '<p class="crm-goal-bar-label">' . $actual . ' / ' . $expected . ' (' . $pct . '%)'
          . ' &nbsp;<span class="' . $status_class . '">' . $delta_label . '</span></p>',
      ),
      'link' => array(
        '#markup' => '<p>' . l(t('View full report →'), 'drop/crm/reports/outreach-goal') . '</p>',
      ),
    );
  }

  $links = array(
    'contacts' => array(
      'title' => t('Manage Contacts'),
      'href'  => 'drop/crm/contacts',
    ),
    'activities' => array(
      'title' => t('Manage Activities'),
      'href'  => 'drop/crm/activities',
    ),
    'settings' => array(
      'title' => t('CRM Settings'),
      'href'  => 'drop/crm/settings',
    ),
  );

  if (module_exists('droptasks') && user_access('access tasks')) {
    $links['droptasks'] = array(
      'title' => t('Go to Tasks →'),
      'href'  => 'drop/tasks',
    );
  }

  $output['links'] = array(
    '#theme' => 'links',
    '#links' => $links,
  );

  return $output;
}

// ---------------------------------------------------------------------------
// crm_contact form
// ---------------------------------------------------------------------------

/**
 * Form callback for adding and editing CRM contacts.
 *
 * Called by entity_ui via entity_ui_get_form(). The wrapper callback
 * entity_ui_main_form_defaults() pre-populates $form_state['crm_contact'].
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state.
 * @param CrmContact $entity
 *   The contact entity being edited or created.
 * @param string $op
 *   'add' or 'edit'.
 *
 * @return array
 *   The form array.
 */
function crm_contact_form($form, &$form_state, $entity, $op = 'edit') {
  // Ensure the entity is stored in form_state for submit.
  $form_state['crm_contact'] = $entity;

  // The bundle determines which field API fields are attached.
  $bundle = $entity->bundle();

  // For organizations and households, the display name lives directly in the
  // base-table 'name' column. For persons, it is computed from field API fields.
  if (in_array($bundle, array('crm_organization', 'crm_household'))) {
    $label = $bundle === 'crm_household' ? t('Household Name') : t('Organization Name');
    $form['name'] = array(
      '#type'          => 'textfield',
      '#title'         => $label,
      '#default_value' => isset($entity->name) ? $entity->name : '',
      '#required'      => TRUE,
      '#weight'        => -20,
      '#maxlength'     => 255,
    );
  }

  // Active/inactive status.
  $form['status'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Active'),
    '#default_value' => isset($entity->status) ? (int) $entity->status : 1,
    '#weight'        => 50,
  );

  // Attach field API fields for this bundle.
  field_attach_form('crm_contact', $entity, $form, $form_state, LANGUAGE_NONE);

  // Duplicate person confirmation checkbox — shown after validation detects a match.
  if ($bundle === 'crm_person') {
    $show = !empty($form_state['duplicate_person_detected']);
    $form['confirm_duplicate_person'] = array(
      '#type'   => 'checkbox',
      '#title'  => t('I confirm this is a different person with the same name'),
      '#weight' => 49,
      '#access' => $show,
    );
  }

  // Submit button.
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $op === 'add' ? t('Add Contact') : t('Save Contact'),
  );

  return $form;
}

/**
 * Form validation callback for crm_contact_form.
 *
 * Organizations: blocks exact duplicate names.
 * People: warns if a person with the same first + last name already exists.
 */
function crm_contact_form_validate($form, &$form_state) {
  $entity = $form_state['crm_contact'];
  $bundle = $entity->bundle();
  $is_new = empty($entity->cid);
  $lang   = LANGUAGE_NONE;

  if ($bundle === 'crm_organization' || $bundle === 'crm_household') {
    $name = trim($form_state['values']['name']);
    if ($name !== '') {
      $query = db_select('crm_contact', 'c')
        ->fields('c', array('cid', 'name'))
        ->condition('c.type', $bundle)
        ->condition('c.name', $name);
      if (!$is_new) {
        $query->condition('c.cid', $entity->cid, '!=');
      }
      $existing = $query->execute()->fetchObject();
      if ($existing) {
        $link = l($existing->name, 'drop/crm/contact/' . $existing->cid);
        form_set_error('name', t('An organization named "!name" already exists: !link', array(
          '!name' => check_plain($name),
          '!link' => $link,
        )));
      }
    }
  }
  elseif ($bundle === 'crm_person') {
    $first = '';
    $last  = '';
    if (!empty($form_state['values']['field_crm_first_name'][$lang][0]['value'])) {
      $first = trim($form_state['values']['field_crm_first_name'][$lang][0]['value']);
    }
    if (!empty($form_state['values']['field_crm_last_name'][$lang][0]['value'])) {
      $last = trim($form_state['values']['field_crm_last_name'][$lang][0]['value']);
    }
    $full_name = trim($first . ' ' . $last);
    if ($full_name !== '' && empty($form_state['values']['confirm_duplicate_person'])) {
      $query = db_select('crm_contact', 'c')
        ->fields('c', array('cid', 'name'))
        ->condition('c.type', 'crm_person')
        ->condition('c.name', $full_name);
      if (!$is_new) {
        $query->condition('c.cid', $entity->cid, '!=');
      }
      $existing = $query->execute()->fetchObject();
      if ($existing) {
        $link = l($existing->name, 'drop/crm/contact/' . $existing->cid);
        form_set_error('confirm_duplicate_person', t('A person named "!name" already exists: !link. Check the box below to save anyway if this is a different person.', array(
          '!name' => check_plain($full_name),
          '!link' => $link,
        )));
        $form_state['duplicate_person_detected'] = TRUE;
        $form_state['rebuild'] = TRUE;
      }
    }
  }
}

/**
 * Form submit callback for crm_contact_form.
 */
function crm_contact_form_submit($form, &$form_state) {
  $entity = entity_ui_form_submit_build_entity($form, $form_state);

  // Copy the name field from form values (org/household only; person name is
  // computed in hook_crm_contact_presave()).
  if (isset($form_state['values']['name'])) {
    $entity->name = $form_state['values']['name'];
  }
  $entity->status = !empty($form_state['values']['status']) ? 1 : 0;

  $entity->save();
  $link = l($entity->label(), 'drop/crm/contact/' . $entity->cid);
  backdrop_set_message(t('Contact !link saved.', array('!link' => $link)));

  // If a destination was embedded in the form (e.g. from the Clients report),
  // redirect there instead of the contact view page.
  if (!empty($form_state['values']['crm_destination'])) {
    $form_state['redirect'] = $form_state['values']['crm_destination'];
    return;
  }

  // Redirect to the contact's own view page so the person can confirm the save.
  $form_state['redirect'] = 'drop/crm/contact/' . $entity->cid;
}

// ---------------------------------------------------------------------------
// crm_activity form
// ---------------------------------------------------------------------------

/**
 * Form callback for adding and editing CRM activities.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state.
 * @param CrmActivity $entity
 *   The activity entity being edited or created.
 * @param string $op
 *   'add' or 'edit'.
 *
 * @return array
 *   The form array.
 */
function crm_activity_form($form, &$form_state, $entity, $op = 'edit') {
  $form_state['crm_activity'] = $entity;

  $form['subject'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Subject'),
    '#default_value' => isset($entity->subject) ? $entity->subject : '',
    '#required'      => FALSE,
    '#weight'        => -30,
    '#maxlength'     => 255,
    '#attributes'    => array('placeholder' => t('Leave blank to auto-generate from type, contact, and date')),
  );

  $form['type'] = array(
    '#type'          => 'select',
    '#title'         => t('Activity Type'),
    '#options'       => _dropcrm_activity_type_options(),
    '#default_value' => isset($entity->type) ? $entity->type : 'note',
    '#required'      => TRUE,
    '#weight'        => -25,
  );

  $form['status'] = array(
    '#type'          => 'select',
    '#title'         => t('Status'),
    '#options'       => _dropcrm_activity_status_options(),
    '#default_value' => isset($entity->status) ? $entity->status : 'completed',
    '#required'      => TRUE,
    '#weight'        => -20,
  );

  $form['is_outreach'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Count as outreach'),
    '#description'   => t('Check if this activity counts toward your outreach goal.'),
    '#default_value' => isset($entity->is_outreach) ? (int) $entity->is_outreach : 0,
    '#weight'        => -19,
  );

  $form['priority'] = array(
    '#type'          => 'select',
    '#title'         => t('Priority'),
    '#options'       => _dropcrm_activity_priority_options(),
    '#default_value' => isset($entity->priority) ? (int) $entity->priority : 1,
    '#weight'        => -18,
  );

  $form['direction'] = array(
    '#type'          => 'select',
    '#title'         => t('Direction'),
    '#options'       => _dropcrm_activity_direction_options(),
    '#default_value' => isset($entity->direction) ? $entity->direction : '',
    '#weight'        => -15,
    '#description'   => t('Applicable to calls and emails.'),
  );

  // Activity date — when the interaction occurred.
  $default_date = !empty($entity->activity_date)
    ? format_date($entity->activity_date, 'custom', 'Y-m-d')
    : format_date(REQUEST_TIME, 'custom', 'Y-m-d');

  $form['activity_date'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Activity Date'),
    '#default_value' => $default_date,
    '#required'      => TRUE,
    '#weight'        => -10,
    '#description'   => t('Format: YYYY-MM-DD. Click the field to pick a date.'),
    '#maxlength'     => 10,
    '#size'          => 10,
    '#attributes'    => array('class' => array('dropcrm-datepicker')),
  );

  // Due date — optional, for planned/to-do items.
  $default_due = !empty($entity->due_date)
    ? format_date($entity->due_date, 'custom', 'Y-m-d')
    : '';

  $form['due_date'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Date Due'),
    '#default_value' => $default_due,
    '#required'      => FALSE,
    '#weight'        => -9,
    '#description'   => t('Optional. Format: YYYY-MM-DD. Leave blank if no due date.'),
    '#maxlength'     => 10,
    '#size'          => 10,
    '#attributes'    => array('class' => array('dropcrm-datepicker')),
  );

  // Show created date read-only when editing an existing activity.
  if ($op === 'edit' && !empty($entity->created)) {
    $form['created_display'] = array(
      '#type'   => 'item',
      '#title'  => t('Created'),
      '#markup' => format_date($entity->created, 'custom', 'M j, Y'),
      '#weight' => 10,
    );
  }

  $form['#attached']['js'][] = 'core/misc/ui/jquery.ui.datepicker.min.js';
  $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.theme.css';
  $form['#attached']['js'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.datepicker.js';

  // Attach field API fields (body, contacts).
  field_attach_form('crm_activity', $entity, $form, $form_state, LANGUAGE_NONE);

  // Quick-add attendees from organizations.
  $form['crm_attendees_wrapper'] = array(
    '#type'   => 'container',
    '#id'     => 'crm-attendees-wrapper',
    '#weight' => 15,
  );
  // Use #type submit (not button) so #submit handlers fire and the form
  // rebuilds properly through form_builder before the AJAX callback runs.
  $form['crm_attendees_wrapper']['attendees_load'] = array(
    '#type'                    => 'submit',
    '#value'                   => t('Load attendees from organizations'),
    '#name'                    => 'attendees_load',
    '#submit'                  => array('_dropcrm_attendees_load_submit'),
    '#ajax'                    => array(
      'callback' => '_dropcrm_attendees_ajax_callback',
      'wrapper'  => 'crm-attendees-wrapper',
      'effect'   => 'fade',
    ),
    '#limit_validation_errors' => array(),
    '#attributes'              => array('class' => array('button-secondary')),
  );

  // Determine which org CIDs to show affiliates for.
  // Priority: button click (stored in form_state) > edit-mode entity > URL param.
  $lang = LANGUAGE_NONE;
  $org_cids = array();

  if (!empty($form_state['crm_attendees_org_cids'])) {
    // Set by _dropcrm_attendees_load_submit() on button click.
    $org_cids = $form_state['crm_attendees_org_cids'];
  }
  else {
    // Pre-populate when editing an existing activity with org contacts.
    if (!empty($entity->field_crm_contacts[$lang])) {
      $contact_cids = array_column($entity->field_crm_contacts[$lang], 'target_id');
      if ($contact_cids) {
        $result = db_select('crm_contact', 'c')
          ->fields('c', array('cid'))
          ->condition('cid', $contact_cids, 'IN')
          ->condition('type', 'crm_organization')
          ->execute();
        foreach ($result as $row) {
          $org_cids[] = (int) $row->cid;
        }
      }
    }
    // Also check ?contact=CID for add-from-org-page scenario.
    $url_cid = isset($_GET['contact']) ? (int) $_GET['contact'] : 0;
    if ($url_cid && !in_array($url_cid, $org_cids)) {
      $url_type = db_select('crm_contact', 'c')->fields('c', array('type'))
        ->condition('cid', $url_cid)->execute()->fetchField();
      if ($url_type === 'crm_organization') {
        $org_cids[] = $url_cid;
      }
    }
  }

  if (!empty($org_cids)) {
    $already = !empty($entity->field_crm_contacts[$lang])
      ? array_column($entity->field_crm_contacts[$lang], 'target_id')
      : array();
    $form['crm_attendees_wrapper'] += _dropcrm_build_attendees_checkboxes($org_cids, $already);
  }
  elseif (!empty($form_state['crm_attendees_no_orgs'])) {
    $form['crm_attendees_wrapper']['no_orgs_msg'] = array(
      '#markup' => '<p class="crm-attendees-notice">'
        . t('Add an organization to the Contacts field first, then click this button.')
        . '</p>',
    );
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $op === 'add' ? t('Add Activity') : t('Save Activity'),
  );

  return $form;
}

/**
 * Form validate callback for crm_activity_form.
 */
function crm_activity_form_validate($form, &$form_state) {
  $date_string = trim($form_state['values']['activity_date']);
  if (strtotime($date_string) === FALSE) {
    form_set_error('activity_date', t('Please enter a valid date, e.g. 2026-01-15.'));
  }
  $due_string = trim($form_state['values']['due_date']);
  if ($due_string !== '' && strtotime($due_string) === FALSE) {
    form_set_error('due_date', t('Please enter a valid due date, e.g. 2026-01-15.'));
  }
}

/**
 * Form submit callback for crm_activity_form.
 */
function crm_activity_form_submit($form, &$form_state) {
  $entity = entity_ui_form_submit_build_entity($form, $form_state);

  // Copy base-table fields from form values.
  $entity->subject       = $form_state['values']['subject'];
  $entity->type          = $form_state['values']['type'];
  $entity->status        = $form_state['values']['status'];
  $entity->priority      = (int) $form_state['values']['priority'];
  $entity->direction     = $form_state['values']['direction'];
  $entity->is_outreach   = !empty($form_state['values']['is_outreach']) ? 1 : 0;
  $entity->activity_date = strtotime(trim($form_state['values']['activity_date']));
  $due_string = trim($form_state['values']['due_date']);
  $entity->due_date = ($due_string !== '') ? strtotime($due_string) : NULL;

  // Merge checked attendees into field_crm_contacts.
  if (!empty($form_state['values']['attendees'])) {
    $lang = LANGUAGE_NONE;
    if (!isset($entity->field_crm_contacts[$lang])) {
      $entity->field_crm_contacts[$lang] = array();
    }
    $existing_cids = array();
    foreach ($entity->field_crm_contacts[$lang] as $item) {
      $existing_cids[] = (int) $item['target_id'];
    }
    foreach ($form_state['values']['attendees'] as $cid => $checked) {
      $cid = (int) $cid;
      if ($checked && !in_array($cid, $existing_cids)) {
        $entity->field_crm_contacts[$lang][] = array('target_id' => $cid);
        $existing_cids[] = $cid;
      }
    }
  }

  $entity->save();
  $link = l($entity->label(), 'drop/crm/activity/' . $entity->aid);
  backdrop_set_message(t('Activity !link saved.', array('!link' => $link)));
  $form_state['redirect'] = 'drop/crm/activities';
}

// ---------------------------------------------------------------------------
// Activity form — attendees helpers
// ---------------------------------------------------------------------------

/**
 * Submit handler for the "Load attendees" button on the activity form.
 *
 * Parses org CIDs from the raw contacts field input, stores them in
 * $form_state, and triggers a form rebuild so the checkboxes are built
 * properly through form_builder() before the AJAX callback runs.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state (by reference).
 */
function _dropcrm_attendees_load_submit($form, &$form_state) {
  $lang = LANGUAGE_NONE;

  // The tags widget stores its value as a plain string at [field][lang].
  $raw = '';
  if (isset($form_state['input']['field_crm_contacts'][$lang]) && is_string($form_state['input']['field_crm_contacts'][$lang])) {
    $raw = $form_state['input']['field_crm_contacts'][$lang];
  }

  // Extract all CIDs from "Label (CID)" pairs.
  preg_match_all('/\((\d+)\)/', $raw, $matches);
  $cids = !empty($matches[1]) ? array_map('intval', $matches[1]) : array();

  // Filter to org-bundle contacts only.
  $org_cids = array();
  if ($cids) {
    $result = db_select('crm_contact', 'c')
      ->fields('c', array('cid'))
      ->condition('cid', $cids, 'IN')
      ->condition('type', 'crm_organization')
      ->execute();
    foreach ($result as $row) {
      $org_cids[] = (int) $row->cid;
    }
  }

  if (empty($org_cids)) {
    $form_state['crm_attendees_org_cids'] = array();
    $form_state['crm_attendees_no_orgs'] = TRUE;
  }
  else {
    $form_state['crm_attendees_org_cids'] = $org_cids;
    $form_state['crm_attendees_no_orgs'] = FALSE;
  }

  $form_state['rebuild'] = TRUE;
}

/**
 * AJAX callback for the "Load attendees" button on the activity form.
 *
 * The form has already been rebuilt by form_builder() at this point, so
 * all checkboxes are properly processed. Just return the wrapper.
 *
 * @param array $form
 *   The rebuilt form array.
 * @param array $form_state
 *   The form state.
 *
 * @return array
 *   The crm_attendees_wrapper container.
 */
function _dropcrm_attendees_ajax_callback($form, &$form_state) {
  return $form['crm_attendees_wrapper'];
}

/**
 * Builds a checkboxes element for people affiliated with the given org CIDs.
 *
 * @param array $org_cids
 *   Organization contact IDs to look up affiliates for.
 * @param array $already_linked_cids
 *   Person CIDs already linked to the activity — pre-checked.
 *
 * @return array
 *   Render array fragment to merge into crm_attendees_wrapper.
 */
function _dropcrm_build_attendees_checkboxes(array $org_cids, array $already_linked_cids = array()) {
  $person_cids = db_select('field_data_field_crm_employer', 'e')
    ->fields('e', array('entity_id'))
    ->condition('field_crm_employer_target_id', $org_cids, 'IN')
    ->execute()->fetchCol();

  if (empty($person_cids)) {
    return array(
      'no_people_msg' => array(
        '#markup' => '<p class="crm-attendees-notice">'
          . t('No people are currently affiliated with the selected organizations.')
          . '</p>',
      ),
    );
  }

  $people = db_select('crm_contact', 'c')
    ->fields('c', array('cid', 'name'))
    ->condition('cid', $person_cids, 'IN')
    ->orderBy('name')
    ->execute()->fetchAllKeyed();

  $options  = array();
  $defaults = array();
  foreach ($people as $cid => $name) {
    $cid = (int) $cid;
    $options[$cid] = check_plain($name);
    if (in_array($cid, $already_linked_cids)) {
      $defaults[$cid] = $cid;
    }
  }

  return array(
    'attendees' => array(
      '#type'          => 'checkboxes',
      '#title'         => t('Attendees'),
      '#title_display' => 'before',
      '#description'   => t('Check people to add them to the Contacts field for this activity.'),
      '#options'       => $options,
      '#default_value' => $defaults,
    ),
  );
}

// ---------------------------------------------------------------------------
// crm_contact_method form
// ---------------------------------------------------------------------------

/**
 * Form callback for adding and editing CRM contact methods.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state.
 * @param CrmContactMethod $entity
 *   The contact method entity being edited or created.
 * @param string $op
 *   'add' or 'edit'.
 *
 * @return array
 *   The form array.
 */
function crm_contact_method_form($form, &$form_state, $entity, $op = 'edit') {
  $form_state['crm_contact_method'] = $entity;

  $bundle = $entity->bundle();

  // Contact reference: autocomplete by name, stores CID in base table.
  // Pre-populate from the entity (edit) or from ?contact=CID (add from contact page).
  $cid_default = '';
  $contact_locked = FALSE;
  if (!empty($entity->cid)) {
    $contact = crm_contact_load($entity->cid);
    if ($contact) {
      $cid_default = entity_label('crm_contact', $contact) . ' [cid:' . $entity->cid . ']';
    }
  }
  elseif ($op === 'add' && ($preload_cid = (int) backdrop_array_get_nested_value($_GET, array('contact')))) {
    $preload_contact = crm_contact_load($preload_cid);
    if ($preload_contact) {
      $cid_default    = entity_label('crm_contact', $preload_contact) . ' [cid:' . $preload_cid . ']';
      $contact_locked = TRUE;
    }
  }
  $form['cid'] = array(
    '#type'              => 'textfield',
    '#title'             => t('Contact'),
    '#default_value'     => $cid_default,
    '#required'          => TRUE,
    '#weight'            => -30,
    '#autocomplete_path' => 'drop/crm/autocomplete/contacts',
    '#description'       => t('Start typing a contact name.'),
    '#maxlength'         => 300,
    '#access'            => !$contact_locked,
  );
  // Hidden field carries the CID when the visible field is suppressed.
  if ($contact_locked) {
    $form['cid_locked'] = array(
      '#type'  => 'hidden',
      '#value' => $cid_default,
      '#name'  => 'cid',
    );
  }

  // Label (Work, Home, etc.) — options come from config.
  $label_options = _dropcrm_contact_method_label_options($bundle);
  $form['label'] = array(
    '#type'          => 'select',
    '#title'         => t('Label'),
    '#options'       => $label_options,
    '#default_value' => isset($entity->label) ? $entity->label : '',
    '#required'      => TRUE,
    '#weight'        => -25,
  );

  // Value field — for email, telephone, and url; address components are fields.
  if ($bundle !== 'address') {
    $titles = array(
      'email'     => t('Email Address'),
      'telephone' => t('Phone Number'),
      'url'       => t('URL'),
    );
    $form['value'] = array(
      '#type'          => 'textfield',
      '#title'         => isset($titles[$bundle]) ? $titles[$bundle] : t('Value'),
      '#default_value' => isset($entity->value) ? $entity->value : '',
      '#required'      => TRUE,
      '#weight'        => -20,
      '#maxlength'     => 255,
    );
  }

  $form['is_primary'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Primary'),
    '#description'   => t('Mark as the primary contact method of this type for the contact.'),
    '#default_value' => isset($entity->is_primary) ? (int) $entity->is_primary : 0,
    '#weight'        => -10,
  );

  // Attach field API fields (address components for the address bundle).
  field_attach_form('crm_contact_method', $entity, $form, $form_state, LANGUAGE_NONE);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $op === 'add' ? t('Add Contact Method') : t('Save Contact Method'),
  );

  return $form;
}

/**
 * Form submit callback for crm_contact_method_form.
 */
function crm_contact_method_form_submit($form, &$form_state) {
  $entity = entity_ui_form_submit_build_entity($form, $form_state);

  // Parse the CID from "Contact Name [cid:N]" autocomplete format.
  $cid_raw = $form_state['values']['cid'];
  if (preg_match('/\[cid:(\d+)\]/', $cid_raw, $matches)) {
    $entity->cid = (int) $matches[1];
  }
  else {
    $entity->cid = (int) $cid_raw;
  }
  $entity->label      = $form_state['values']['label'];
  $entity->is_primary = !empty($form_state['values']['is_primary']) ? 1 : 0;

  if (isset($form_state['values']['value'])) {
    $entity->value = $form_state['values']['value'];
  }

  $entity->save();
  $contact_link = l(t('View contact'), 'drop/crm/contact/' . $entity->cid);
  backdrop_set_message(t('Contact method saved. !link.', array('!link' => $contact_link)));
  $form_state['redirect'] = 'drop/crm/contact-methods';
}

// ---------------------------------------------------------------------------
// Settings form
// ---------------------------------------------------------------------------

/**
 * Form callback: CRM global settings.
 *
 * Renders an editable table of activity types. Existing types can have their
 * label and direction flag changed, or be deleted (if not in use). A new type
 * can be added at the bottom.
 *
 * Uses the prefix/suffix table pattern: siblings in the form array are given
 * weights so they render in row order, with <tr>/<td> tags in #prefix/#suffix.
 *
 * @return array
 *   A Form API array.
 */
function dropcrm_settings_form($form, &$form_state) {
  $config = config('dropcrm.settings');
  $activity_types = $config->get('activity_types') ?: array();

  $form['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  $form['custom_fields'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Custom Fields'),
    '#description' => t('Add or remove fields on contact types.'),
    '#weight'      => -20,
  );
  $bundles = array(
    'crm_person'       => t('People'),
    'crm_organization' => t('Organizations'),
  );
  if ($config->get('enable_households')) {
    $bundles['crm_household'] = t('Households');
  }
  $links = array();
  foreach ($bundles as $bundle => $label) {
    $links[] = l($label, 'drop/crm/contacts/manage/' . $bundle . '/fields');
  }
  $links[] = l(t('Activities'), 'drop/crm/activities/manage/crm_activity/fields');
  if (module_exists('droptasks')) {
    $links[] = l(t('Tasks'), 'drop/tasks/configure/manage/drop_task/fields');
  }
  $form['custom_fields']['links'] = array(
    '#markup' => implode(' &nbsp;·&nbsp; ', $links),
  );

  $form['features'] = array(
    '#type'   => 'fieldset',
    '#title'  => t('Features'),
    '#weight' => -10,
  );
  $form['features']['enable_households'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Enable Households'),
    '#description'   => t('Show the Households tab in the CRM. Disable to hide it without removing the feature from the module.'),
    '#default_value' => $config->get('enable_households') ? 1 : 0,
  );

  // Outreach goal settings.
  $form['outreach_goal'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Outreach Goal'),
    '#description' => t('Track your outreach pace. Any completed activity with the "Count as outreach" checkbox ticked contributes to this goal. View progress at <a href="@url">Reports → Outreach Goal</a>.', array('@url' => url('drop/crm/reports/outreach-goal'))),
    '#weight'      => -5,
  );
  $form['outreach_goal']['outreach_goal_amount'] = array(
    '#type'          => 'number',
    '#title'         => t('Goal'),
    '#field_suffix'  => t('outreach activities'),
    '#default_value' => $config->get('outreach_goal_amount') ?: 0,
    '#min'           => 0,
    '#step'          => 1,
    '#size'          => 5,
    '#description'   => t('Set to 0 to disable goal tracking.'),
  );
  $form['outreach_goal']['outreach_goal_period'] = array(
    '#type'          => 'select',
    '#title'         => t('Per'),
    '#options'       => array('day' => t('Day'), 'week' => t('Week')),
    '#default_value' => $config->get('outreach_goal_period') ?: 'day',
  );
  $window_options = array(
    1  => t('1 week'),
    2  => t('2 weeks'),
    4  => t('4 weeks'),
    8  => t('8 weeks'),
    12 => t('12 weeks'),
  );
  $form['outreach_goal']['outreach_goal_window_weeks'] = array(
    '#type'          => 'select',
    '#title'         => t('Measurement window'),
    '#options'       => $window_options,
    '#default_value' => $config->get('outreach_goal_window_weeks') ?: 4,
    '#description'   => t('How far back to measure your progress. The "Start over" button on the Outreach Goal report page resets this window.'),
  );

  // Table header.
  $form['at_table_open'] = array(
    '#markup' => '<table class="dropcrm-settings-table">'
      . '<thead><tr>'
      . '<th>' . t('Key') . '</th>'
      . '<th>' . t('Label') . '</th>'
      . '<th>' . t('Has direction') . '</th>'
      . '<th>' . t('Delete') . '</th>'
      . '</tr></thead><tbody>',
    '#prefix' => '<h3>' . t('Activity Types') . '</h3>'
      . '<p>' . t('Activity types classify how a contact interaction was made. The machine key cannot be changed after saving.') . '</p>',
    '#weight' => 0,
  );

  $w = 1;
  foreach ($activity_types as $key => $info) {
    $in_use = (bool) db_query(
      'SELECT COUNT(*) FROM {crm_activity} WHERE type = :type',
      array(':type' => $key)
    )->fetchField();

    // Key column — opens the <tr>.
    $form['at_key_' . $key] = array(
      '#markup' => '<td><code>' . check_plain($key) . '</code></td>',
      '#prefix' => '<tr>',
      '#weight' => $w,
    );

    $form['at_label_' . $key] = array(
      '#type'          => 'textfield',
      '#title'         => t('Label'),
      '#title_display' => 'invisible',
      '#default_value' => isset($info['label']) ? $info['label'] : '',
      '#size'          => 24,
      '#maxlength'     => 64,
      '#prefix'        => '<td>',
      '#suffix'        => '</td>',
      '#weight'        => $w + 0.1,
    );

    $form['at_dir_' . $key] = array(
      '#type'          => 'checkbox',
      '#title'         => t('Has direction'),
      '#title_display' => 'invisible',
      '#default_value' => !empty($info['has_direction']) ? 1 : 0,
      '#prefix'        => '<td class="checkbox-cell">',
      '#suffix'        => '</td>',
      '#weight'        => $w + 0.2,
    );

    $form['at_del_' . $key] = array(
      '#type'          => 'checkbox',
      '#title'         => t('Delete'),
      '#title_display' => 'invisible',
      '#default_value' => 0,
      '#disabled'      => $in_use,
      '#description'   => $in_use ? t('In use') : '',
      '#prefix'        => '<td class="checkbox-cell">',
      '#suffix'        => '</td></tr>',
      '#weight'        => $w + 0.3,
    );

    $w += 1;
  }

  $form['at_table_close'] = array(
    '#markup' => '</tbody></table>',
    '#weight' => $w,
  );

  // Add new type.
  $form['new_type'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Add a new activity type'),
    '#collapsible' => FALSE,
    '#weight'      => $w + 1,
  );

  $form['new_type']['new_type_key'] = array(
    '#type'        => 'textfield',
    '#title'       => t('Machine key'),
    '#description' => t('Lowercase letters, numbers, and underscores only — e.g. %example.', array('%example' => 'linkedin_message')),
    '#size'        => 24,
    '#maxlength'   => 32,
  );

  $form['new_type']['new_type_label'] = array(
    '#type'     => 'textfield',
    '#title'    => t('Label'),
    '#size'     => 24,
    '#maxlength' => 64,
  );

  $form['new_type']['new_type_has_direction'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Has direction (inbound / outbound)'),
    '#default_value' => 0,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => $w + 10);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save activity types'),
  );

  return $form;
}

/**
 * Validate callback for dropcrm_settings_form.
 */
function dropcrm_settings_form_validate($form, &$form_state) {
  $values = $form_state['values'];
  $activity_types = config_get('dropcrm.settings', 'activity_types') ?: array();

  // Existing type labels must not be blanked out unless the row is being deleted.
  foreach (array_keys($activity_types) as $key) {
    if (empty($values['at_del_' . $key]) && trim($values['at_label_' . $key]) === '') {
      form_set_error('at_label_' . $key, t('The label for %key cannot be empty.', array('%key' => $key)));
    }
  }

  // New type: validate only when either field is filled.
  $new_key   = trim($values['new_type_key']);
  $new_label = trim($values['new_type_label']);

  if ($new_key || $new_label) {
    if (!$new_key) {
      form_set_error('new_type_key', t('Please enter a machine key for the new type.'));
    }
    elseif (!preg_match('/^[a-z0-9_]+$/', $new_key)) {
      form_set_error('new_type_key', t('Machine key may only contain lowercase letters, numbers, and underscores.'));
    }
    elseif (isset($activity_types[$new_key])) {
      form_set_error('new_type_key', t('A type with the key %key already exists.', array('%key' => $new_key)));
    }

    if (!$new_label) {
      form_set_error('new_type_label', t('Please enter a label for the new type.'));
    }
  }
}

// ---------------------------------------------------------------------------
// Relationship forms
// ---------------------------------------------------------------------------

/**
 * Returns the default relationship types array.
 *
 * @return array
 *   Array of relationship type definitions keyed by machine key.
 */
function _dropcrm_default_relationship_types() {
  return array(
    'referral'     => array('key' => 'referral',     'label' => 'Referral / Connection'),
    'spouse'       => array('key' => 'spouse',       'label' => 'Spouse / Partner'),
    'parent_child' => array('key' => 'parent_child', 'label' => 'Parent / Child'),
    'sibling'      => array('key' => 'sibling',      'label' => 'Sibling'),
    'friend'       => array('key' => 'friend',       'label' => 'Friend'),
    'colleague'    => array('key' => 'colleague',    'label' => 'Colleague'),
    'other'        => array('key' => 'other',        'label' => 'Other'),
  );
}

/**
 * Returns a key => label array of relationship types from config.
 *
 * @return array
 */
function _dropcrm_relationship_type_options() {
  $types = config_get('dropcrm.settings', 'relationship_types');
  if (empty($types)) {
    $types = _dropcrm_default_relationship_types();
  }
  $options = array();
  foreach ($types as $key => $info) {
    $options[$key] = t($info['label']);
  }
  return $options;
}

/**
 * Form callback: add or edit a CRM relationship.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state.
 * @param int|object|null $rid_or_object
 *   A numeric relationship ID for editing, or NULL for adding.
 *
 * @return array
 *   The form array.
 */
function dropcrm_relationship_form($form, &$form_state, $rid_or_object = NULL) {
  $form['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  $form['relationship_help'] = array(
    '#type'   => 'markup',
    '#markup' => '<p class="crm-section-help">' . t('Relationships link two <strong>people</strong> — family members, colleagues, and similar connections. To associate a person with an organization, edit the person\'s contact profile and set the <strong>Employer</strong> field instead.') . '</p>',
    '#weight' => -10,
  );

  // Resolve to an existing record or set defaults.
  $relationship = NULL;
  $rid = 0;

  if (is_object($rid_or_object)) {
    $relationship = $rid_or_object;
    $rid = (int) $relationship->rid;
  }
  elseif (is_numeric($rid_or_object) && (int) $rid_or_object > 0) {
    $relationship = crm_relationship_load((int) $rid_or_object);
    $rid = $relationship ? (int) $relationship->rid : 0;
  }

  $form_state['rid'] = $rid;

  // Default values for contact_a field.
  $contact_a_default = '';
  if ($relationship) {
    $contact_a = crm_contact_load($relationship->cid_a);
    if ($contact_a) {
      $contact_a_default = entity_label('crm_contact', $contact_a) . ' [cid:' . $relationship->cid_a . ']';
    }
  }
  elseif (isset($_GET['contact']) && (int) $_GET['contact']) {
    $preloaded = crm_contact_load((int) $_GET['contact']);
    if ($preloaded) {
      $contact_a_default = entity_label('crm_contact', $preloaded) . ' [cid:' . $preloaded->cid . ']';
    }
  }

  // Default values for contact_b field.
  $contact_b_default = '';
  if ($relationship) {
    $contact_b = crm_contact_load($relationship->cid_b);
    if ($contact_b) {
      $contact_b_default = entity_label('crm_contact', $contact_b) . ' [cid:' . $relationship->cid_b . ']';
    }
  }

  $form['contact_a'] = array(
    '#type'              => 'textfield',
    '#title'             => t('Person'),
    '#default_value'     => $contact_a_default,
    '#required'          => TRUE,
    '#autocomplete_path' => 'drop/crm/autocomplete/contacts',
    '#description'       => t('Start typing a contact name.'),
    '#maxlength'         => 300,
    '#weight'            => -30,
  );

  $form['contact_b'] = array(
    '#type'              => 'textfield',
    '#title'             => t('Related Contact'),
    '#default_value'     => $contact_b_default,
    '#required'          => TRUE,
    '#autocomplete_path' => 'drop/crm/autocomplete/contacts',
    '#description'       => t('Start typing a contact name.'),
    '#maxlength'         => 300,
    '#weight'            => -25,
  );

  $form['type'] = array(
    '#type'          => 'select',
    '#title'         => t('Relationship Type'),
    '#options'       => _dropcrm_relationship_type_options(),
    '#default_value' => $relationship ? $relationship->type : 'other',
    '#required'      => TRUE,
    '#weight'        => -20,
  );

  $form['notes'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Notes'),
    '#default_value' => $relationship ? $relationship->notes : '',
    '#required'      => FALSE,
    '#maxlength'     => 255,
    '#weight'        => -15,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 100);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $rid ? t('Save Relationship') : t('Add Relationship'),
  );

  return $form;
}

/**
 * Validate callback for dropcrm_relationship_form.
 */
function dropcrm_relationship_form_validate($form, &$form_state) {
  $values = $form_state['values'];

  // Parse contact_a CID.
  if (!preg_match('/\[cid:(\d+)\]/', $values['contact_a'], $m_a)) {
    form_set_error('contact_a', t('Please select a valid contact from the autocomplete list.'));
    return;
  }
  $cid_a = (int) $m_a[1];

  // Parse contact_b CID.
  if (!preg_match('/\[cid:(\d+)\]/', $values['contact_b'], $m_b)) {
    form_set_error('contact_b', t('Please select a valid contact from the autocomplete list.'));
    return;
  }
  $cid_b = (int) $m_b[1];

  // Prevent self-relationships.
  if ($cid_a === $cid_b) {
    form_set_error('contact_b', t('A contact cannot be related to themselves.'));
    return;
  }

  // Prevent person-to-organization relationships. Use the Employer field instead.
  $contact_a = crm_contact_load($cid_a);
  $contact_b = crm_contact_load($cid_b);
  if ($contact_a && $contact_b) {
    $bundles = array($contact_a->bundle(), $contact_b->bundle());
    if (in_array('crm_organization', $bundles)) {
      form_set_error('contact_b', t('Relationships can only link two people. To associate a person with an organization, edit the person\'s profile and set the <strong>Employer</strong> field.', array(), array('context' => 'relationship form')));
      return;
    }
  }

  // Check for duplicate (same pair, same type), skipping this rid when editing.
  $type = $values['type'];
  $rid  = $form_state['rid'];

  $query = db_select('crm_relationship', 'r')
    ->fields('r', array('rid'))
    ->condition('type', $type);
  $or = db_or()
    ->condition(db_and()
      ->condition('cid_a', $cid_a)
      ->condition('cid_b', $cid_b)
    )
    ->condition(db_and()
      ->condition('cid_a', $cid_b)
      ->condition('cid_b', $cid_a)
    );
  $query->condition($or);
  if ($rid) {
    $query->condition('rid', $rid, '<>');
  }
  if ($query->countQuery()->execute()->fetchField()) {
    form_set_error('type', t('A relationship of this type already exists between these two contacts.'));
    return;
  }

  // Pass parsed CIDs to submit.
  $form_state['relationship_cids'] = array('cid_a' => $cid_a, 'cid_b' => $cid_b);
}

/**
 * Submit callback for dropcrm_relationship_form.
 */
function dropcrm_relationship_form_submit($form, &$form_state) {
  $cid_a = $form_state['relationship_cids']['cid_a'];
  $cid_b = $form_state['relationship_cids']['cid_b'];
  $type  = $form_state['values']['type'];
  $notes = trim($form_state['values']['notes']);
  $rid   = $form_state['rid'];

  // Redirect back to the originating contact if ?contact is set.
  $return_cid = isset($_GET['contact']) && (int) $_GET['contact']
    ? (int) $_GET['contact']
    : $cid_a;
  $contact_link = l(t('View contact'), 'drop/crm/contact/' . $return_cid);

  if ($rid) {
    db_update('crm_relationship')
      ->fields(array(
        'cid_a'  => $cid_a,
        'cid_b'  => $cid_b,
        'type'   => $type,
        'notes'  => $notes,
      ))
      ->condition('rid', $rid)
      ->execute();
    backdrop_set_message(t('Relationship updated. !link.', array('!link' => $contact_link)));
  }
  else {
    db_insert('crm_relationship')
      ->fields(array(
        'cid_a'   => $cid_a,
        'cid_b'   => $cid_b,
        'type'    => $type,
        'notes'   => $notes,
        'created' => REQUEST_TIME,
      ))
      ->execute();
    backdrop_set_message(t('Relationship added. !link.', array('!link' => $contact_link)));
  }

  $form_state['redirect'] = 'drop/crm/contact/' . $return_cid;
}

/**
 * Confirm form: delete a CRM relationship.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state.
 * @param int $rid
 *   The relationship ID to delete (passed from page arguments).
 *
 * @return array
 *   The form array.
 */
function dropcrm_relationship_delete_confirm($form, &$form_state, $rid) {
  $rid = (int) $rid;
  $relationship = crm_relationship_load($rid);

  if (!$relationship) {
    backdrop_set_message(t('Relationship not found.'), 'error');
    backdrop_goto('<front>');
  }

  $contact_a = crm_contact_load($relationship->cid_a);
  $contact_b = crm_contact_load($relationship->cid_b);
  $name_a = $contact_a ? entity_label('crm_contact', $contact_a) : t('Unknown');
  $name_b = $contact_b ? entity_label('crm_contact', $contact_b) : t('Unknown');

  $type_options = _dropcrm_relationship_type_options();
  $type_label = isset($type_options[$relationship->type]) ? $type_options[$relationship->type] : $relationship->type;

  $form_state['relationship_rid']    = $rid;
  $form_state['relationship_return'] = $relationship->cid_a;

  return confirm_form(
    $form,
    t('Are you sure you want to delete the %type relationship between %a and %b?',
      array('%type' => $type_label, '%a' => $name_a, '%b' => $name_b)),
    'drop/crm/contact/' . $relationship->cid_a,
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit callback for dropcrm_relationship_delete_confirm.
 */
function dropcrm_relationship_delete_confirm_submit($form, &$form_state) {
  $rid        = $form_state['relationship_rid'];
  $return_cid = $form_state['relationship_return'];

  db_delete('crm_relationship')
    ->condition('rid', $rid)
    ->execute();

  backdrop_set_message(t('Relationship deleted.'));
  $form_state['redirect'] = 'drop/crm/contact/' . $return_cid;
}

/**
 * Submit callback for dropcrm_settings_form.
 */
function dropcrm_settings_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $config = config('dropcrm.settings');
  $activity_types = $config->get('activity_types') ?: array();

  // Update / delete existing types.
  foreach (array_keys($activity_types) as $key) {
    if (!empty($values['at_del_' . $key])) {
      $in_use = (bool) db_query(
        'SELECT COUNT(*) FROM {crm_activity} WHERE type = :type',
        array(':type' => $key)
      )->fetchField();
      if (!$in_use) {
        unset($activity_types[$key]);
        continue;
      }
      // Fallback safety — shouldn't be reachable due to #disabled.
      backdrop_set_message(t('Type %key is in use and cannot be deleted.', array('%key' => $key)), 'warning');
    }
    else {
      $activity_types[$key]['label']         = trim($values['at_label_' . $key]);
      $activity_types[$key]['has_direction'] = (bool) $values['at_dir_' . $key];
    }
  }

  // Add new type if supplied.
  $new_key   = trim($values['new_type_key']);
  $new_label = trim($values['new_type_label']);
  if ($new_key && $new_label) {
    $activity_types[$new_key] = array(
      'label'         => $new_label,
      'has_direction' => (bool) $values['new_type_has_direction'],
    );
  }

  $config->set('activity_types', $activity_types);
  $config->set('enable_households', (bool) $values['enable_households']);
  $config->set('outreach_goal_amount', (int) $values['outreach_goal_amount']);
  $config->set('outreach_goal_period', $values['outreach_goal_period']);
  $config->set('outreach_goal_window_weeks', (int) $values['outreach_goal_window_weeks']);
  $config->save();

  // Rebuild the menu so the Households tab appears/disappears immediately.
  menu_rebuild();

  backdrop_set_message(t('Settings saved.'));
}

// ---------------------------------------------------------------------------
// Outreach Goal report
// ---------------------------------------------------------------------------

/**
 * Page callback: Outreach goal progress report.
 *
 * Shows how many outreach activities have been completed relative to the
 * configured goal, over the measurement window or since the last "start over".
 *
 * @return array
 *   A render array.
 */
function dropcrm_report_outreach_goal() {
  $config = config('dropcrm.settings');
  $goal_amount       = (int) $config->get('outreach_goal_amount');
  $goal_period       = $config->get('outreach_goal_period') ?: 'day';
  $window_weeks      = (int) ($config->get('outreach_goal_window_weeks') ?: 4);

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  if (empty($goal_amount)) {
    $output['no_goal'] = array(
      '#markup' => '<p>' . t('No outreach goal has been set. Configure one on the <a href="@url">Settings page</a>.', array('@url' => url('drop/crm/settings'))) . '</p>',
    );
    return $output;
  }

  // Determine measurement start: the later of (start-over date) and (window ago).
  $window_start   = strtotime('-' . $window_weeks . ' weeks', REQUEST_TIME);
  $start_over_ts  = (int) state_get('dropcrm.outreach_goal_start', 0);
  $measure_start  = max($window_start, $start_over_ts);

  // Days elapsed since measurement start (capped at the window length).
  $elapsed_seconds = REQUEST_TIME - $measure_start;
  $elapsed_days    = max(0, $elapsed_seconds / 86400);

  // Expected count based on elapsed time.
  if ($goal_period === 'week') {
    $expected = $goal_amount * ($elapsed_days / 7);
  }
  else {
    $expected = $goal_amount * $elapsed_days;
  }
  $expected = round($expected, 1);

  // Actual completed outreach activities in the window.
  $actual = (int) db_query(
    'SELECT COUNT(*) FROM {crm_activity}
     WHERE is_outreach = 1
       AND status = :status
       AND activity_date >= :start',
    array(':status' => 'completed', ':start' => $measure_start)
  )->fetchField();

  $delta        = $actual - $expected;
  $delta_abs    = abs(round($delta, 1));
  $delta_label  = ($delta >= 0)
    ? t('@n ahead of goal', array('@n' => $delta_abs))
    : t('@n behind goal', array('@n' => $delta_abs));
  $delta_class  = ($delta >= 0) ? 'crm-goal-ahead' : 'crm-goal-behind';

  // Progress percentage (capped at 100 for the bar width only).
  $pct          = $expected > 0 ? min(100, round(($actual / $expected) * 100)) : ($actual > 0 ? 100 : 0);

  // Goal description string.
  $period_label = ($goal_period === 'week') ? t('per week') : t('per day');
  $goal_desc    = t('@amount outreach @period', array('@amount' => $goal_amount, '@period' => $period_label));

  $start_label  = format_date($measure_start, 'custom', 'M j, Y');
  $today_label  = format_date(REQUEST_TIME, 'custom', 'M j, Y');
  $elapsed_label = t('@days days', array('@days' => (int) $elapsed_days));

  // Build the summary table.
  $rows = array(
    array(t('Goal'), $goal_desc),
    array(t('Measuring from'), $start_label . ' → ' . $today_label . ' (' . $elapsed_label . ')'),
    array(t('Expected by now'), $expected),
    array(t('Actual'), $actual),
    array(
      t('Status'),
      array(
        'data'  => $delta_label,
        'class' => array($delta_class),
      ),
    ),
  );

  $output['summary'] = array(
    '#theme'   => 'table',
    '#rows'    => $rows,
    '#attributes' => array('class' => array('crm-goal-summary')),
  );

  // Simple progress bar.
  $output['progress'] = array(
    '#markup' => '<div class="crm-goal-bar-wrap"><div class="crm-goal-bar" style="width:' . $pct . '%"></div></div>'
      . '<p class="crm-goal-bar-label">' . $actual . ' / ' . $expected . ' (' . $pct . '%)</p>',
  );

  // Start over form.
  $output['start_over'] = backdrop_get_form('dropcrm_outreach_start_over_form');

  return $output;
}

/**
 * Form: "Start over" button for the outreach goal.
 */
function dropcrm_outreach_start_over_form($form, &$form_state) {
  $current_start = (int) state_get('dropcrm.outreach_goal_start', 0);
  $current_label = $current_start
    ? format_date($current_start, 'custom', 'M j, Y')
    : t('beginning of measurement window');

  $form['help'] = array(
    '#markup' => '<p>' . t('Current start date: <strong>@date</strong>. Resetting moves the measurement baseline to the date below. Your activity history is preserved; the goal counter restarts from zero.', array('@date' => $current_label)) . '</p>',
  );
  $form['start_date'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Start date'),
    '#default_value' => format_date(REQUEST_TIME, 'custom', 'Y-m-d'),
    '#description'   => t('Format: YYYY-MM-DD. Defaults to today.'),
    '#maxlength'     => 10,
    '#size'          => 12,
    '#attributes'    => array('class' => array('dropcrm-datepicker')),
  );
  $form['#attached']['js'][]  = 'core/misc/ui/jquery.ui.datepicker.min.js';
  $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.theme.css';
  $form['#attached']['js'][]  = backdrop_get_path('module', 'dropcrm') . '/dropcrm.datepicker.js';

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Set start date'),
  );
  return $form;
}

/**
 * Validate handler for the start-over form.
 */
function dropcrm_outreach_start_over_form_validate($form, &$form_state) {
  $date_string = trim($form_state['values']['start_date']);
  if ($date_string === '' || strtotime($date_string) === FALSE) {
    form_set_error('start_date', t('Please enter a valid date, e.g. 2026-01-15.'));
  }
}

/**
 * Submit handler for the start-over form.
 */
function dropcrm_outreach_start_over_form_submit($form, &$form_state) {
  $ts = strtotime(trim($form_state['values']['start_date']));
  state_set('dropcrm.outreach_goal_start', $ts);
  backdrop_set_message(t('Outreach goal measurement restarted from @date.', array(
    '@date' => format_date($ts, 'custom', 'M j, Y'),
  )));
  $form_state['redirect'] = 'drop/crm/reports/outreach-goal';
}

// ---------------------------------------------------------------------------
// Project management
// ---------------------------------------------------------------------------

/**
 * Page callback: list all projects.
 */
function dropcrm_project_list() {
  backdrop_set_title(t('Projects'));

  $projects = db_select('drop_project', 'p')
    ->fields('p')
    ->orderBy('p.active', 'DESC')
    ->orderBy('p.name', 'ASC')
    ->execute()
    ->fetchAll();

  if (empty($projects)) {
    return '<p>' . t('No projects yet. !link.', array(
      '!link' => l(t('Add your first project'), 'drop/crm/projects/add'),
    )) . '</p>';
  }

  $colors = dropcrm_color_options();

  $rows = array();
  foreach ($projects as $project) {
    $color       = check_plain($project->color);
    $swatch      = '<span class="droptime-swatch" style="background:' . $color . ';"></span>';
    $status      = $project->active ? t('Active') : '<em>' . t('Archived') . '</em>';
    $label       = $swatch . ' ' . l(check_plain($project->name), 'drop/crm/projects/' . $project->pid);
    $color_label = isset($colors[$project->color]) ? $colors[$project->color] : $project->color;

    // Linked contact.
    $contact_cell = '—';
    if (!empty($project->cid)) {
      $contacts = entity_load('crm_contact', array((int) $project->cid));
      if ($contacts) {
        $contact = reset($contacts);
        $contact_cell = l(check_plain($contact->name), 'drop/crm/contact/' . $project->cid);
      }
    }

    $links = array(
      l(t('Edit'), 'drop/crm/projects/' . $project->pid . '/edit'),
      l(t('Delete'), 'drop/crm/projects/' . $project->pid . '/delete'),
    );

    $prefix_cell = !empty($project->prefix)
      ? '<code>' . check_plain($project->prefix) . '</code>'
      : '—';

    $row = array(
      array('data' => $label),
      array('data' => $prefix_cell),
      array('data' => $contact_cell),
      array('data' => '<span class="dropcrm-color-swatch" style="background:' . $color . ';display:inline-block;width:24px;height:24px;border-radius:4px;border:1px solid rgba(0,0,0,.15);"></span>'),
      array('data' => $status),
      array('data' => implode(' · ', $links)),
    );
    $rows[] = $row;
  }

  $header = array(t('Project'), t('Prefix'), t('Contact'), t('Color'), t('Status'), t('Actions'));

  return theme('table', array(
    'header'     => $header,
    'rows'       => $rows,
    'attributes' => array('class' => array('droptime-project-table')),
  ));
}

/**
 * Page callback: view a single project with its ticket list.
 */
function dropcrm_project_view($project) {
  backdrop_set_title(check_plain($project->name));

  $css = array(backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css');
  if (module_exists('droptickets')) {
    $css[] = backdrop_get_path('module', 'droptickets') . '/droptickets.css';
  }
  $output = array(
    '#attached' => array('css' => $css),
  );

  // Meta bar — color swatch, prefix, contact, status.
  $colors = dropcrm_color_options();
  $color  = check_plain($project->color);
  $swatch = '<span class="droptime-swatch" style="background:' . $color . ';"></span>';
  $color_label = isset($colors[$project->color]) ? $colors[$project->color] : $project->color;

  $contact_html = '—';
  if (!empty($project->cid)) {
    $contacts = entity_load('crm_contact', array((int) $project->cid));
    if ($contacts) {
      $contact = reset($contacts);
      $contact_html = l(check_plain($contact->name), 'drop/crm/contact/' . $project->cid);
    }
  }

  $meta = '<div class="dropcrm-project-meta">';
  $meta .= '<span>' . $swatch . ' ' . check_plain($color_label) . '</span>';
  if (!empty($project->prefix)) {
    $meta .= '<span>' . t('Prefix: <code>@p</code>', array('@p' => $project->prefix)) . '</span>';
  }
  $meta .= '<span>' . t('Contact: !c', array('!c' => $contact_html)) . '</span>';
  if (!$project->active) {
    $meta .= '<span class="dropcrm-project-archived">' . t('Archived') . '</span>';
  }
  $meta .= '</div>';

  $actions = '';
  if (user_access('administer crm')) {
    $actions = '<div class="drop-page-actions">'
      . l(t('Edit project'), 'drop/crm/projects/' . $project->pid . '/edit')
      . l(t('Delete'), 'drop/crm/projects/' . $project->pid . '/delete')
      . '</div>';
  }

  $output['meta'] = array(
    '#markup' => $meta . $actions,
    '#weight' => -10,
  );

  // Tickets for this project.
  if (module_exists('droptickets')) {
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
      $tabs .= '<a href="' . url('drop/crm/projects/' . $project->pid, array('query' => array('status' => $key))) . '"' . $active . '>' . check_plain($label) . '</a> ';
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

    $output['ticket_tabs'] = array(
      '#markup' => '<div class="drop-section-header">'
        . '<h2>' . t('Tickets (@total)', array('@total' => $ticket_count)) . '</h2>'
        . '<div class="drop-section-add-group">' . $add_link . '</div>'
        . '</div>'
        . '<div class="dropcrm-project-status-tabs">' . $tabs . '</div>',
      '#weight' => 0,
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
      '#weight'     => 1,
    );
    $output['pager'] = array('#theme' => 'pager', '#weight' => 10);
  }

  $output['back'] = array(
    '#markup' => '<p>' . l(t('← All projects'), 'drop/crm/projects') . '</p>',
    '#weight' => 20,
  );

  return $output;
}

/**
 * Form: add or edit a project.
 *
 * @param DropProject|null $project
 *   Existing project for editing, or NULL to add.
 */
function dropcrm_project_form($form, &$form_state, $project = NULL) {
  $is_edit = !empty($project);

  $form['#project'] = $project;

  $form['name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Project name'),
    '#default_value' => $is_edit ? $project->name : '',
    '#required'      => TRUE,
    '#maxlength'     => 255,
  );

  // Contact autocomplete.
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

  $form['color'] = array(
    '#type'          => 'select',
    '#title'         => t('Color'),
    '#options'       => dropcrm_color_options(),
    '#default_value' => $is_edit ? $project->color : '#1565c0',
    '#required'      => TRUE,
    '#description'   => t('Used to identify this project in the time log and reports.'),
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

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Update project') : t('Add project'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'drop/crm/projects'),
  );

  return $form;
}

/**
 * Validation handler for dropcrm_project_form().
 *
 * Blocks duplicate project names.
 */
function dropcrm_project_form_validate($form, &$form_state) {
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
    $link = l($existing->name, 'drop/crm/projects/' . $existing->pid);
    form_set_error('name', t('A project named "!name" already exists: !link', array(
      '!name' => check_plain($name),
      '!link' => $link,
    )));
  }
}

/**
 * Submit handler for dropcrm_project_form().
 */
function dropcrm_project_form_submit($form, &$form_state) {
  $values = $form_state['values'];

  if (!empty($values['pid'])) {
    $projects = entity_load('drop_project', array($values['pid']));
    $project  = $projects ? reset($projects) : entity_create('drop_project', array());
  }
  else {
    $project = entity_create('drop_project', array());
  }

  $project->name   = trim($values['name']);
  $project->color  = $values['color'];
  $project->active = (int) $values['active'];

  // Parse cid from autocomplete value.
  $raw = trim($values['cid_autocomplete']);
  if ($raw && preg_match('/\[cid:(\d+)\]/', $raw, $m)) {
    $project->cid = (int) $m[1];
  }
  else {
    $project->cid = NULL;
  }

  $project->save();

  backdrop_set_message(t('Project %name saved.', array('%name' => $project->name)));
  $form_state['redirect'] = 'drop/crm/projects';
}

/**
 * Form: confirm deletion of a project.
 */
function dropcrm_project_delete_form($form, &$form_state, DropProject $project) {
  $form['#project'] = $project;
  return confirm_form(
    $form,
    t('Delete the project %name?', array('%name' => $project->name)),
    'drop/crm/projects',
    t('All time entries for this project will remain but will show as unknown project. This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for dropcrm_project_delete_form().
 */
function dropcrm_project_delete_form_submit($form, &$form_state) {
  $project = $form['#project'];
  $project->delete();
  backdrop_set_message(t('Project %name deleted.', array('%name' => $project->name)));
  $form_state['redirect'] = 'drop/crm/projects';
}

// ---------------------------------------------------------------------------
// Clients report
// ---------------------------------------------------------------------------

/**
 * Page callback: Clients report.
 *
 * Lists contacts tagged with the "Client" tag. Contact names link directly
 * to the edit form with a destination back to this report so that saving
 * returns here rather than to the bundle list.
 */
function dropcrm_report_clients() {
  backdrop_set_title(t('Clients'));

  $vocab = taxonomy_vocabulary_machine_name_load('drop_tags');
  $client_tid = NULL;
  if ($vocab) {
    $terms = taxonomy_term_load_multiple(array(), array(
      'vocabulary' => $vocab->machine_name,
      'name'       => 'Client',
    ));
    if ($terms) {
      $term = reset($terms);
      $client_tid = $term->tid;
    }
  }

  if (!$client_tid) {
    return '<p class="dropcrm-empty">' . t('No "Client" tag found. Create a tag named "Client" in the Drop Tags vocabulary.') . '</p>';
  }

  $query = db_select('crm_contact', 'c');
  $query->fields('c', array('cid', 'name', 'type', 'status'));
  $query->innerJoin('field_data_field_drop_tags', 'ft',
    'ft.entity_id = c.cid AND ft.entity_type = :type',
    array(':type' => 'crm_contact'));
  $query->condition('ft.field_drop_tags_tid', $client_tid);
  $query->condition('c.status', 1);
  $query->orderBy('c.name', 'ASC');

  $results = $query->execute()->fetchAll();

  if (empty($results)) {
    return '<p class="dropcrm-empty">' . t('No active contacts tagged "Client".') . '</p>';
  }

  $cids = array_map(function($r) { return $r->cid; }, $results);

  $status_by_cid = array();
  $status_rows = db_select('field_data_field_crm_status', 's')
    ->fields('s', array('entity_id', 'field_crm_status_value'))
    ->condition('s.entity_id', $cids, 'IN')
    ->condition('s.entity_type', 'crm_contact')
    ->execute()
    ->fetchAll();
  foreach ($status_rows as $row) {
    $status_by_cid[$row->entity_id] = $row->field_crm_status_value;
  }

  $bundle_labels = array(
    'crm_person'       => t('Person'),
    'crm_organization' => t('Organization'),
    'crm_household'    => t('Household'),
  );

  $link_options = array('query' => array('destination' => 'drop/crm/reports/clients'));

  $rows = array();
  foreach ($results as $contact) {
    $type_label  = isset($bundle_labels[$contact->type]) ? $bundle_labels[$contact->type] : $contact->type;
    $status_note = !empty($status_by_cid[$contact->cid])
      ? '<span class="dropcrm-status-note">' . nl2br(check_plain($status_by_cid[$contact->cid])) . '</span>'
      : '<span class="dropcrm-status-empty">—</span>';

    $rows[] = array(
      l(check_plain($contact->name), 'drop/crm/contacts/' . $contact->cid . '/edit', $link_options),
      check_plain($type_label),
      array('data' => $status_note, 'class' => array('dropcrm-col-status')),
    );
  }

  $output  = theme('table', array(
    'header'     => array(t('Name'), t('Type'), t('Status')),
    'rows'       => $rows,
    'attributes' => array('class' => array('dropcrm-clients-table')),
    'empty'      => t('No contacts found.'),
  ));
  $output .= '<p class="dropcrm-report-count">' . format_plural(count($rows), '1 client', '@count clients') . '</p>';

  return $output;
}

// ---------------------------------------------------------------------------
// Tag Filter report (formerly "Clients")
// ---------------------------------------------------------------------------

/**
 * Page callback: Tag Filter report.
 *
 * Lists contacts tagged with client-relationship tags from the drop_tags
 * vocabulary. A tag filter lets the user switch between Client, Past Client,
 * Prospect, etc.
 */
function dropcrm_report_tag_filter() {
  backdrop_set_title(t('Tag Filter'));

  // Load all terms from drop_tags that exist.
  $vocab = taxonomy_vocabulary_machine_name_load('drop_tags');
  $all_terms = $vocab ? taxonomy_term_load_multiple(array(), array('vocabulary' => $vocab->machine_name)) : array();

  // Build a tid => name map for display.
  $term_map = array();
  foreach ($all_terms as $term) {
    $term_map[$term->tid] = $term->name;
  }

  // Active tag filter via ?tid=N.
  $filter_tid = (!empty($_GET['tid']) && is_numeric($_GET['tid'])) ? (int) $_GET['tid'] : 0;

  // Build filter tabs from all available tags.
  $tabs = '<div class="dropcrm-report-tabs">';
  $active_class = $filter_tid === 0 ? ' class="dropcrm-tab-active"' : '';
  $tabs .= '<a href="' . url('drop/crm/reports/tag-filter') . '"' . $active_class . '>' . t('All Tags') . '</a> ';
  foreach ($term_map as $tid => $name) {
    $active_class = ($filter_tid === $tid) ? ' class="dropcrm-tab-active"' : '';
    $tabs .= '<a href="' . url('drop/crm/reports/tag-filter', array('query' => array('tid' => $tid))) . '"' . $active_class . '>'
      . check_plain($name) . '</a> ';
  }
  $tabs .= '</div>';

  if (empty($all_terms)) {
    return $tabs . '<p>' . t('No tags found. Add tags to contacts using the Tags field on the contact edit form.') . '</p>';
  }

  // Query contacts that have any drop_tags term (or a specific one).
  $query = db_select('crm_contact', 'c');
  $query->fields('c', array('cid', 'name', 'type', 'status'));
  $query->innerJoin('field_data_field_drop_tags', 'ft', 'ft.entity_id = c.cid AND ft.entity_type = :type', array(':type' => 'crm_contact'));

  if ($filter_tid) {
    $query->condition('ft.field_drop_tags_tid', $filter_tid);
  }
  else {
    // All tagged contacts — use distinct to avoid duplicates from multiple tags.
    $query->distinct();
  }

  $query->condition('c.status', 1);
  $query->orderBy('c.name', 'ASC');

  $results = $query->execute()->fetchAll();

  if (empty($results)) {
    $label = $filter_tid && isset($term_map[$filter_tid]) ? $term_map[$filter_tid] : t('any tag');
    return $tabs . '<p class="dropcrm-empty">' . t('No active contacts tagged %tag.', array('%tag' => $label)) . '</p>';
  }

  // Load all contacts' tags and status notes in bulk.
  $cids = array_map(function($r) { return $r->cid; }, $results);

  $tags_by_cid = array();
  $tag_rows = db_select('field_data_field_drop_tags', 'ft')
    ->fields('ft', array('entity_id', 'field_drop_tags_tid'))
    ->condition('ft.entity_id', $cids, 'IN')
    ->condition('ft.entity_type', 'crm_contact')
    ->execute()
    ->fetchAll();
  foreach ($tag_rows as $row) {
    $tags_by_cid[$row->entity_id][] = $row->field_drop_tags_tid;
  }

  $status_by_cid = array();
  $status_rows = db_select('field_data_field_crm_status', 's')
    ->fields('s', array('entity_id', 'field_crm_status_value'))
    ->condition('s.entity_id', $cids, 'IN')
    ->condition('s.entity_type', 'crm_contact')
    ->execute()
    ->fetchAll();
  foreach ($status_rows as $row) {
    $status_by_cid[$row->entity_id] = $row->field_crm_status_value;
  }

  $bundle_labels = array(
    'crm_person'       => t('Person'),
    'crm_organization' => t('Organization'),
    'crm_household'    => t('Household'),
  );

  $rows = array();
  foreach ($results as $contact) {
    $tag_names = array();
    if (!empty($tags_by_cid[$contact->cid])) {
      foreach ($tags_by_cid[$contact->cid] as $tid) {
        if (isset($term_map[$tid])) {
          $tag_names[] = check_plain($term_map[$tid]);
        }
      }
    }
    sort($tag_names);

    $type_label   = isset($bundle_labels[$contact->type]) ? $bundle_labels[$contact->type] : $contact->type;
    $status_note  = !empty($status_by_cid[$contact->cid])
      ? '<span class="dropcrm-status-note">' . nl2br(check_plain($status_by_cid[$contact->cid])) . '</span>'
      : '<span class="dropcrm-status-empty">—</span>';

    $rows[] = array(
      l(check_plain($contact->name), 'drop/crm/contact/' . $contact->cid),
      check_plain($type_label),
      implode(', ', $tag_names),
      array('data' => $status_note, 'class' => array('dropcrm-col-status')),
    );
  }

  $output  = $tabs;
  $output .= theme('table', array(
    'header'     => array(t('Name'), t('Type'), t('Tags'), t('Status')),
    'rows'       => $rows,
    'attributes' => array('class' => array('dropcrm-clients-table')),
    'empty'      => t('No contacts found.'),
  ));
  $output .= '<p class="dropcrm-report-count">' . format_plural(count($rows), '1 contact', '@count contacts') . '</p>';

  return $output;
}

// ---------------------------------------------------------------------------
// Email & Newsletter Audit
// ---------------------------------------------------------------------------

/**
 * Form: audit all contacts for email addresses and newsletter subscriptions.
 */
function dropcrm_email_audit_form($form, &$form_state) {
  backdrop_set_title(t('Email & Newsletter Audit'));
  $form['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';
  $form['#attached']['js'][]  = backdrop_get_path('module', 'dropcrm') . '/dropcrm.email-audit.js';

  // Load all newsletter categories.
  $newsletters = db_select('taxonomy_term_data', 't')
    ->fields('t', array('tid', 'name'))
    ->condition('t.vocabulary', 'newsletter')
    ->orderBy('t.name')
    ->execute()
    ->fetchAllKeyed();

  // Load all subscribers keyed by email.
  $subscribers = db_select('simplenews_subscriber', 's')
    ->fields('s', array('snid', 'mail'))
    ->execute()
    ->fetchAllAssoc('mail');

  // Load subscriptions keyed by snid → array of tids.
  $subs_by_snid = array();
  if ($subscribers) {
    $snids = array_keys(array_column((array) $subscribers, NULL, 'snid'));
    $snids = array_map(function($s) { return $s->snid; }, $subscribers);
    $sub_rows = db_select('simplenews_subscription', 'ss')
      ->fields('ss', array('snid', 'tid', 'status'))
      ->condition('ss.snid', $snids, 'IN')
      ->execute();
    foreach ($sub_rows as $sr) {
      $subs_by_snid[$sr->snid][$sr->tid] = (int) $sr->status;
    }
  }

  // Load all contacts with their primary emails.
  $contacts = db_select('crm_contact', 'c')
    ->fields('c', array('cid', 'name', 'type'))
    ->orderBy('c.name')
    ->execute()
    ->fetchAll();

  $primary_emails = array();
  if ($contacts) {
    $cids = array_map(function($c) { return $c->cid; }, $contacts);
    $email_rows = db_select('crm_contact_method', 'cm')
      ->fields('cm', array('cid', 'value'))
      ->condition('cm.cid', $cids, 'IN')
      ->condition('cm.type', 'email')
      ->condition('cm.is_primary', 1)
      ->execute();
    foreach ($email_rows as $er) {
      $primary_emails[$er->cid] = $er->value;
    }
  }

  $token = backdrop_get_token('dropcrm_email_audit');

  // Count linked people for each organization.
  $org_contact_counts = array();
  $org_cids = array();
  foreach ($contacts as $c) {
    if ($c->type === 'crm_organization') {
      $org_cids[] = $c->cid;
    }
  }
  if ($org_cids) {
    // Count people linked via the Employer field (field_crm_employer on crm_person).
    $org_query = db_select('field_data_field_crm_employer', 'fe')
      ->fields('fe', array('field_crm_employer_target_id'))
      ->condition('fe.entity_type', 'crm_contact')
      ->condition('fe.deleted', 0)
      ->condition('fe.field_crm_employer_target_id', $org_cids, 'IN')
      ->groupBy('fe.field_crm_employer_target_id');
    $org_query->addExpression('COUNT(DISTINCT fe.entity_id)', 'cnt');
    $result = $org_query->execute();
    foreach ($result as $row) {
      $org_contact_counts[$row->field_crm_employer_target_id] = (int) $row->cnt;
    }
    // Orgs with no linked people won't appear in the result — default to 0.
    foreach ($org_cids as $ocid) {
      if (!isset($org_contact_counts[$ocid])) {
        $org_contact_counts[$ocid] = 0;
      }
    }
  }

  // Build table header.
  $nl_headers = array();
  foreach ($newsletters as $tid => $nl_name) {
    $nl_headers[] = array('data' => check_plain($nl_name), 'class' => array('audit-col-newsletter'));
  }
  $header = array_merge(
    array(
      array('data' => t('Contact'), 'class' => array('audit-col-name')),
      array('data' => t('Type'),    'class' => array('audit-col-type')),
      array('data' => t('People'),  'class' => array('audit-col-people')),
      array('data' => t('Email'),   'class' => array('audit-col-email')),
    ),
    $nl_headers
  );

  $rows = array();
  foreach ($contacts as $contact) {
    $email = isset($primary_emails[$contact->cid]) ? $primary_emails[$contact->cid] : NULL;
    $type_label = $contact->type === 'crm_person' ? t('Person') : ($contact->type === 'crm_organization' ? t('Org') : check_plain($contact->type));

    // Name cell.
    $name_cell = l(check_plain($contact->name), 'drop/crm/contact/' . $contact->cid, array('html' => TRUE));

    // People cell — only meaningful for organisations.
    if ($contact->type === 'crm_organization') {
      $count = isset($org_contact_counts[$contact->cid]) ? $org_contact_counts[$contact->cid] : 0;
      if ($count === 0) {
        $people_cell = array(
          'data'  => '<span class="audit-people-none" title="' . t('No linked contacts') . '">0 !</span>',
          'class' => array('audit-col-people', 'audit-people-missing'),
        );
      }
      else {
        $people_cell = array(
          'data'  => '<span class="audit-people-count">' . $count . '</span>',
          'class' => array('audit-col-people'),
        );
      }
    }
    else {
      $people_cell = array('data' => '—', 'class' => array('audit-col-people', 'audit-col-people-na'));
    }

    // Email cell — show address or an inline add form.
    if ($email) {
      $email_cell = '<span class="audit-email">' . check_plain($email) . '</span>';
    }
    else {
      $email_cell = '<span class="audit-no-email">'
        . '<input type="text" class="audit-email-input" placeholder="' . t('Add email') . '" data-cid="' . (int) $contact->cid . '" data-token="' . $token . '" size="22" />'
        . ' <button type="button" class="audit-email-save button button-small">' . t('Save') . '</button>'
        . '</span>';
    }

    // Newsletter cells.
    $snid = $email && isset($subscribers[$email]) ? $subscribers[$email]->snid : NULL;
    $nl_cells = array();
    foreach ($newsletters as $tid => $nl_name) {
      if (!$email) {
        $nl_cells[] = array('data' => '<span class="audit-nl-noemail" title="' . t('No email address') . '">—</span>', 'class' => array('audit-col-newsletter'));
        continue;
      }
      $subscribed = $snid && isset($subs_by_snid[$snid][$tid]) && $subs_by_snid[$snid][$tid] == 1;
      $checked    = $subscribed ? ' checked' : '';
      $nl_cells[] = array(
        'data' => '<input type="checkbox" class="audit-nl-checkbox"'
          . $checked
          . ' data-email="' . check_plain($email) . '"'
          . ' data-tid="' . (int) $tid . '"'
          . ' data-token="' . $token . '"'
          . ' title="' . ($subscribed ? t('Subscribed') : t('Not subscribed')) . '"'
          . ' />',
        'class' => array('audit-col-newsletter'),
      );
    }

    $row_cells = array_merge(
      array(
        array('data' => $name_cell,  'class' => array('audit-col-name')),
        array('data' => $type_label, 'class' => array('audit-col-type')),
        $people_cell,
        array('data' => $email_cell, 'class' => array('audit-col-email')),
      ),
      $nl_cells
    );

    $row_class = $email ? array() : array('audit-row-missing-email');
    $rows[] = array('data' => $row_cells, 'class' => $row_class);
  }

  $form['intro'] = array(
    '#markup' => '<p>' . t('All @count contacts. Contacts missing an email are highlighted. Add an email inline, or toggle newsletter subscriptions directly.',
        array('@count' => count($contacts))) . '</p>',
  );

  $form['table'] = array(
    '#markup' => theme('table', array(
      'header'     => $header,
      'rows'       => $rows,
      'empty'      => t('No contacts found.'),
      'attributes' => array('class' => array('dropcrm-audit-table')),
    )),
  );

  return $form;
}

/**
 * AJAX endpoint: add a primary email to a contact.
 * POST: cid, email, token.
 */
function dropcrm_email_audit_add_email() {
  backdrop_add_http_header('Content-Type', 'application/json');

  if (!backdrop_valid_token(isset($_POST['token']) ? $_POST['token'] : '', 'dropcrm_email_audit')) {
    print backdrop_json_encode(array('error' => 'Invalid token'));
    return;
  }
  if (!user_access('administer crm')) {
    print backdrop_json_encode(array('error' => 'Access denied'));
    return;
  }

  $cid   = isset($_POST['cid'])   ? (int) $_POST['cid']            : 0;
  $email = isset($_POST['email']) ? trim($_POST['email'])           : '';

  if (!$cid || !valid_email_address($email)) {
    print backdrop_json_encode(array('error' => 'Invalid input'));
    return;
  }

  $contact = crm_contact_load($cid);
  if (!$contact) {
    print backdrop_json_encode(array('error' => 'Contact not found'));
    return;
  }

  // Check for existing primary email.
  $existing = db_select('crm_contact_method', 'cm')
    ->fields('cm', array('cmid'))
    ->condition('cm.cid', $cid)
    ->condition('cm.type', 'email')
    ->condition('cm.is_primary', 1)
    ->range(0, 1)
    ->execute()
    ->fetchField();

  if ($existing) {
    print backdrop_json_encode(array('error' => 'Contact already has a primary email'));
    return;
  }

  db_insert('crm_contact_method')
    ->fields(array(
      'cid'        => $cid,
      'type'       => 'email',
      'label'      => '',
      'value'      => $email,
      'is_primary' => 1,
      'created'    => REQUEST_TIME,
    ))
    ->execute();

  print backdrop_json_encode(array('success' => TRUE, 'email' => $email));
}

/**
 * AJAX endpoint: subscribe or unsubscribe a contact email from a newsletter.
 * POST: email, tid, subscribe (1/0), token.
 */
function dropcrm_email_audit_toggle_newsletter() {
  backdrop_add_http_header('Content-Type', 'application/json');

  if (!backdrop_valid_token(isset($_POST['token']) ? $_POST['token'] : '', 'dropcrm_email_audit')) {
    print backdrop_json_encode(array('error' => 'Invalid token'));
    return;
  }
  if (!user_access('administer crm')) {
    print backdrop_json_encode(array('error' => 'Access denied'));
    return;
  }

  $email     = isset($_POST['email'])     ? trim($_POST['email'])      : '';
  $tid       = isset($_POST['tid'])       ? (int) $_POST['tid']        : 0;
  $subscribe = isset($_POST['subscribe']) ? (int) $_POST['subscribe']  : 0;

  if (!valid_email_address($email) || !$tid) {
    print backdrop_json_encode(array('error' => 'Invalid input'));
    return;
  }

  if ($subscribe) {
    // Use simplenews API to subscribe.
    module_load_include('inc', 'simplenews', 'includes/simplenews.subscription');
    simplenews_subscribe_user($email, $tid, FALSE, 'dropcrm_audit');
  }
  else {
    module_load_include('inc', 'simplenews', 'includes/simplenews.subscription');
    simplenews_unsubscribe_user($email, $tid, FALSE, 'dropcrm_audit');
  }

  print backdrop_json_encode(array('success' => TRUE));
}
