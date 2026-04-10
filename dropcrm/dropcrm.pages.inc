<?php
/**
 * @file
 * Page callbacks for viewing CRM entities on the front end.
 */

/**
 * Page callback: display a single CRM contact.
 *
 * @param CrmContact $contact
 *   The contact entity to display.
 *
 * @return array
 *   A render array.
 */
function dropcrm_contact_view(CrmContact $contact) {
  backdrop_set_title(entity_label('crm_contact', $contact), PASS_THROUGH);
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('CRM'), 'drop/crm'),
    l(t('Contacts'), 'drop/crm/contacts'),
  ));

  // Get extra field display settings for this bundle.
  $display = field_extra_fields_get_display('crm_contact', $contact->bundle(), 'default');

  // Always start with a plain array so all named children render reliably.
  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  // --- Hero section (replaces separate action bar + details block) ---
  if ($contact->bundle() === 'crm_person') {
    if (!isset($display['hero']) || $display['hero']['visible']) {
      $output['hero'] = _dropcrm_contact_hero($contact);
      $output['hero']['#weight'] = isset($display['hero']['weight']) ? $display['hero']['weight'] : -90;
    }
  }
  else {
    // Non-person contacts: simple action bar + entity view.
    $actions = array();
    if (dropcrm_contact_access('edit', $contact)) {
      $bundle_labels = array(
        'crm_organization' => t('Edit organization'),
        'crm_household'    => t('Edit household'),
      );
      $edit_label = isset($bundle_labels[$contact->bundle()]) ? $bundle_labels[$contact->bundle()] : t('Edit');
      $actions[] = l($edit_label, 'drop/crm/contacts/' . $contact->cid . '/edit');
    }
    if (dropcrm_contact_access('delete', $contact)) {
      $actions[] = l(t('Delete'), 'drop/crm/contacts/' . $contact->cid . '/delete');
    }
    $output['actions'] = array('#markup' => drop_base_page_actions($actions), '#weight' => -100);
    $output['details'] = entity_view($contact, 'full');
    $output['details']['#weight'] = 0;

    // Hide warmth from the entity display (shown only on reports).
    if (isset($output['details']['crm_contact'][$contact->cid]['field_crm_warmth'])) {
      unset($output['details']['crm_contact'][$contact->cid]['field_crm_warmth']);
    }

    // Move tags to bottom and render inline.
    if (isset($output['details']['crm_contact'][$contact->cid]['field_drop_tags'])) {
      $tags_element = $output['details']['crm_contact'][$contact->cid]['field_drop_tags'];
      unset($output['details']['crm_contact'][$contact->cid]['field_drop_tags']);
      // Re-render tags as inline label: value.
      $tag_items = field_get_items('crm_contact', $contact, 'field_drop_tags');
      if ($tag_items) {
        $tag_names = array();
        foreach ($tag_items as $item) {
          if (!empty($item['tid'])) {
            $term = taxonomy_term_load($item['tid']);
            if ($term) {
              $tag_names[] = '<span class="crm-hero-tag">' . check_plain($term->name) . '</span>';
            }
          }
        }
        if ($tag_names) {
          $output['tags_bottom'] = array(
            '#markup' => '<div class="crm-tags-inline"><span class="crm-tags-label">' . t('Tags:') . '</span> ' . implode(' ', $tag_names) . '</div>',
            '#weight' => 200,
          );
        }
      }
    }
  }

  // --- Affiliated contacts (organizations only) ---
  if ($contact->bundle() === 'crm_organization') {
    $affiliated = _dropcrm_load_org_contacts($contact->cid);
    $aff_rows = array();
    foreach ($affiliated as $person) {
      $aff_rows[] = array(
        l(entity_label('crm_contact', $person), 'drop/crm/contact/' . $person->cid),
      );
    }
    $add_person_link = l(t('+ Add person'), 'drop/crm/contacts/add/crm_person',
      array('attributes' => array('class' => array('drop-section-add')),
            'query'      => array('org' => $contact->cid)));
    $output['affiliated_contacts'] = array(
      '#weight' => isset($display['affiliated_contacts']['weight']) ? $display['affiliated_contacts']['weight'] : 5,
      '#access' => !isset($display['affiliated_contacts']) || $display['affiliated_contacts']['visible'],
      '#prefix' => dropcrm_accordion_prefix(t('Affiliated People'), $add_person_link, TRUE),
      '#suffix' => dropcrm_accordion_suffix(),
      'help'    => array(
        '#markup' => '<p class="crm-section-help">' . t('People who list this organization as their employer. To add someone, edit their contact profile and set the <strong>Employer</strong> field.') . '</p>',
        '#weight' => -1,
      ),
      'table'   => array(
        '#theme'  => 'table',
        '#header' => array(t('Name')),
        '#rows'   => $aff_rows,
        '#empty'  => t('No people affiliated with this organization yet.'),
      ),
    );
  }

  // --- Field API fields (custom fields added via UI) ---
  // For person contacts, the hero renders specific fields manually.
  // Render remaining Field API fields so custom fields are visible.
  if ($contact->bundle() === 'crm_person') {
    $field_output = field_attach_view('crm_contact', $contact, 'default');
    // Remove fields already rendered in the hero to avoid duplication.
    $hero_fields = array('field_crm_rating', 'field_crm_job_title', 'field_crm_employer', 'field_crm_tags', 'field_crm_warmth', 'field_drop_tags');
    foreach ($hero_fields as $hf) {
      if (isset($field_output[$hf])) {
        unset($field_output[$hf]);
      }
    }
    if (!empty($field_output)) {
      $field_output['#weight'] = 8;
      $output['custom_fields'] = $field_output;
    }
  }

  // --- Contact details (phone, email, address) ---
  $add_links = array();
  foreach (array(
    'email'     => t('Email'),
    'telephone' => t('Phone'),
    'address'   => t('Address'),
    'url'       => t('Website/Social'),
  ) as $bundle => $label) {
    $add_links[] = l('+ ' . $label, 'drop/crm/contact-methods/add/' . $bundle,
      array('attributes' => array('class' => array('drop-section-add')),
            'query'      => array('contact' => $contact->cid)));
  }
  $add_links_html = '<span class="crm-section-add-group">' . implode(' ', $add_links) . '</span>';

  $output['contact_methods'] = array(
    '#weight' => isset($display['contact_methods']['weight']) ? $display['contact_methods']['weight'] : 10,
    '#access' => !isset($display['contact_methods']) || $display['contact_methods']['visible'],
    '#prefix' => dropcrm_accordion_prefix(t('Contact Details'), $add_links_html, TRUE),
    '#suffix' => dropcrm_accordion_suffix(),
  );
  $methods = _dropcrm_load_contact_methods($contact->cid);
  $method_rows = array();
  foreach ($methods as $type => $type_methods) {
    $type_label = _dropcrm_contact_method_bundle_label($type);
    foreach ($type_methods as $method) {
      $label_text = !empty($method->label) ? check_plain($method->label) : '';
      if ($type === 'url') {
        $value = l(check_plain($method->value), $method->value,
          array('attributes' => array('target' => '_blank', 'rel' => 'noopener')));
      }
      else {
        $value = check_plain($method->value);
      }
      $ops = '';
      if (dropcrm_contact_method_access('edit', $method)) {
        $ops .= l(t('Edit'), 'drop/crm/contact-methods/manage/' . $method->cmid . '/edit',
          array('attributes' => array('class' => array('crm-method-edit-link'))));
      }
      if (dropcrm_contact_method_access('delete', $method)) {
        if ($ops) {
          $ops .= ' &middot; ';
        }
        $ops .= l(t('Delete'), 'drop/crm/contact-methods/manage/' . $method->cmid . '/delete',
          array('query' => array('destination' => 'drop/crm/contact/' . $contact->cid)));
      }
      $method_rows[] = array(
        array('data' => '<span class="crm-method-type">' . check_plain($type_label) . '</span>', 'class' => array('crm-method-col-type')),
        array('data' => ($label_text ? '<span class="crm-method-label">' . $label_text . '</span> ' : '') . $value, 'class' => array('crm-method-col-value')),
        array('data' => $ops, 'class' => array('crm-method-col-edit')),
      );
    }
  }
  if ($method_rows) {
    $output['contact_methods']['table'] = array(
      '#theme'      => 'table',
      '#rows'       => $method_rows,
      '#attributes' => array('class' => array('crm-methods-table')),
    );
  }

  // --- Relationships ---
  // Relationships link two people (family, colleagues, etc.).
  // Organizations use "Affiliated People" (via the Employer field) instead.
  $rel_add_link = '';
  if ($contact->bundle() !== 'crm_organization') {
    $rel_add_link = l(t('+ Add relationship'), 'drop/crm/relationships/add',
      array('attributes' => array('class' => array('drop-section-add')),
            'query'      => array('contact' => $contact->cid)));
  }
  $output['relationships'] = array(
    '#weight' => isset($display['relationships']['weight']) ? $display['relationships']['weight'] : 15,
    '#access' => !isset($display['relationships']) || $display['relationships']['visible'],
    '#prefix' => dropcrm_accordion_prefix(t('Relationships'), $rel_add_link),
    '#suffix' => dropcrm_accordion_suffix(),
    'help'    => array(
      '#markup' => '<p class="crm-section-help">' . t('Relationships connect two people — family members, colleagues, and so on. To link a person to an organization, use the <strong>Employer</strong> field on the person\'s profile.') . '</p>',
      '#weight' => -1,
    ),
  );
  $relationships  = _dropcrm_load_contact_relationships($contact->cid);
  $type_options   = _dropcrm_relationship_type_options();
  $rel_rows = array();
  foreach ($relationships as $rel) {
    $other = crm_contact_load($rel->other_cid);
    if (!$other) {
      continue;
    }
    $type_label = isset($type_options[$rel->type]) ? $type_options[$rel->type] : $rel->type;
    $ops = l(t('Edit'), 'drop/crm/relationships/' . $rel->rid . '/edit')
      . ' &middot; ' . l(t('Delete'), 'drop/crm/relationships/' . $rel->rid . '/delete');
    $rel_rows[] = array(
      l(entity_label('crm_contact', $other), 'drop/crm/contact/' . $other->cid),
      $type_label,
      check_plain($rel->notes),
      $ops,
    );
  }
  $output['relationships']['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Contact'), t('Relationship'), t('Notes'), t('Operations')),
    '#rows'   => $rel_rows,
    '#empty'  => t('No relationships recorded.'),
  );

  // --- Activities ---
  $output['activities'] = array(
    '#weight' => isset($display['activities']['weight']) ? $display['activities']['weight'] : 20,
    '#access' => !isset($display['activities']) || $display['activities']['visible'],
    '#prefix' => dropcrm_accordion_prefix(
      t('Activities'),
      l(t('+ Add activity'), 'drop/crm/activities/add',
        array('attributes' => array('class' => array('drop-section-add')),
              'query'      => array('contact' => $contact->cid))),
      TRUE
    ),
    '#suffix' => dropcrm_accordion_suffix(),
  );
  $activities = _dropcrm_load_contact_activities($contact->cid);
  $type_options   = _dropcrm_activity_type_options();
  $status_options = _dropcrm_activity_status_options();

  // Find which activities have meeting notes — single query for all IDs.
  $aids = array_keys($activities);
  $aids_with_notes = array();
  if ($aids) {
    $notes_result = db_select('field_data_field_crm_meeting_notes', 'n')
      ->fields('n', array('entity_id'))
      ->condition('entity_id', $aids, 'IN')
      ->condition('field_crm_meeting_notes_value', '', '<>')
      ->execute();
    foreach ($notes_result as $row) {
      $aids_with_notes[$row->entity_id] = TRUE;
    }
  }

  $rows = array();
  foreach ($activities as $activity) {
    $subject_cell = l(check_plain($activity->subject), 'drop/crm/activity/' . $activity->aid);
    if (!empty($activity->is_outreach)) {
      $subject_cell .= ' <span class="crm-outreach-badge">' . t('outreach') . '</span>';
    }
    if (!empty($aids_with_notes[$activity->aid])) {
      $subject_cell .= ' <span class="crm-notes-badge" title="' . t('Has meeting notes') . '">'
        . icon('note', array('attributes' => array('width' => 14, 'height' => 14))) . '</span>';
    }
    $rows[] = array(
      'data'  => array(
        array('data' => $subject_cell),
        isset($type_options[$activity->type]) ? $type_options[$activity->type] : $activity->type,
        isset($status_options[$activity->status]) ? $status_options[$activity->status] : $activity->status,
        format_date($activity->activity_date, 'custom', 'M j, Y'),
      ),
      'class' => $activity->status === 'planned' ? array('crm-activity-planned') : array(),
    );
  }
  $output['activities']['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Subject'), t('Type'), t('Status'), t('Date')),
    '#rows'   => $rows,
    '#empty'  => t('No activities recorded.'),
  );

  // Allow other modules (e.g. dropbridge) to append sections.
  backdrop_alter('dropcrm_contact_view', $output, $contact);

  return $output;
}

