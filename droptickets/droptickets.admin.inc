<?php
/**
 * @file
 * Admin forms for DropTickets.
 */

// ---------------------------------------------------------------------------
// Ticket form
// ---------------------------------------------------------------------------

/**
 * Form: add or edit a ticket.
 */
function droptickets_ticket_form($form, &$form_state, $ticket = NULL) {
  global $user;

  $is_edit = !empty($ticket);

  // Store the entity on the form for use in validate/submit.
  if (!$is_edit) {
    $ticket = entity_create('drop_ticket', array());
  }
  $form['#ticket'] = $ticket;

  $form['#attached']['css'][] = backdrop_get_path('module', 'droptickets') . '/droptickets.css';

  if ($is_edit) {
    backdrop_set_breadcrumb(array(
      l(t('Home'), '<front>'),
      l(t('Drop Suite'), 'drop'),
      l(t('Tickets'), 'drop/tickets'),
      l(droptickets_ticket_label($ticket) . ': ' . check_plain($ticket->title), 'drop/tickets/' . $ticket->tid),
    ));
  }
  else {
    backdrop_set_breadcrumb(array(
      l(t('Home'), '<front>'),
      l(t('Drop Suite'), 'drop'),
      l(t('Tickets'), 'drop/tickets'),
    ));
  }

  // Per-user last-used project (persists across devices via state API).
  $last_pid = state_get('droptickets_last_pid_' . $user->uid, NULL);

  // Two-column layout: main (left) and sidebar (right).
  $form['layout'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('drop-form-layout', 'droptickets-form-layout')),
    '#weight' => -20,
  );
  $form['layout']['main'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('drop-form-main', 'droptickets-form-main')),
  );
  $form['layout']['sidebar'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('drop-form-sidebar', 'droptickets-form-sidebar')),
  );

  $form['layout']['main']['title'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Title'),
    '#default_value' => $is_edit ? $ticket->title : '',
    '#required'      => TRUE,
    '#maxlength'     => 255,
    '#weight'        => -20,
  );

  $form['layout']['main']['body'] = array(
    '#type'          => 'text_format',
    '#title'         => t('Description'),
    '#default_value' => $is_edit ? $ticket->body : '',
    '#format'        => $is_edit ? $ticket->body_format : NULL,
    '#rows'          => 10,
    '#weight'        => -19,
  );

  $form['layout']['sidebar']['type'] = array(
    '#type'          => 'select',
    '#title'         => t('Type'),
    '#options'       => droptickets_type_options(),
    '#default_value' => $is_edit ? $ticket->type : 'task',
    '#required'      => TRUE,
    '#weight'        => -18,
    '#attributes'    => array('class' => array('droptickets-type-select')),
  );

  // Load all active projects with their contact (client) info.
  $project_rows = db_select('drop_project', 'p')
    ->fields('p', array('pid', 'name', 'cid'))
    ->condition('p.active', 1)
    ->orderBy('p.name', 'ASC')
    ->execute()
    ->fetchAll();

  // Build a map of cid => sorted client name.
  $cids = array_filter(array_unique(array_map(function($r) { return $r->cid; }, $project_rows)));
  $client_names = array();
  if ($cids) {
    $client_names = db_select('crm_contact', 'c')
      ->fields('c', array('cid', 'name'))
      ->condition('c.cid', $cids, 'IN')
      ->execute()
      ->fetchAllKeyed();
    asort($client_names);
  }

  // Build grouped options: client name => array(pid => project name).
  $grouped_options = array();
  $ungrouped = array();
  foreach ($project_rows as $row) {
    if (!empty($row->cid) && isset($client_names[$row->cid])) {
      $grouped_options[$client_names[$row->cid]][$row->pid] = $row->name;
    }
    else {
      $ungrouped[$row->pid] = $row->name;
    }
  }
  ksort($grouped_options);
  $pid_options = $grouped_options;
  if ($ungrouped) {
    $pid_options[t('— No client —')] = $ungrouped;
  }

  // Determine default pid: explicit edit value, URL param, or last used.
  if ($is_edit) {
    $default_pid = $ticket->pid ?: NULL;
  }
  elseif (!empty($_GET['pid'])) {
    $default_pid = (int) $_GET['pid'];
  }
  else {
    $default_pid = $last_pid;
  }

  $form['layout']['sidebar']['pid'] = array(
    '#type'          => 'select',
    '#title'         => t('Project'),
    '#options'       => $pid_options,
    '#empty_option'  => t('— Select project —'),
    '#required'      => FALSE,
    '#default_value' => $default_pid,
    '#weight'        => -17,
    '#attributes'    => array('class' => array('droptickets-project-select')),
  );

  $form['layout']['sidebar']['priority'] = array(
    '#type'          => 'select',
    '#title'         => t('Priority'),
    '#options'       => array_reverse(drop_base_priority_labels(), TRUE),
    '#default_value' => $is_edit ? $ticket->priority : 2,
    '#weight'        => -16,
  );

  $is_discussion = $is_edit && ($ticket->type === 'discussion');

  if ($is_edit) {
    if ($is_discussion) {
      $status_options = droptickets_discussion_status_options();
    }
    else {
      $status_options = droptickets_status_options();
      if (!droptickets_done_access($ticket)) {
        unset($status_options['done']);
      }
      if (!droptickets_close_access($ticket)) {
        unset($status_options['closed']);
      }
    }
    $form['layout']['sidebar']['status'] = array(
      '#type'          => 'select',
      '#title'         => t('Status'),
      '#options'       => $status_options,
      '#default_value' => $ticket->status,
      '#weight'        => -15,
    );
  }

  $team_options = droptickets_assignee_options();

  // Owner field hidden from UI — see ticket #436 on simplo.

  // Assignee options: special modes first, then individual team members.
  $assignee_options = array(
    'team'    => t('— The Team —'),
    'on_hold' => t('— On Hold —'),
  ) + $team_options;

  // Determine current assignee select value.
  if ($is_edit) {
    if ($ticket->assignee_mode === 'team') {
      $default_assignee = 'team';
    }
    elseif ($ticket->assignee_mode === 'on_hold') {
      $default_assignee = 'on_hold';
    }
    else {
      $default_assignee = $ticket->assignee_uid;
    }
  }
  else {
    $default_assignee = NULL;
  }

  $form['layout']['sidebar']['assignee_uid'] = array(
    '#type'          => 'select',
    '#title'         => $is_discussion ? t('Facilitator') : t('Assignee'),
    '#description'   => $is_discussion ? t('Who is facilitating this discussion.') : t('Who needs to act on this right now.'),
    '#options'       => $assignee_options,
    '#empty_option'  => t('— Unassigned —'),
    '#default_value' => $default_assignee,
    '#weight'        => -14,
  );

  $form['layout']['sidebar']['due_date'] = array(
    '#type'          => 'textfield',
    '#title'         => $is_discussion ? t('Decision by') : t('Due date'),
    '#default_value' => ($is_edit && $ticket->due_date) ? format_date($ticket->due_date, 'custom', 'Y-m-d') : '',
    '#size'          => 12,
    '#maxlength'     => 10,
    '#description'   => t('YYYY-MM-DD format, optional.'),
    '#attributes'    => array('class' => array('droptickets-datepicker')),
    '#weight'        => -13,
  );

  // Parent ticket autocomplete.
  $parent_default = '';
  if (!$is_edit && !empty($_GET['parent_tid']) && is_numeric($_GET['parent_tid'])) {
    $pre_parent = entity_load('drop_ticket', array((int) $_GET['parent_tid']));
    $pre_parent = $pre_parent ? reset($pre_parent) : NULL;
    if ($pre_parent) {
      $pre_proj_load = !empty($pre_parent->pid) ? entity_load('drop_project', array($pre_parent->pid)) : array();
      $pre_proj      = $pre_proj_load ? reset($pre_proj_load) : NULL;
      $pre_label  = droptickets_ticket_label($pre_parent, $pre_proj);
      $parent_default = $pre_label . ': ' . $pre_parent->title . ' [' . $pre_parent->tid . ']';
    }
  }
  if ($is_edit && !empty($ticket->parent_tid)) {
    $parent_load = entity_load('drop_ticket', array($ticket->parent_tid));
    $parent      = $parent_load ? reset($parent_load) : NULL;
    if ($parent) {
      $parent_proj_load = !empty($parent->pid) ? entity_load('drop_project', array($parent->pid)) : array();
      $parent_project   = $parent_proj_load ? reset($parent_proj_load) : NULL;
      $parent_label   = droptickets_ticket_label($parent, $parent_project);
      $parent_default = $parent_label . ': ' . $parent->title . ' [' . $parent->tid . ']';
    }
  }
  $form['layout']['sidebar']['parent_tid'] = array(
    '#type'              => 'textfield',
    '#title'             => t('Parent ticket'),
    '#default_value'     => $parent_default,
    '#description'       => t('Optional. Makes this a sub-ticket. Type a ticket ID or title to search.'),
    '#autocomplete_path' => 'drop/tickets/autocomplete',
    '#maxlength'         => 300,
    '#weight'            => -12,
  );

  // Goal link — only shown when dropstrategic is enabled.
  if (module_exists('dropstrategic')) {
    $goal_options = array(0 => t('— No goal —'));
    $goals = db_select('drop_strategic_goal', 'g')
      ->fields('g', array('gid', 'title', 'type'))
      ->condition('g.status', 'active')
      ->orderBy('g.type')
      ->orderBy('g.title')
      ->execute()->fetchAll();
    foreach ($goals as $g) {
      $type_label = $g->type === 'team' ? t('Team') : t('Personal');
      $goal_options[$g->gid] = '[' . $type_label . '] ' . $g->title;
    }
    $form['layout']['sidebar']['goal_id'] = array(
      '#type'          => 'select',
      '#title'         => t('Goal'),
      '#options'       => $goal_options,
      '#default_value' => ($is_edit && !empty($ticket->goal_id)) ? $ticket->goal_id : 0,
      '#description'   => t('Link this ticket to a strategic goal.'),
      '#weight'        => -11,
    );
  }

  $form['layout']['sidebar']['external_link'] = array(
    '#type'          => 'textfield',
    '#title'         => t('External link'),
    '#default_value' => ($is_edit && !empty($ticket->external_link)) ? $ticket->external_link : '',
    '#maxlength'     => 512,
    '#description'   => t('Optional URL, e.g. the original JIRA issue.'),
    '#weight'        => -10,
  );

  $form['#attached']['js'][] = 'core/misc/ui/jquery.ui.datepicker.min.js';
  $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.core.css';
  $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.theme.css';
  $form['#attached']['js'][]  = backdrop_get_path('module', 'droptickets') . '/droptickets.js';

  if ($is_edit) {
    $form['tid'] = array('#type' => 'hidden', '#value' => $ticket->tid);
  }

  // On create, show Slack silence option if a webhook is configured.
  if (!$is_edit && config_get('droptickets.settings', 'slack_webhook_url')) {
    $form['layout']['main']['silence_slack'] = array(
      '#type'          => 'checkbox',
      '#title'         => t('Silence Slack notification'),
      '#default_value' => 0,
      '#weight'        => 24,
    );
  }

  // On create, show watcher checkboxes pre-populated from config.
  if (!$is_edit) {
    $team            = droptickets_assignee_options();
    $saved_defaults  = config_get('droptickets.settings', 'default_watchers');
    $default_uids    = ($saved_defaults !== NULL) ? $saved_defaults : array_keys($team);
    $form['layout']['main']['watchers'] = array(
      '#type'        => 'fieldset',
      '#title'       => t('Notify'),
      '#description' => t('These people will be added as watchers and notified when the ticket is created.'),
      '#collapsible' => FALSE,
      '#weight'      => 25,
    );
    $form['layout']['main']['watchers']['watcher_uids'] = array(
      '#type'          => 'checkboxes',
      '#options'       => $team,
      '#default_value' => $default_uids,
    );
  }

  // Attach any Field API fields defined for this entity.
  field_attach_form('drop_ticket', $ticket, $form, $form_state);

  // Move non-AJAX Field API fields into the main column directly.
  // Note: field_ticket_files uses an AJAX upload widget whose callback
  // resolves the element by its original #array_parents. Moving it in an
  // after_build breaks that resolution, so we leave it at the top level and
  // use #weight to position it visually within the layout.
  $move_fields = array('field_drop_tags', 'field_drop_board');
  foreach ($move_fields as $field_name) {
    if (isset($form[$field_name])) {
      $form['layout']['main'][$field_name] = $form[$field_name];
      unset($form[$field_name]);
    }
  }
  // The file field stays at form root for AJAX compatibility but is weighted
  // to render after the main layout.
  if (isset($form['field_ticket_files'])) {
    $form['field_ticket_files']['#weight'] = -19;
  }

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Update ticket') : t('Add ticket'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), $is_edit ? 'drop/tickets/' . $ticket->tid : 'drop/tickets'),
  );

  return $form;
}