/**
 * Page callback: display a single CRM activity.
 *
 * @param CrmActivity $activity
 *   The activity entity to display.
 *
 * @return array
 *   A render array.
 */
function dropcrm_activity_view(CrmActivity $activity) {
  backdrop_set_title(entity_label('crm_activity', $activity), PASS_THROUGH);
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('CRM'), 'drop/crm'),
    l(t('Activities'), 'drop/crm/activities'),
  ));

  $aid            = $activity->aid;
  $type_options   = _dropcrm_activity_type_options();
  $status_options = _dropcrm_activity_status_options();
  $priority_options = _dropcrm_activity_priority_options();
  $direction_options = _dropcrm_activity_direction_options();

  $rows = array();

  if (!empty($activity->type)) {
    $rows[] = array(t('Type'), isset($type_options[$activity->type]) ? $type_options[$activity->type] : $activity->type);
  }
  if (!empty($activity->status)) {
    $rows[] = array(t('Status'), isset($status_options[$activity->status]) ? $status_options[$activity->status] : $activity->status);
  }
  if (isset($activity->priority)) {
    $p_class = 'drop-priority-' . drop_base_priority_class($activity->priority);
    $rows[] = array(
      t('Priority'),
      array(
        'data'  => isset($priority_options[$activity->priority]) ? $priority_options[$activity->priority] : '',
        'class' => array($p_class),
      ),
    );
  }
  if (!empty($activity->direction)) {
    $rows[] = array(t('Direction'), isset($direction_options[$activity->direction]) ? $direction_options[$activity->direction] : $activity->direction);
  }
  if (!empty($activity->activity_date)) {
    $rows[] = array(t('Activity Date'), format_date($activity->activity_date, 'custom', 'M j, Y'));
  }
  if (!empty($activity->is_outreach)) {
    $rows[] = array(t('Outreach'), t('Yes — counted toward outreach goal'));
  }
  if (!empty($activity->created)) {
    $rows[] = array(t('Created'), format_date($activity->created, 'custom', 'M j, Y'));
  }
  if (!empty($activity->due_date)) {
    $today     = strtotime('today midnight');
    $two_days  = strtotime('+2 days midnight');
    if ($activity->due_date < $today) {
      $due_class = 'crm-due-overdue';
    }
    elseif ($activity->due_date < $two_days) {
      $due_class = 'crm-due-soon';
    }
    else {
      $due_class = 'crm-due-future';
    }
    $rows[] = array(
      t('Date Due'),
      array(
        'data'  => format_date($activity->due_date, 'custom', 'M j, Y'),
        'class' => array($due_class),
      ),
    );
  }

  // Contacts linked to this activity.
  $lang = LANGUAGE_NONE;
  try {
    $wrapper = entity_metadata_wrapper('crm_activity', $activity);
    $contacts = $wrapper->field_crm_contacts->value();
    if ($contacts) {
      $links = array();
      foreach ((array) $contacts as $contact) {
        if ($contact) {
          $links[] = l(entity_label('crm_contact', $contact), 'drop/crm/contact/' . $contact->cid);
        }
      }
      if ($links) {
        $rows[] = array(t('Contacts'), implode(', ', $links));
      }
    }
  }
  catch (EntityMetadataWrapperException $e) {}

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css';

  // --- Action links bar (top of page) ---
  $activity_actions = array();
  if (dropcrm_activity_access('edit', $activity)) {
    $activity_actions[] = l(t('Edit activity'), 'drop/crm/activities/manage/' . $aid . '/edit');
  }
  if (dropcrm_activity_access('delete', $activity)) {
    $activity_actions[] = l(t('Delete'), 'drop/crm/activities/manage/' . $aid . '/delete');
  }

  // "Add follow-up task" button — pre-fills title, links contact and activity.
  if (module_exists('droptasks') && user_access('access tasks')) {
    $task_query = array(
      'title'    => 'Follow up: ' . $activity->subject,
      'activity' => $aid,
    );
    // Link to the first contact on the activity if available.
    try {
      $wrapper  = entity_metadata_wrapper('crm_activity', $activity);
      $contacts = $wrapper->field_crm_contacts->value();
      if ($contacts) {
        $first = reset($contacts);
        if ($first) {
          $task_query['contact'] = $first->cid;
        }
      }
    }
    catch (EntityMetadataWrapperException $e) {}

    $activity_actions[] = l(t('+ Add follow-up task'), 'drop/tasks/add', array(
      'query' => $task_query,
    ));
  }

  // "Log time" button — links to DropTime daily page with note pre-filled.
  if (module_exists('droptime') && user_access('access droptime')) {
    $activity_actions[] = l(t('+ Log time'), 'drop/time', array(
      'query' => array(
        'notes'  => $activity->subject,
        'return' => 'drop/crm/activity/' . $aid,
      ),
    ));
  }
  $output['actions'] = array(
    '#markup' => drop_base_page_actions($activity_actions),
    '#weight' => -100,
  );

  // --- Details section ---
  $output['details_heading'] = array(
    '#markup' => drop_base_section_header(t('Details')),
    '#weight' => 0,
  );
  $output['details'] = array(
    '#theme'      => 'table',
    '#rows'       => $rows,
    '#attributes' => array('class' => array('crm-activity-view')),
    '#weight'     => 1,
  );

  // --- Extend due date shortcuts ---
  $extend_label = !empty($activity->due_date) ? t('Extend Due Date:') : t('Set Due Date:');
  $output['extend'] = array(
    '#prefix' => '<div class="crm-extend-links"><strong>' . $extend_label . '</strong> ',
    '#markup' => l(t('+ 1 Day'),   'drop/crm/activity/' . $aid . '/extend/day')
      . ' &nbsp;·&nbsp; '
      . l(t('+ 1 Week'),  'drop/crm/activity/' . $aid . '/extend/week')
      . ' &nbsp;·&nbsp; '
      . l(t('+ 1 Month'), 'drop/crm/activity/' . $aid . '/extend/month'),
    '#suffix' => '</div>',
    '#weight' => 2,
  );

  // --- Notes section ---
  try {
    $notes = entity_metadata_wrapper('crm_activity', $activity)->field_crm_activity_body->value();
    if (!empty($notes)) {
      $output['notes_heading'] = array(
        '#markup' => drop_base_section_header(t('Notes')),
        '#weight' => 10,
      );
      $output['notes'] = array(
        '#markup' => check_markup($notes, 'plain_text'),
        '#weight' => 11,
      );
    }
  }
  catch (EntityMetadataWrapperException $e) {}

  // --- Meeting notes — displayed as a collapsed accordion ---
  $meeting_items = field_get_items('crm_activity', $activity, 'field_crm_meeting_notes');
  if ($meeting_items && !empty($meeting_items[0]['value'])) {
    $format = !empty($meeting_items[0]['format']) ? $meeting_items[0]['format'] : 'filtered_html';
    $output['meeting_notes_heading'] = array(
      '#markup' => drop_base_section_header(t('Details')),
      '#weight' => 20,
    );
    $output['meeting_notes'] = array(
      '#markup' => '<details class="crm-accordion">'
        . '<summary class="crm-accordion-summary">' . t('Show / Hide') . '</summary>'
        . '<div class="crm-accordion-body">'
        . check_markup($meeting_items[0]['value'], $format)
        . '</div>'
        . '</details>',
      '#weight' => 21,
    );
  }

  // Render any additional Field API fields not already shown above.
  $field_output = field_attach_view('crm_activity', $activity, 'default');
  $manual_fields = array('field_crm_contacts', 'field_crm_activity_body', 'field_crm_meeting_notes');
  foreach ($manual_fields as $mf) {
    if (isset($field_output[$mf])) {
      unset($field_output[$mf]);
    }
  }
  if (!empty($field_output)) {
    $field_output['#weight'] = 15;
    $output['custom_fields'] = $field_output;
  }

  return $output;
}

/**
 * Page callback: display a single CRM contact method.
 *
 * @param CrmContactMethod $method
 *   The contact method entity to display.
 *
 * @return array
 *   A render array.
 */
function dropcrm_contact_method_view(CrmContactMethod $method) {
  backdrop_set_title(entity_label('crm_contact_method', $method), PASS_THROUGH);

  $crumb = array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('CRM'), 'drop/crm'),
  );
  if (!empty($method->cid)) {
    $contact = crm_contact_load($method->cid);
    if ($contact) {
      $crumb[] = l(t('Contacts'), 'drop/crm/contacts');
      $crumb[] = l(entity_label('crm_contact', $contact), 'drop/crm/contact/' . $method->cid);
    }
  }
  backdrop_set_breadcrumb($crumb);

  return entity_view($method, 'full');
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/**
 * Returns a key => label array of relationship types from config.
 *
 * @return array
 */
function _dropcrm_relationship_type_options() {
  $types = config_get('dropcrm.settings', 'relationship_types');
  if (empty($types)) {
    // Fall back to a minimal set if config has not been populated yet.
    return array(
      'referral'     => t('Referral / Connection'),
      'spouse'       => t('Spouse / Partner'),
      'parent_child' => t('Parent / Child'),
      'sibling'      => t('Sibling'),
      'friend'       => t('Friend'),
      'colleague'    => t('Colleague'),
      'other'        => t('Other'),
    );
  }
  $options = array();
  foreach ($types as $key => $info) {
    $options[$key] = t($info['label']);
  }
  return $options;
}