/**
 * After-build callback (legacy, no longer registered).
 *
 * Field API fields are now moved directly after field_attach_form() in the
 * form builder, except for field_ticket_files which stays at form root for
 * AJAX upload compatibility.
 */

/**
 * Validation handler for droptickets_ticket_form().
 */
function droptickets_ticket_form_validate($form, &$form_state) {
  $due = trim($form_state['values']['due_date']);
  if ($due && !preg_match('/^\d{4}-\d{2}-\d{2}$/', $due)) {
    form_set_error('due_date', t('Due date must be in YYYY-MM-DD format.'));
  }

  // Validate and resolve the parent ticket autocomplete value.
  $parent_raw = trim($form_state['values']['parent_tid']);
  if ($parent_raw !== '') {
    $parent_tid = 0;
    if (preg_match('/\[(\d+)\]$/', $parent_raw, $m)) {
      $parent_tid = (int) $m[1];
    }
    $parent_check = $parent_tid ? entity_load('drop_ticket', array($parent_tid)) : array();
    if (!$parent_tid || !$parent_check) {
      form_set_error('parent_tid', t('Parent ticket not found. Select a suggestion from the list.'));
    }
    elseif (!empty($form_state['values']['tid']) && $parent_tid === (int) $form_state['values']['tid']) {
      form_set_error('parent_tid', t('A ticket cannot be its own parent.'));
    }
    else {
      // Store resolved tid for the submit handler.
      $form_state['values']['parent_tid_resolved'] = $parent_tid;
    }
  }
  else {
    $form_state['values']['parent_tid_resolved'] = 0;
  }

  field_attach_form_validate('drop_ticket', $form['#ticket'], $form, $form_state);
}

/**
 * Submit handler for droptickets_ticket_form().
 */
function droptickets_ticket_form_submit($form, &$form_state) {
  $values = $form_state['values'];

  if (!empty($values['tid'])) {
    $tickets = entity_load('drop_ticket', array($values['tid']));
    $ticket  = $tickets ? reset($tickets) : new DropTicket();
  }
  else {
    $ticket = new DropTicket();
  }

  $ticket->pid = (int) $values['pid'];

  // Remember last-used project per user (across devices).
  if ($ticket->pid && $ticket->type !== 'discussion') {
    global $user;
    state_set('droptickets_last_pid_' . $user->uid, $ticket->pid);
  }
  $ticket->title       = trim($values['title']);
  $ticket->body        = $values['body']['value'];
  $ticket->body_format = $values['body']['format'];
  $ticket->type        = $values['type'];
  $ticket->priority    = (int) $values['priority'];
  $ticket->owner_uid   = !empty($values['owner_uid']) ? (int) $values['owner_uid'] : NULL;

  // Parse combined assignee select: 'team', 'on_hold', or a numeric UID.
  $assignee_val = $values['assignee_uid'];
  if ($assignee_val === 'team') {
    $ticket->assignee_uid  = NULL;
    $ticket->assignee_mode = 'team';
  }
  elseif ($assignee_val === 'on_hold') {
    $ticket->assignee_uid  = NULL;
    $ticket->assignee_mode = 'on_hold';
  }
  else {
    $ticket->assignee_uid  = !empty($assignee_val) ? (int) $assignee_val : NULL;
    $ticket->assignee_mode = 'user';
  }

  $due = trim($values['due_date']);
  $ticket->due_date = $due ? strtotime($due . ' 00:00:00') : NULL;

  $external_link = trim($values['external_link']);
  $ticket->external_link = $external_link !== '' ? $external_link : NULL;

  $ticket->parent_tid = isset($values['parent_tid_resolved']) ? (int) $values['parent_tid_resolved'] : 0;
  $ticket->goal_id    = isset($values['goal_id']) ? (int) $values['goal_id'] : 0;

  if (isset($values['status'])) {
    $ticket->status = $values['status'];
  }

  $is_new = empty($values['tid']);

  // New discussion tickets: set disc_open status and a 7-day default decision date.
  if ($is_new && $ticket->type === 'discussion') {
    $ticket->status = 'disc_open';
    if (empty($ticket->due_date)) {
      $ticket->due_date = strtotime('+7 days 00:00:00');
    }
  }

  field_attach_submit('drop_ticket', $ticket, $form, $form_state);

  // Store the silence flag so hook_entity_insert() can read it.
  $silence = &drupal_static('droptickets_silence_slack', FALSE);
  $silence = !empty($values['silence_slack']);

  $ticket->save();

  // On create: save watchers and notify them.
  if ($is_new && isset($values['watcher_uids'])) {
    global $user;
    $selected = array_keys(array_filter($values['watcher_uids']));
    foreach ($selected as $uid) {
      db_merge('drop_ticket_watcher')
        ->key(array('tid' => $ticket->tid, 'uid' => (int) $uid))
        ->fields(array('tid' => $ticket->tid, 'uid' => (int) $uid))
        ->execute();
      if ($uid != $user->uid) {
        droptickets_notify_create((int) $uid, $ticket->tid, NULL, 'created', $user->uid);
      }
    }
  }

  backdrop_set_message(t('Ticket %label saved.', array('%label' => droptickets_ticket_label($ticket))));
  $form_state['redirect'] = 'drop/tickets/' . $ticket->tid;
}

// ---------------------------------------------------------------------------
// Ticket delete form
// ---------------------------------------------------------------------------

/**
 * Form: confirm deletion of a ticket.
 */