/**
 * Loads all relationships for a contact, annotating each with other_cid.
 *
 * Queries both cid_a and cid_b sides so symmetric relationships always
 * appear on both contacts' pages.
 *
 * @param int $cid
 *   The contact ID to load relationships for.
 *
 * @return array
 *   Array of stdClass objects from crm_relationship, each with an added
 *   'other_cid' property set to the CID of the other contact in the pair.
 */
function _dropcrm_load_contact_relationships($cid) {
  $cid = (int) $cid;
  $result = db_select('crm_relationship', 'r')
    ->fields('r')
    ->condition(
      db_or()
        ->condition('cid_a', $cid)
        ->condition('cid_b', $cid)
    )
    ->orderBy('created', 'DESC')
    ->execute()
    ->fetchAll();

  foreach ($result as $row) {
    $row->other_cid = ($row->cid_a == $cid) ? (int) $row->cid_b : (int) $row->cid_a;
  }
  return $result;
}

/**
 * Loads all contact methods for a contact, keyed by bundle type.
 *
 * @param int $cid
 *   Contact ID.
 *
 * @return array
 *   Array of arrays, keyed by bundle type (email, telephone, address).
 */
function _dropcrm_load_contact_methods($cid) {
  $result = db_select('crm_contact_method', 'm')
    ->fields('m', array('cmid', 'type', 'label', 'value', 'is_primary'))
    ->condition('cid', $cid)
    ->orderBy('type')
    ->orderBy('is_primary', 'DESC')
    ->execute();

  $grouped = array();
  foreach ($result as $row) {
    $grouped[$row->type][] = $row;
  }
  return $grouped;
}

/**
 * Returns a human label for a contact method bundle.
 *
 * @param string $type
 *   Bundle machine name.
 *
 * @return string
 */
function _dropcrm_contact_method_bundle_label($type) {
  $labels = array(
    'email'     => t('Email'),
    'telephone' => t('Telephone'),
    'address'   => t('Address'),
    'url'       => t('Website & Social'),
  );
  return isset($labels[$type]) ? $labels[$type] : ucfirst($type);
}

/**
 * Builds the hero header block for a crm_person contact view page.
 *
 * Shows rating, job title, organizations, tags, status badge, and action links
 * in a unified card above the accordion sections.
 *
 * @param CrmContact $contact
 *   A contact entity with bundle crm_person.
 *
 * @return array
 *   A render array for the hero block.
 */
function _dropcrm_contact_hero(CrmContact $contact) {
  // Action links — modules may append via hook_dropcrm_contact_hero_actions_alter().
  $action_items = array();
  if (dropcrm_contact_access('edit', $contact)) {
    $action_items[] = l(t('Edit person'), 'drop/crm/contacts/' . $contact->cid . '/edit');
  }
  if (dropcrm_contact_access('delete', $contact)) {
    $action_items[] = l(t('Delete'), 'drop/crm/contacts/' . $contact->cid . '/delete');
  }
  backdrop_alter('dropcrm_contact_hero_actions', $action_items, $contact);
  $actions_html = '';
  if ($action_items) {
    $actions_html = '<div class="crm-hero-actions">' . implode(' ', $action_items) . '</div>';
  }

  $meta_html = '';
  try {
    $w = entity_metadata_wrapper('crm_contact', $contact);
  }
  catch (EntityMetadataWrapperException $e) {
    $w = NULL;
  }

  // Rating.
  $rating_html = '';
  $rating_items = field_get_items('crm_contact', $contact, 'field_crm_rating');
  if ($rating_items && !empty($rating_items[0]['value'])) {
    $stars = _dropcrm_render_stars((int) $rating_items[0]['value']);
    if ($stars) {
      $rating_html = '<div class="crm-hero-rating">' . $stars . '</div>';
    }
  }

  // Job title and organizations on one subtitle line.
  $subtitle_parts = array();
  if ($w) {
    try {
      $job = $w->field_crm_job_title->value();
      if ($job) {
        $subtitle_parts[] = '<span class="crm-hero-jobtitle">' . check_plain($job) . '</span>';
      }
    }
    catch (EntityMetadataWrapperException $e) {}

    try {
      $org_links = array();
      foreach ($w->field_crm_employer->getIterator() as $item) {
        $org = $item->value();
        if ($org) {
          $org_links[] = l(entity_label('crm_contact', $org), 'drop/crm/contact/' . $org->cid);
        }
      }
      if ($org_links) {
        $subtitle_parts[] = '<span class="crm-hero-orgs">' . implode(', ', $org_links) . '</span>';
      }
    }
    catch (EntityMetadataWrapperException $e) {}
  }
  $subtitle_html = '';
  if ($subtitle_parts) {
    $subtitle_html = '<div class="crm-hero-subtitle">' . implode(' &middot; ', $subtitle_parts) . '</div>';
  }

  // Tags.
  $tags_html = '';
  $tag_items = field_get_items('crm_contact', $contact, 'field_crm_tags');
  if ($tag_items) {
    $tag_links = array();
    foreach ($tag_items as $item) {
      if (!empty($item['tid'])) {
        $term = taxonomy_term_load($item['tid']);
        if ($term) {
          $tag_links[] = l(check_plain($term->name), 'drop/crm/people',
            array('query' => array('tag' => $item['tid']), 'attributes' => array('class' => array('crm-hero-tag'))));
        }
      }
    }
    if ($tag_links) {
      $tags_html = '<div class="crm-hero-tags">' . implode('', $tag_links) . '</div>';
    }
  }

  // Status badge — only when inactive.
  $status_html = '';
  if (!$contact->status) {
    $status_html = '<span class="crm-hero-status-badge">' . t('Inactive') . '</span>';
  }

  $meta_html = $rating_html . $subtitle_html . $tags_html . $status_html;

  $html = '<div class="crm-contact-hero">';
  if ($meta_html) {
    $html .= '<div class="crm-hero-meta">' . $meta_html . '</div>';
  }
  $html .= $actions_html;
  $html .= '</div>';

  return array('#markup' => $html, '#weight' => -90);
}