function droptickets_ticket_delete_form($form, &$form_state, DropTicket $ticket) {
  $form['#ticket'] = $ticket;
  return confirm_form(
    $form,
    t('Delete ticket %label?', array('%label' => droptickets_ticket_label($ticket))),
    'drop/tickets/' . $ticket->tid,
    t('All comments on this ticket will also be deleted. This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for droptickets_ticket_delete_form().
 */
function droptickets_ticket_delete_form_submit($form, &$form_state) {
  $ticket = $form['#ticket'];

  // Delete associated comments.
  $tcids = db_select('drop_ticket_comment', 'c')
    ->fields('c', array('tcid'))
    ->condition('c.tid', $ticket->tid)
    ->execute()
    ->fetchCol();
  if ($tcids) {
    entity_delete_multiple('drop_ticket_comment', $tcids);
  }

  // Delete associated relationships.
  db_delete('drop_ticket_relationship')
    ->condition(db_or()
      ->condition('tid_a', $ticket->tid)
      ->condition('tid_b', $ticket->tid)
    )
    ->execute();

  $ticket->delete();
  backdrop_set_message(t('Ticket deleted.'));
  $form_state['redirect'] = 'drop/tickets';
}

// ---------------------------------------------------------------------------
// Relate / unrelate forms
// ---------------------------------------------------------------------------

/**
 * Form: add a relationship between two tickets.
 */
function droptickets_relate_form($form, &$form_state, DropTicket $ticket) {
  $form['#ticket'] = $ticket;

  $form['ticket_ref'] = array(
    '#type'              => 'textfield',
    '#title'             => t('Ticket'),
    '#autocomplete_path' => 'drop/tickets/autocomplete',
    '#required'          => TRUE,
    '#description'       => t('Start typing a ticket number or title.'),
    '#size'              => 50,
  );

  $form['type'] = array(
    '#type'          => 'select',
    '#title'         => t('Relationship'),
    '#options'       => droptickets_relationship_type_options(),
    '#default_value' => 'relates_to',
    '#required'      => TRUE,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Add relation'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'drop/tickets/' . $ticket->tid),
  );

  return $form;
}

/**
 * Validation handler for droptickets_relate_form().
 */
function droptickets_relate_form_validate($form, &$form_state) {
  $ticket = $form['#ticket'];
  $ref    = trim($form_state['values']['ticket_ref']);

  // Parse tid from "PREFIX-N: Title [tid]" format.
  if (!preg_match('/\[(\d+)\]$/', $ref, $m)) {
    form_set_error('ticket_ref', t('Please select a ticket from the autocomplete list.'));
    return;
  }

  $tid_b = (int) $m[1];

  if ($tid_b === (int) $ticket->tid) {
    form_set_error('ticket_ref', t('A ticket cannot be related to itself.'));
    return;
  }

  // Check the related ticket exists.
  $related = entity_load('drop_ticket', array($tid_b));
  if (!$related) {
    form_set_error('ticket_ref', t('Ticket not found.'));
    return;
  }

  // Check the relationship doesn't already exist in either direction.
  $exists = db_select('drop_ticket_relationship', 'r')
    ->fields('r', array('trid'))
    ->condition(
      db_or()
        ->condition(db_and()
          ->condition('r.tid_a', $ticket->tid)
          ->condition('r.tid_b', $tid_b))
        ->condition(db_and()
          ->condition('r.tid_a', $tid_b)
          ->condition('r.tid_b', $ticket->tid))
    )
    ->execute()
    ->fetchField();

  if ($exists) {
    form_set_error('ticket_ref', t('These tickets are already related.'));
    return;
  }

  $form_state['values']['tid_b'] = $tid_b;
}

/**
 * Submit handler for droptickets_relate_form().
 */
function droptickets_relate_form_submit($form, &$form_state) {
  $ticket = $form['#ticket'];
  $tid_b  = (int) $form_state['values']['tid_b'];
  $type   = $form_state['values']['type'];

  db_insert('drop_ticket_relationship')
    ->fields(array(
      'tid_a' => $ticket->tid,
      'tid_b' => $tid_b,
      'type'  => $type,
    ))
    ->execute();

  backdrop_set_message(t('Relation added.'));
  $form_state['redirect'] = 'drop/tickets/' . $ticket->tid;
}

/**
 * Form: confirm removal of a ticket relationship.
 */
function droptickets_unrelate_confirm_form($form, &$form_state, DropTicket $ticket, $trid) {
  $trid = (int) $trid;
  $row  = db_select('drop_ticket_relationship', 'r')
    ->fields('r')
    ->condition('r.trid', $trid)
    ->execute()
    ->fetchObject();

  if (!$row || ($row->tid_a != $ticket->tid && $row->tid_b != $ticket->tid)) {
    backdrop_set_message(t('Relationship not found.'), 'error');
    backdrop_goto('drop/tickets/' . $ticket->tid);
  }

  $form['#ticket'] = $ticket;
  $form['#trid']   = $trid;

  return confirm_form(
    $form,
    t('Remove this relationship?'),
    'drop/tickets/' . $ticket->tid,
    t('This action cannot be undone.'),
    t('Remove'),
    t('Cancel')
  );
}

/**
 * Submit handler for droptickets_unrelate_confirm_form().
 */
function droptickets_unrelate_confirm_form_submit($form, &$form_state) {
  $ticket = $form['#ticket'];
  db_delete('drop_ticket_relationship')
    ->condition('trid', $form['#trid'])
    ->execute();
  backdrop_set_message(t('Relationship removed.'));
  $form_state['redirect'] = 'drop/tickets/' . $ticket->tid;
}

// ---------------------------------------------------------------------------
// Comment form
// ---------------------------------------------------------------------------

/**
 * Form: add or edit a comment.
 */
function droptickets_comment_form($form, &$form_state, DropTicket $ticket, $comment = NULL) {
  $is_edit = !empty($comment);

  $form['#ticket'] = $ticket;

  // Plain textarea so jquery.textcomplete @mention autocomplete works.
  // Format is locked to drop_comment (mentions filter, URL linking, no TinyMCE).
  $form['body'] = array(
    '#type'          => 'textarea',
    '#title'         => $is_edit ? t('Edit comment') : t('Add comment'),
    '#default_value' => $is_edit ? $comment->body : '',
    '#rows'          => 6,
    '#required'      => TRUE,
    '#attributes'    => array('class' => array('droptickets-comment-body')),
    '#description'   => t('Type @username to mention a team member.'),
  );
  $form['body_format'] = array('#type' => 'hidden', '#value' => 'drop_comment');

  // Attach mentions autocomplete library and behaviour.
  if (module_exists('mentions')) {
    $mentions_path = backdrop_get_path('module', 'mentions');
    $form['#attached']['css'][] = $mentions_path . '/textcomplete/dist/jquery.textcomplete.css';
    $form['#attached']['js'][]  = $mentions_path . '/textcomplete/dist/jquery.textcomplete.min.js';
    $form['#attached']['js'][]  = backdrop_get_path('module', 'droptickets') . '/droptickets.mentions.js';
  }

  // Attach image paste handler.
  $form['#attached']['js'][] = backdrop_get_path('module', 'droptickets') . '/droptickets.comment-paste.js';
  $form['#attached']['js'][] = array(
    'data' => array('droptickets' => array(
      'uploadUrl'   => url('drop/tickets/upload-image'),
      'uploadToken' => backdrop_get_token('droptickets_image_upload'),
    )),
    'type' => 'setting',
  );

  if ($is_edit) {
    $form['tcid'] = array('#type' => 'hidden', '#value' => $comment->tcid);
  }
  $form['tid'] = array('#type' => 'hidden', '#value' => $ticket->tid);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Update comment') : t('Post comment'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'drop/tickets/' . $ticket->tid),
  );

  return $form;
}

/**
 * Submit handler for droptickets_comment_form().
 */
function droptickets_comment_form_submit($form, &$form_state) {
  $values = $form_state['values'];

  if (!empty($values['tcid'])) {
    $comments = entity_load('drop_ticket_comment', array($values['tcid']));
    $comment  = $comments ? reset($comments) : new DropTicketComment();
  }
  else {
    $comment = new DropTicketComment();
  }

  $comment->tid         = (int) $values['tid'];
  $comment->body        = $values['body'];
  $comment->body_format = $values['body_format'];
  $comment->save();

  global $user;

  // Notify watchers of the new comment.
  droptickets_notify_watchers($comment->tid, 'commented', $user->uid, $comment->tcid);

  // Notify any @mentioned team members and add them as watchers.
  if (module_exists('mentions')) {
    $mentions = mentions_get_mentions($comment->body);
    $already_notified = array();
    foreach ($mentions as $mention) {
      $mentioned_uid = (int) $mention['user']->uid;
      if ($mentioned_uid === (int) $user->uid || in_array($mentioned_uid, $already_notified)) {
        continue;
      }
      droptickets_watcher_add($comment->tid, $mentioned_uid);
      droptickets_notify_create($mentioned_uid, $comment->tid, $comment->tcid, 'mentioned', $user->uid);
      $already_notified[] = $mentioned_uid;
    }
  }

  backdrop_set_message(t('Comment saved.'));
  $form_state['redirect'] = 'drop/tickets/' . $values['tid'];
}

// ---------------------------------------------------------------------------
// Comment delete form
// ---------------------------------------------------------------------------

/**
 * Form: confirm deletion of a comment.
 */
function droptickets_comment_delete_form($form, &$form_state, DropTicketComment $comment) {
  $form['#comment'] = $comment;
  return confirm_form(
    $form,
    t('Delete this comment?'),
    'drop/tickets/' . $comment->tid,
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for droptickets_comment_delete_form().
 */
function droptickets_comment_delete_form_submit($form, &$form_state) {
  $comment = $form['#comment'];
  $tid     = $comment->tid;
  $comment->delete();
  backdrop_set_message(t('Comment deleted.'));
  $form_state['redirect'] = 'drop/tickets/' . $tid;
}

// ---------------------------------------------------------------------------
// Watchers form
// ---------------------------------------------------------------------------

/**
 * Form: manage ticket watchers.
 */
function droptickets_watchers_form($form, &$form_state, DropTicket $ticket) {
  $form_state['ticket'] = $ticket;

  $team    = droptickets_assignee_options();
  $current = array_map('intval', droptickets_watchers_get($ticket->tid));

  $form['watcher_uids'] = array(
    '#type'          => 'checkboxes',
    '#options'       => $team,
    '#default_value' => $current,
  );

  $form['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Update'),
  );

  return $form;
}

/**
 * Submit: save watcher list.
 */
function droptickets_watchers_form_submit($form, &$form_state) {
  $ticket   = $form_state['ticket'];
  $selected = array_keys(array_filter($form_state['values']['watcher_uids']));

  db_delete('drop_ticket_watcher')
    ->condition('tid', $ticket->tid)
    ->execute();

  foreach ($selected as $uid) {
    db_insert('drop_ticket_watcher')
      ->fields(array('tid' => $ticket->tid, 'uid' => (int) $uid))
      ->execute();
  }

  $form_state['redirect'] = 'drop/tickets/' . $ticket->tid;
}

// ---------------------------------------------------------------------------
// Settings form
// ---------------------------------------------------------------------------

/**
 * Form: DropTickets settings.
 */
function droptickets_settings_form($form, &$form_state) {
  $config = config('droptickets.settings');

  $form['#config'] = 'droptickets.settings';

  $form['statuses_link'] = array(
    '#markup' => '<p>' . l(t('Manage statuses and Kanban lanes'), 'drop/tickets/settings/statuses') . '</p>',
    '#weight' => -100,
  );

  // --- Notifications ---
  $form['notifications'] = array(
    '#type' => 'fieldset',
    '#title' => t('Notifications'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['notifications']['notification_lifetime'] = array(
    '#type'          => 'select',
    '#title'         => t('Delete notifications after'),
    '#description'   => t('Choose "Never" to keep all notifications indefinitely.'),
    '#options'       => array(
      0   => t('Never'),
      7   => t('7 days'),
      30  => t('30 days'),
      60  => t('60 days'),
      90  => t('90 days'),
      365 => t('365 days'),
    ),
    '#default_value' => $config->get('notification_lifetime'),
  );
  $form['notifications']['slack_webhook_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Slack webhook URL'),
    '#description'   => t('Posts to Slack when a new ticket is created. Leave blank to disable.'),
    '#default_value' => $config->get('slack_webhook_url'),
    '#maxlength'     => 512,
    '#placeholder'   => 'https://hooks.slack.com/services/…',
  );
  $form['notifications']['reactions_enabled'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Enable emoji reactions'),
    '#description'   => t('Reactions notify the ticket author.'),
    '#default_value' => $config->get('reactions_enabled'),
  );

  // --- Automation ---
  $form['automation'] = array(
    '#type' => 'fieldset',
    '#title' => t('Automation'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );

  // Build status options from config for auto-close source/target.
  $all_statuses = droptickets_status_options();

  $form['automation']['auto_close_from'] = array(
    '#type'          => 'select',
    '#title'         => t('Auto-close: source status'),
    '#description'   => t('Tickets in this status will be auto-closed after the period below.'),
    '#options'       => array('' => t('— Disabled —')) + $all_statuses,
    '#default_value' => $config->get('auto_close_from') ?: 'done',
  );
  $form['automation']['auto_close_to'] = array(
    '#type'          => 'select',
    '#title'         => t('Auto-close: target status'),
    '#description'   => t('Status to move tickets to when auto-closed.'),
    '#options'       => $all_statuses,
    '#default_value' => $config->get('auto_close_to') ?: 'closed',
  );
  $form['automation']['auto_close_days'] = array(
    '#type'          => 'select',
    '#title'         => t('Auto-close after'),
    '#options'       => array(
      0   => t('Never'),
      7   => t('7 days'),
      14  => t('14 days'),
      30  => t('30 days'),
      60  => t('60 days'),
      90  => t('90 days'),
      180 => t('180 days'),
    ),
    '#default_value' => $config->get('auto_close_days') ?: 0,
  );
  $form['automation']['stale_days'] = array(
    '#type'          => 'select',
    '#title'         => t('Stale ticket threshold'),
    '#description'   => t('Tickets not updated within this many days appear in the Stale report.'),
    '#options'       => array(
      1  => t('1 day'),
      3  => t('3 days'),
      7  => t('7 days'),
      14 => t('14 days'),
      20 => t('20 days'),
      30 => t('30 days'),
      60 => t('60 days'),
      90 => t('90 days'),
    ),
    '#default_value' => $config->get('stale_days'),
  );

  // --- Display ---
  $form['display'] = array(
    '#type' => 'fieldset',
    '#title' => t('Display'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['display']['comment_order'] = array(
    '#type'          => 'radios',
    '#title'         => t('Comment order'),
    '#options'       => array(
      'asc'  => t('Oldest first'),
      'desc' => t('Newest first'),
    ),
    '#default_value' => $config->get('comment_order') ?: 'asc',
  );
  $form['display']['kanban_modals'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Enable Kanban card modals'),
    '#description'   => t('Click a Kanban card to open a quick-view modal with description, assignee, and priority editing. Also adds an express "Add ticket" button to each column.'),
    '#default_value' => $config->get('kanban_modals'),
  );
  $form['display']['flag_soft_limit'] = array(
    '#type'          => 'select',
    '#title'         => t('Flagged tickets soft limit'),
    '#description'   => t('Warn when flagged tickets exceed this count.'),
    '#options'       => array(
      2  => '2', 4 => '4', 6 => '6', 8 => '8',
      10 => '10', 12 => '12', 15 => '15',
    ),
    '#default_value' => $config->get('flag_soft_limit'),
  );

  // --- Team ---
  $form['team'] = array(
    '#type' => 'fieldset',
    '#title' => t('Team'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $team = droptickets_assignee_options();
  $saved_defaults = $config->get('default_watchers');
  $default_watcher_uids = ($saved_defaults !== NULL) ? $saved_defaults : array_keys($team);
  $form['team']['default_watchers'] = array(
    '#type'        => 'checkboxes',
    '#title'       => t('Default watchers on new tickets'),
    '#description' => t('Pre-selected as watchers when creating a new ticket.'),
    '#options'     => $team,
    '#default_value' => $default_watcher_uids,
  );

  // --- Import ---
  $form['import'] = array(
    '#type' => 'fieldset',
    '#title' => t('Import'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['import']['jira_base_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('JIRA base URL'),
    '#description'   => t('e.g. <code>https://yourteam.atlassian.net</code>. Used for ticket links when importing CSV.'),
    '#default_value' => $config->get('jira_base_url'),
    '#maxlength'     => 255,
  );

  // --- Review mode ---
  $form['review'] = array(
    '#type' => 'fieldset',
    '#title' => t('Review mode'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['review']['review_enabled'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Enable review mode'),
    '#description'   => t('Adds a Review tab for working through tickets with a temporary checklist.'),
    '#default_value' => $config->get('review_enabled'),
  );

  $form = system_settings_form($form);
  $form['#submit'][] = 'droptickets_settings_form_normalize_watchers';
  return $form;
}

/**
 * Submit handler: normalize default_watchers checkboxes to a plain array.
 *
 * system_settings_form saves checkboxes as ['uid' => 'uid', 'uid2' => 0].
 * We store a plain indexed array of selected UIDs instead.
 */
function droptickets_settings_form_normalize_watchers($form, &$form_state) {
  if (isset($form_state['values']['default_watchers'])) {
    $form_state['values']['default_watchers'] = array_values(
      array_keys(array_filter($form_state['values']['default_watchers']))
    );
  }
}

/**
 * Legacy submit handler — kanban limits are now managed on the statuses page.
 */
function droptickets_settings_form_save_kanban_limits($form, &$form_state) {
  // No-op. Kanban limits are now stored in droptickets.statuses config.
}

// ---------------------------------------------------------------------------
// JIRA CSV import
// ---------------------------------------------------------------------------

/**
 * Form: import tickets from a JIRA CSV export.
 *
 * Also shows a history of past imports with delete links.
 */
function droptickets_import_form($form, &$form_state) {
  $all_projects = db_select('drop_project', 'p')
    ->fields('p', array('pid', 'name'))
    ->condition('p.active', 1)
    ->orderBy('p.name', 'ASC')
    ->execute()
    ->fetchAllKeyed();

  $form['pid'] = array(
    '#type'          => 'select',
    '#title'         => t('Import into project'),
    '#options'       => $all_projects,
    '#empty_option'  => t('— Select a project —'),
    '#required'      => TRUE,
    '#description'   => t('All imported tickets will be added to this project.'),
  );

  $form['csv_file'] = array(
    '#type'        => 'file',
    '#title'       => t('JIRA CSV file'),
    '#description' => t('Export from JIRA using <em>Export Excel CSV (all fields)</em>. The file must contain at minimum a <em>Summary</em> column. A <em>Description</em> column is optional.'),
  );

  $form['#attributes']['enctype'] = 'multipart/form-data';

  $form['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Import tickets'),
  );

  // Import history.
  $imports = db_select('drop_ticket_import', 'i')
    ->fields('i')
    ->orderBy('i.created', 'DESC')
    ->execute()
    ->fetchAll();

  if (!empty($imports)) {
    $pids     = array_unique(array_column((array) $imports, 'pid'));
    $projects = entity_load('drop_project', $pids);
    $uids     = array_unique(array_column((array) $imports, 'uid'));
    $accounts = $uids ? user_load_multiple($uids) : array();

    $rows = array();
    foreach ($imports as $imp) {
      $proj_name = isset($projects[$imp->pid]) ? check_plain($projects[$imp->pid]->name) : '—';
      $importer  = isset($accounts[$imp->uid]) ? check_plain($accounts[$imp->uid]->name) : '—';
      $date      = format_date($imp->created, 'custom', 'M j, Y g:i A');
      $delete    = l(t('Delete'), 'drop/tickets/import/' . $imp->import_id . '/delete');
      $rows[] = array($proj_name, $imp->ticket_count, $importer, $date, $delete);
    }

    $history = theme('table', array(
      'header' => array(t('Project'), t('Tickets'), t('Imported by'), t('Date'), t('Actions')),
      'rows'   => $rows,
    ));

    $form['history'] = array(
      '#markup' => '<h3>' . t('Import history') . '</h3>' . $history,
      '#weight' => 10,
    );
  }

  return $form;
}

/**
 * Validate handler: save the uploaded file and store it for the submit handler.
 */
function droptickets_import_form_validate($form, &$form_state) {
  $file = file_save_upload('csv_file', array(
    'file_validate_extensions' => array('csv'),
  ));
  if (!$file) {
    form_set_error('csv_file', t('Please select a CSV file to upload.'));
  }
  else {
    $form_state['storage']['import_file'] = $file;
  }
}

/**
 * Submit handler: parse CSV and create tickets with a batch import ID.
 */
function droptickets_import_form_submit($form, &$form_state) {
  global $user;

  $pid  = (int) $form_state['values']['pid'];
  $file = $form_state['storage']['import_file'];

  $path = backdrop_realpath($file->uri);
  $handle = fopen($path, 'r');
  if (!$handle) {
    backdrop_set_message(t('Could not open the uploaded file.'), 'error');
    return;
  }

  // Read header row and find column positions.
  $header = fgetcsv($handle, 0, ',', '"', '\\');
  if (empty($header)) {
    fclose($handle);
    backdrop_set_message(t('The CSV file appears to be empty.'), 'error');
    return;
  }

  // Normalize header names (trim whitespace, handle UTF-8 BOM).
  $header = array_map(function($col) {
    return trim(str_replace("\xEF\xBB\xBF", '', $col));
  }, $header);

  // Use case-insensitive column matching to handle JIRA export variations.
  $header_lower    = array_map('strtolower', $header);
  $col_summary     = array_search('summary', $header_lower);
  $col_description = array_search('description', $header_lower);
  // JIRA exports 'Issue key' or 'Issue Key' depending on version.
  $col_issue_key = array_search('issue key', $header_lower);
  if ($col_issue_key === FALSE) {
    $col_issue_key = array_search('issue id', $header_lower);
  }

  if ($col_summary === FALSE) {
    fclose($handle);
    backdrop_set_message(t('Could not find a <em>Summary</em> column in the CSV. Make sure you exported from JIRA with "all fields" or with Summary visible. Columns found: @cols', array(
      '@cols' => implode(', ', $header),
    )), 'error');
    return;
  }

  // Build JIRA links from the Issue key column if a base URL is configured.
  $jira_base_url = rtrim((string) config_get('droptickets.settings', 'jira_base_url'), '/');

  // Generate a unique batch ID for this import.
  $import_id = 'jira_' . REQUEST_TIME . '_' . substr(md5(uniqid('', TRUE)), 0, 6);

  $imported = 0;
  $skipped  = 0;

  while (($row = fgetcsv($handle, 0, ',', '"', '\\')) !== FALSE) {
    $title = isset($row[$col_summary]) ? trim($row[$col_summary]) : '';
    if ($title === '') {
      $skipped++;
      continue;
    }

    $body = ($col_description !== FALSE && isset($row[$col_description]))
      ? trim($row[$col_description])
      : '';

    // Build external link from Issue key + base URL.
    // Validate the key looks like a real JIRA key (e.g. PROJ-123) before using it.
    $external_link = NULL;
    if ($col_issue_key !== FALSE && !empty($row[$col_issue_key]) && $jira_base_url !== '') {
      $issue_key = trim($row[$col_issue_key]);
      if (preg_match('/^[A-Z][A-Z0-9]+-\d+$/', $issue_key)) {
        $external_link = $jira_base_url . '/browse/' . $issue_key;
      }
    }

    $ticket = entity_create('drop_ticket', array(
      'pid'           => $pid,
      'title'         => $title,
      'body'          => $body,
      'body_format'   => 'plain_text',
      'type'          => 'task',
      'status'        => 'open',
      'priority'      => 3,
      'uid'           => $user->uid,
      'import_id'     => $import_id,
      'external_link' => $external_link,
    ));
    $ticket->save();
    $imported++;
  }

  fclose($handle);

  if ($imported > 0) {
    // Record the import batch.
    db_insert('drop_ticket_import')
      ->fields(array(
        'import_id'    => $import_id,
        'pid'          => $pid,
        'uid'          => $user->uid,
        'ticket_count' => $imported,
        'created'      => REQUEST_TIME,
      ))
      ->execute();

    backdrop_set_message(format_plural($imported,
      'Imported 1 ticket successfully. You can manage imports on the <a href="@url">Import from JIRA</a> tab.',
      'Imported @count tickets successfully. You can manage imports on the <a href="@url">Import from JIRA</a> tab.',
      array('@url' => url('drop/tickets/import'))
    ));

    $form_state['redirect'] = 'drop/tickets';
  }
  if ($skipped > 0) {
    backdrop_set_message(format_plural($skipped,
      'Skipped 1 row with an empty title.',
      'Skipped @count rows with empty titles.'
    ), 'warning');
  }
}

/**
 * Form: confirm deletion of an import batch.
 */
function droptickets_import_delete_form($form, &$form_state, $import_id) {
  $import = db_select('drop_ticket_import', 'i')
    ->fields('i')
    ->condition('i.import_id', $import_id)
    ->execute()
    ->fetchObject();

  if (!$import) {
    backdrop_set_message(t('Import batch not found.'), 'error');
    backdrop_goto('drop/tickets/import');
  }

  $loaded  = entity_load('drop_project', array($import->pid));
  $project = $loaded ? reset($loaded) : NULL;
  $proj_name = $project ? check_plain($project->name) : t('unknown project');
  $date      = format_date($import->created, 'custom', 'M j, Y g:i A');

  $form['import_id'] = array('#type' => 'value', '#value' => $import_id);

  return confirm_form(
    $form,
    t('Delete @count imported tickets?', array('@count' => $import->ticket_count)),
    'drop/tickets/import',
    t('This will permanently delete the @count tickets imported into %project on @date. This cannot be undone.', array(
      '@count'   => $import->ticket_count,
      '%project' => $proj_name,
      '@date'    => $date,
    )),
    t('Delete all @count tickets', array('@count' => $import->ticket_count)),
    t('Cancel')
  );
}

/**
 * Submit handler: delete all tickets from an import batch.
 */
function droptickets_import_delete_form_submit($form, &$form_state) {
  $import_id = $form_state['values']['import_id'];

  // Load ticket IDs to delete (entity delete handles watchers/notifications cleanup).
  $tids = db_select('drop_ticket', 't')
    ->fields('t', array('tid'))
    ->condition('t.import_id', $import_id)
    ->execute()
    ->fetchCol();

  if (!empty($tids)) {
    entity_delete_multiple('drop_ticket', $tids);
  }

  db_delete('drop_ticket_import')
    ->condition('import_id', $import_id)
    ->execute();

  backdrop_set_message(format_plural(count($tids),
    'Deleted 1 imported ticket.',
    'Deleted @count imported tickets.'
  ));

  $form_state['redirect'] = 'drop/tickets/import';
}

// -------------------------------------------------------------------------
// Status management.
// -------------------------------------------------------------------------

/**
 * Form for managing ticket statuses and kanban lanes.
 */
function droptickets_statuses_form($form, &$form_state) {
  $form['#theme'] = 'droptickets_statuses_form';

  $statuses = _droptickets_load_statuses();

  // Sort by weight for display.
  uasort($statuses, function($a, $b) {
    return (isset($a['weight']) ? $a['weight'] : 0) - (isset($b['weight']) ? $b['weight'] : 0);
  });

  $form['#tree'] = TRUE;

  $form['description'] = array(
    '#markup' => '<p>' . t('Configure ticket statuses and which ones appear as Kanban board lanes. Statuses with the "Active" group appear in the default ticket list. "Inactive" statuses (Icebox, Closed) are hidden from default views.') . '</p>',
  );

  $group_options = array(
    'active' => t('Active'),
    'inactive' => t('Inactive'),
  );

  $form['statuses'] = array(
    '#type' => 'container',
  );

  // Count tickets per status for delete warnings.
  $ticket_counts = db_select('drop_ticket', 't')
    ->fields('t', array('status'))
    ->groupBy('t.status')
    ->execute()
    ->fetchAllKeyed(0, 0);
  $ticket_counts_real = array();
  $result = db_query('SELECT status, COUNT(*) AS cnt FROM {drop_ticket} GROUP BY status');
  foreach ($result as $row) {
    $ticket_counts_real[$row->status] = (int) $row->cnt;
  }

  $weight = 0;
  foreach ($statuses as $key => $info) {
    $count = isset($ticket_counts_real[$key]) ? $ticket_counts_real[$key] : 0;
    $form['statuses'][$key] = array(
      '#type' => 'container',
    );
    $form['statuses'][$key]['key'] = array(
      '#markup' => '<code>' . check_plain($key) . '</code>',
    );
    $form['statuses'][$key]['delete'] = array(
      '#markup' => l(t('delete'), 'drop/tickets/settings/statuses/' . $key . '/delete',
        array('attributes' => array('class' => array('droptickets-status-delete')))),
    );
    $form['statuses'][$key]['label'] = array(
      '#type' => 'textfield',
      '#default_value' => isset($info['label']) ? $info['label'] : $key,
      '#size' => 20,
      '#maxlength' => 64,
      '#required' => TRUE,
    );
    $form['statuses'][$key]['group'] = array(
      '#type' => 'select',
      '#options' => $group_options,
      '#default_value' => isset($info['group']) ? $info['group'] : 'active',
    );
    $form['statuses'][$key]['kanban'] = array(
      '#type' => 'checkbox',
      '#default_value' => !empty($info['kanban']),
    );
    $form['statuses'][$key]['kanban_limit'] = array(
      '#type' => 'number',
      '#default_value' => isset($info['kanban_limit']) ? $info['kanban_limit'] : 0,
      '#size' => 4,
      '#min' => 0,
      '#max' => 100,
    );
    $form['statuses'][$key]['weight'] = array(
      '#type' => 'number',
      '#default_value' => isset($info['weight']) ? $info['weight'] : $weight,
      '#size' => 4,
      '#min' => -50,
      '#max' => 50,
    );
    $weight++;
  }

  // New status fields — rendered inline.
  $form['new_status'] = array(
    '#type' => 'fieldset',
    '#title' => t('Add new status'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#attributes' => array('class' => array('droptickets-new-status')),
  );
  $form['new_status']['key'] = array(
    '#type' => 'textfield',
    '#title' => t('Machine name'),
    '#required' => FALSE,
    '#size' => 15,
    '#maxlength' => 32,
    '#attributes' => array('placeholder' => t('e.g. waiting_on_client')),
    '#element_validate' => array('_droptickets_validate_status_key'),
  );
  $form['new_status']['label'] = array(
    '#type' => 'textfield',
    '#title' => t('Label'),
    '#size' => 15,
    '#maxlength' => 64,
  );
  $form['new_status']['group'] = array(
    '#type' => 'select',
    '#title' => t('Group'),
    '#options' => $group_options,
    '#default_value' => 'active',
  );
  $form['new_status']['kanban'] = array(
    '#type' => 'checkbox',
    '#title' => t('Kanban'),
  );
  $form['new_status']['kanban_limit'] = array(
    '#type' => 'number',
    '#title' => t('Limit'),
    '#default_value' => 0,
    '#min' => 0,
    '#max' => 100,
    '#size' => 3,
  );
  $form['new_status']['weight'] = array(
    '#type' => 'number',
    '#title' => t('Weight'),
    '#default_value' => count($statuses),
    '#size' => 3,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save statuses'),
  );

  return $form;
}

/**
 * Element validate callback for status machine name.
 */
function _droptickets_validate_status_key($element, &$form_state) {
  $value = $element['#value'];
  if (empty($value)) {
    return;
  }
  if (!preg_match('/^[a-z][a-z0-9_]*$/', $value)) {
    form_error($element, t('Machine name must start with a lowercase letter and contain only lowercase letters, numbers, and underscores.'));
    return;
  }
  $config = config('droptickets.statuses');
  $statuses = $config->get('statuses');
  if (isset($statuses[$value])) {
    form_error($element, t('A status with the machine name %key already exists.', array('%key' => $value)));
  }
}

/**
 * Submit handler for statuses form.
 */
function droptickets_statuses_form_submit($form, &$form_state) {
  $config = config('droptickets.statuses');
  $values = $form_state['values'];

  // Update existing statuses.
  $statuses = array();
  if (!empty($values['statuses'])) {
    foreach ($values['statuses'] as $key => $data) {
      $statuses[$key] = array(
        'label' => $data['label'],
        'group' => $data['group'],
        'kanban' => (bool) $data['kanban'],
        'kanban_limit' => (int) $data['kanban_limit'],
        'weight' => (int) $data['weight'],
      );
    }
  }

  // Add new status if provided.
  if (!empty($values['new_status']['key']) && !empty($values['new_status']['label'])) {
    $new_key = $values['new_status']['key'];
    $statuses[$new_key] = array(
      'label' => $values['new_status']['label'],
      'group' => $values['new_status']['group'],
      'kanban' => (bool) $values['new_status']['kanban'],
      'kanban_limit' => (int) $values['new_status']['kanban_limit'],
      'weight' => (int) $values['new_status']['weight'],
    );
  }

  $config->set('statuses', $statuses);
  $config->save();

  // Clear the static cache.
  backdrop_static_reset('_droptickets_load_statuses');

  backdrop_set_message(t('Statuses saved.'));
}

// -------------------------------------------------------------------------
// Status delete with reassignment.
// -------------------------------------------------------------------------

/**
 * Confirmation form for deleting a status.
 *
 * If tickets exist with this status, requires reassignment to another status
 * before deletion.
 */
function droptickets_status_delete_form($form, &$form_state, $status_key) {
  $config = config('droptickets.statuses');
  $statuses = $config->get('statuses');

  if (!isset($statuses[$status_key])) {
    backdrop_set_message(t('Status %key does not exist.', array('%key' => $status_key)), 'error');
    backdrop_goto('drop/tickets/settings/statuses');
    return;
  }

  $info = $statuses[$status_key];
  $form['#status_key'] = $status_key;

  // Count tickets using this status.
  $ticket_count = (int) db_select('drop_ticket', 't')
    ->condition('status', $status_key)
    ->countQuery()
    ->execute()
    ->fetchField();

  $form['info'] = array(
    '#markup' => '<p>' . t('You are about to delete the status %label (<code>@key</code>).', array(
      '%label' => $info['label'],
      '@key' => $status_key,
    )) . '</p>',
  );

  if ($ticket_count > 0) {
    $form['warning'] = array(
      '#markup' => '<p class="messages warning">' . t('@count ticket(s) currently use this status. Choose a new status to reassign them to before deleting.', array(
        '@count' => $ticket_count,
      )) . '</p>',
    );

    // Build options from remaining statuses.
    $options = array();
    foreach ($statuses as $key => $s) {
      if ($key !== $status_key) {
        $options[$key] = $s['label'];
      }
    }

    $form['reassign_to'] = array(
      '#type' => 'select',
      '#title' => t('Reassign tickets to'),
      '#options' => $options,
      '#required' => TRUE,
    );
  }

  $form['ticket_count'] = array(
    '#type' => 'value',
    '#value' => $ticket_count,
  );

  $confirm_button = $ticket_count > 0 ? t('Reassign and delete') : t('Delete');
  $confirm_description = $ticket_count === 0
    ? t('No tickets use this status. It will be removed immediately.')
    : '';

  return confirm_form(
    $form,
    t('Delete status %label?', array('%label' => $info['label'])),
    'drop/tickets/settings/statuses',
    $confirm_description,
    $confirm_button,
    t('Cancel')
  );
}

/**
 * Submit handler for status delete form.
 */
function droptickets_status_delete_form_submit($form, &$form_state) {
  $status_key = $form['#status_key'];
  $config = config('droptickets.statuses');
  $statuses = $config->get('statuses');
  $label = isset($statuses[$status_key]) ? $statuses[$status_key]['label'] : $status_key;

  // Reassign tickets if needed.
  $ticket_count = $form_state['values']['ticket_count'];
  if ($ticket_count > 0 && !empty($form_state['values']['reassign_to'])) {
    $new_status = $form_state['values']['reassign_to'];
    $updated = db_update('drop_ticket')
      ->fields(array('status' => $new_status))
      ->condition('status', $status_key)
      ->execute();

    $new_label = isset($statuses[$new_status]) ? $statuses[$new_status]['label'] : $new_status;
    backdrop_set_message(t('Reassigned @count ticket(s) from %old to %new.', array(
      '@count' => $updated,
      '%old' => $label,
      '%new' => $new_label,
    )));

    watchdog('droptickets', 'Reassigned @count tickets from status @old to @new during status deletion.', array(
      '@count' => $updated,
      '@old' => $status_key,
      '@new' => $new_status,
    ), WATCHDOG_INFO);
  }

  // Remove the status from config.
  unset($statuses[$status_key]);
  $config->set('statuses', $statuses);
  $config->save();

  backdrop_static_reset('_droptickets_load_statuses');

  backdrop_set_message(t('Status %label deleted.', array('%label' => $label)));
  watchdog('droptickets', 'Deleted ticket status @key (%label).', array(
    '@key' => $status_key,
    '%label' => $label,
  ), WATCHDOG_INFO);

  $form_state['redirect'] = 'drop/tickets/settings/statuses';
}

/**
 * Theme function for the statuses form — renders as a table.
 */
function theme_droptickets_statuses_form($variables) {
  $form = $variables['form'];

  backdrop_add_css('
    .droptickets-statuses-table td,
    .droptickets-statuses-table th { padding: 4px 8px; vertical-align: middle; }
    .droptickets-statuses-table input[type="text"],
    .droptickets-statuses-table input[type="number"] { margin: 0; padding: 4px 6px; }
    .droptickets-statuses-table select { margin: 0; padding: 2px 4px; }
    .droptickets-statuses-table input[type="checkbox"] { margin: 0; }
    .droptickets-statuses-table .form-item { margin: 0; }
    .droptickets-new-status .fieldset-wrapper { display: flex; flex-wrap: wrap; gap: 0.75rem; align-items: flex-end; }
    .droptickets-new-status .form-item { margin: 0; }
    .droptickets-new-status .form-item-new-status-kanban { align-self: center; padding-top: 1.2em; }
  ', 'inline');

  $header = array(
    t('Label'),
    t('Group'),
    t('Kanban'),
    t('Limit'),
    t('Weight'),
    '',
  );

  $rows = array();
  if (isset($form['statuses'])) {
    foreach (element_children($form['statuses']) as $key) {
      // Add tabledrag class to the weight field.
      $form['statuses'][$key]['weight']['#attributes']['class'][] = 'status-weight';

      // Render key markup but don't show it as a column — hide it.
      backdrop_render($form['statuses'][$key]['key']);

      $rows[] = array(
        'data' => array(
          backdrop_render($form['statuses'][$key]['label']),
          backdrop_render($form['statuses'][$key]['group']),
          backdrop_render($form['statuses'][$key]['kanban']),
          backdrop_render($form['statuses'][$key]['kanban_limit']),
          backdrop_render($form['statuses'][$key]['weight']),
          backdrop_render($form['statuses'][$key]['delete']),
        ),
        'class' => array('draggable'),
      );
    }
  }

  $output = theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'empty' => t('No statuses configured.'),
    'attributes' => array('id' => 'droptickets-statuses-table', 'class' => array('droptickets-statuses-table')),
  ));

  backdrop_add_tabledrag('droptickets-statuses-table', 'order', 'sibling', 'status-weight');

  // Render remaining form elements (new status fieldset, submit button).
  $output .= backdrop_render_children($form);

  return $output;
}