/**
 * Builds a custom render array for a crm_person contact's core fields.
 *
 * Combines first/middle/last name onto one line and labels the employer
 * field clearly. Avoids relying on the field display system, which renders
 * each name part as a separate unlabelled row.
 *
 * @param CrmContact $contact
 *   A contact entity with bundle crm_person.
 *
 * @return array
 *   A render array for the person's details block.
 */
function _dropcrm_person_details(CrmContact $contact) {
  try {
    $w = entity_metadata_wrapper('crm_contact', $contact);
  }
  catch (EntityMetadataWrapperException $e) {
    return array();
  }

  $items = array();

  // Rating.
  $rating_items = field_get_items('crm_contact', $contact, 'field_crm_rating');
  if ($rating_items && !empty($rating_items[0]['value'])) {
    $stars = _dropcrm_render_stars((int) $rating_items[0]['value']);
    if ($stars) {
      $items[] = array('label' => t('Rating'), 'value' => $stars);
    }
  }

  // Job title.
  $job = $w->field_crm_job_title->value();
  if ($job) {
    $items[] = array('label' => t('Job Title'), 'value' => check_plain($job));
  }

  // Affiliated organization(s) — field is unlimited cardinality.
  try {
    $org_links = array();
    foreach ($w->field_crm_employer->getIterator() as $item) {
      $org = $item->value();
      if ($org) {
        $org_links[] = l(entity_label('crm_contact', $org), 'drop/crm/contact/' . $org->cid);
      }
    }
    if ($org_links) {
      $items[] = array(
        'label'  => format_plural(count($org_links), 'Organization', 'Organizations'),
        'value'  => implode(', ', $org_links),
      );
    }
  }
  catch (EntityMetadataWrapperException $e) {}

  // Website.
  try {
    $website = $w->field_crm_website->value();
    if ($website) {
      $items[] = array('label' => t('Website'), 'value' => l(check_plain($website), $website));
    }
  }
  catch (EntityMetadataWrapperException $e) {}

  // Tags.
  $tag_items = field_get_items('crm_contact', $contact, 'field_crm_tags');
  if ($tag_items) {
    $tag_links = array();
    foreach ($tag_items as $item) {
      if (!empty($item['tid'])) {
        $term = taxonomy_term_load($item['tid']);
        if ($term) {
          $tag_links[] = l($term->name, 'drop/crm/people',
            array('query' => array('tag' => $item['tid'])));
        }
      }
    }
    if ($tag_links) {
      $items[] = array('label' => t('Tags'), 'value' => implode(', ', $tag_links));
    }
  }

  // Status — only show when inactive; active is the silent default.
  if (!$contact->status) {
    $items[] = array('label' => t('Status'), 'value' => t('Inactive'));
  }

  if (!$items) {
    return array();
  }

  $html = '<dl class="crm-contact-details">';
  foreach ($items as $item) {
    $html .= '<dt>' . check_plain($item['label']) . '</dt>';
    $html .= '<dd>' . $item['value'] . '</dd>';
  }
  $html .= '</dl>';

  return array(
    '#markup' => $html,
    '#weight' => 0,
  );
}

/**
 * Loads people whose Organization field points to a given organization.
 *
 * @param int $cid
 *   The organization's contact ID.
 *
 * @return array
 *   Array of CrmContact entities (crm_person bundle), sorted by name.
 */
function _dropcrm_load_org_contacts($cid) {
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', 'crm_contact')
    ->entityCondition('bundle', 'crm_person')
    ->fieldCondition('field_crm_employer', 'target_id', $cid)
    ->propertyOrderBy('name', 'ASC');

  $result = $query->execute();
  if (empty($result['crm_contact'])) {
    return array();
  }

  return crm_contact_load_multiple(array_keys($result['crm_contact']));
}

/**
 * Loads activities linked to a contact via the field_crm_contacts entity reference.
 *
 * @param int $cid
 *   Contact ID.
 *
 * @return array
 *   Array of CrmActivity entities, sorted by activity_date descending.
 */
function _dropcrm_load_contact_activities($cid) {
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', 'crm_activity')
    ->fieldCondition('field_crm_contacts', 'target_id', $cid)
    ->propertyOrderBy('activity_date', 'DESC');

  $result = $query->execute();
  if (empty($result['crm_activity'])) {
    return array();
  }

  return crm_activity_load_multiple(array_keys($result['crm_activity']));
}

// ---------------------------------------------------------------------------
// Organization review checklist
// ---------------------------------------------------------------------------

/**
 * Page callback: organization review checklist.
 *
 * Lists organizations with a temporary checkbox for working through a list
 * (e.g. invoicing clients). Supports tag filtering.
 */
function dropcrm_review_page() {
  global $user;

  backdrop_set_title(t('Review'));

  $base_path = backdrop_get_path('module', 'drop_base');
  $output = array(
    '#attached' => array(
      'css' => array(
        $base_path . '/drop_base.review.css',
        backdrop_get_path('module', 'dropcrm') . '/dropcrm.admin.css',
      ),
      'js' => array($base_path . '/drop_base.review.js'),
    ),
  );

  // Tag filter from URL.
  $tag_filter = isset($_GET['tag']) ? (int) $_GET['tag'] : 0;

  // Build tag dropdown from drop_tags vocabulary.
  $tag_opts_html = '<option value="0"' . (!$tag_filter ? ' selected' : '') . '>' . t('— All tags —') . '</option>';
  $vocab = taxonomy_vocabulary_machine_name_load('drop_tags');
  if ($vocab) {
    $all_terms = db_select('taxonomy_term_data', 'd')
      ->fields('d', array('tid', 'name'))
      ->condition('d.vocabulary', 'drop_tags')
      ->orderBy('d.name')
      ->execute()->fetchAllKeyed();
    foreach ($all_terms as $tid => $tname) {
      $tag_opts_html .= '<option value="' . (int) $tid . '"'
        . ($tag_filter === (int) $tid ? ' selected' : '') . '>'
        . check_plain($tname) . '</option>';
    }
  }

  $filter_form = '<form method="get" action="' . url('drop/crm/review') . '" class="dropcrm-filter-form">'
    . '<select name="tag" class="dropcrm-filter-select" onchange="this.form.submit()">' . $tag_opts_html . '</select>'
    . '</form>';

  $output['filters'] = array(
    '#markup' => '<div class="dropcrm-filter-row">' . $filter_form . '</div>',
    '#weight' => -20,
  );

  // Load active organizations.
  $query = db_select('crm_contact', 'c')
    ->fields('c', array('cid', 'name'))
    ->condition('c.type', 'crm_organization')
    ->condition('c.status', 1)
    ->orderBy('c.name', 'ASC');

  if ($tag_filter) {
    $query->innerJoin('field_data_field_drop_tags', 'ft',
      'ft.entity_id = c.cid AND ft.entity_type = :type',
      array(':type' => 'crm_contact'));
    $query->condition('ft.field_drop_tags_tid', $tag_filter);
  }

  $orgs = $query->execute()->fetchAll();

  if (empty($orgs)) {
    $output['empty'] = array(
      '#markup' => '<p class="dropcrm-empty">' . t('No organizations to review.') . '</p>',
    );
    return $output;
  }

  $reviewed_cids = drop_base_review_get('organizations', $user->uid);
  $reviewed_set  = array_flip($reviewed_cids);
  $reviewed_count = count(array_intersect($reviewed_cids, array_column($orgs, 'cid')));

  // Controls bar.
  $clear_link = $reviewed_count
    ? l(t('Clear @count reviewed flag(s)', array('@count' => $reviewed_count)),
        'drop/crm/review/clear',
        array('query' => array('token' => backdrop_get_token('crm-review-clear'))))
    : '';
  $output['controls'] = array(
    '#markup' => '<div class="drop-review-controls">'
      . '<span class="drop-review-summary">'
      . format_plural(count($orgs), '1 organization', '@count organizations')
      . ($reviewed_count ? ', ' . format_plural($reviewed_count, '1 reviewed', '@count reviewed') : '')
      . '</span>'
      . ($clear_link ? ' &nbsp;·&nbsp; ' . $clear_link : '')
      . '</div>',
    '#weight' => -10,
  );

  // Split into unreviewed (first) and reviewed (last).
  $unreviewed = array();
  $reviewed   = array();
  foreach ($orgs as $org) {
    if (isset($reviewed_set[$org->cid])) {
      $reviewed[] = $org;
    }
    else {
      $unreviewed[] = $org;
    }
  }
  $ordered = array_merge($unreviewed, $reviewed);

  $token = backdrop_get_token('crm-review-toggle');
  $rows  = array();
  foreach ($ordered as $org) {
    $is_reviewed = isset($reviewed_set[$org->cid]);
    $toggle_url = url('drop/crm/review/toggle/' . $org->cid,
      array('query' => array('token' => $token)));

    $icon_off = icon('circle',       array('attributes' => array('width' => 20, 'height' => 20)));
    $icon_on  = icon('check-circle', array('attributes' => array('width' => 20, 'height' => 20)));
    $btn_class = 'drop-review-btn' . ($is_reviewed ? ' drop-review-btn--done' : '');
    $btn_title = $is_reviewed ? t('Reviewed — click to unmark') : t('Mark as reviewed');
    $btn = '<button type="button" class="' . $btn_class . '" '
      . 'data-url="' . check_plain($toggle_url) . '" '
      . 'title="' . $btn_title . '">'
      . '<span class="drop-review-icon-off">' . $icon_off . '</span>'
      . '<span class="drop-review-icon-on">' . $icon_on . '</span>'
      . '</button>';

    $rows[] = array(
      'data' => array(
        array('data' => $btn, 'class' => array('drop-review-col-btn')),
        array('data' => l(check_plain($org->name), 'drop/crm/contact/' . $org->cid)),
      ),
      'class' => array(
        'drop-review-row',
        $is_reviewed ? 'drop-review-row--done' : '',
      ),
    );
  }

  $output['table'] = array(
    '#theme'      => 'table',
    '#header'     => array('', t('Organization')),
    '#rows'       => $rows,
    '#attributes' => array('class' => array('drop-review-table')),
    '#empty'      => t('No organizations.'),
  );

  return $output;
}

/**
 * AJAX callback: toggles the review flag for an organization.
 */
function dropcrm_review_toggle($cid) {
  global $user;

  backdrop_add_http_header('Content-Type', 'application/json');

  $token = isset($_GET['token']) ? $_GET['token'] : '';
  if (!backdrop_valid_token($token, 'crm-review-toggle')) {
    print backdrop_json_encode(array('error' => 'invalid token'));
    exit();
  }

  $cid      = (int) $cid;
  $reviewed = drop_base_review_toggle('organizations', $user->uid, $cid);
  print backdrop_json_encode(array('id' => $cid, 'reviewed' => $reviewed));
  exit();
}

/**
 * Page callback: clears all organization review flags for the current user.
 */
function dropcrm_review_clear() {
  global $user;

  if (!backdrop_valid_token($_GET['token'], 'crm-review-clear')) {
    backdrop_access_denied();
    return;
  }

  drop_base_review_clear('organizations', $user->uid);
  backdrop_set_message(t('Review flags cleared.'));
  backdrop_goto('drop/crm/review');
}
