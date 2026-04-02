<?php
/**
 * @file
 * Admin forms for Drop Strategic.
 */

/**
 * Form: add or edit a strategic goal.
 */
function dropstrategic_goal_form($form, &$form_state, ?DropGoalStrategic $goal = NULL) {
  global $user;

  $is_edit = isset($goal->gid);
  $form['#goal'] = $goal;

  backdrop_set_title($is_edit ? t('Edit: @title', array('@title' => $goal->title)) : t('Add goal'));

  $form['#attached'] = array(
    'css' => array(backdrop_get_path('module', 'dropstrategic') . '/dropstrategic.css'),
  );

  $form['main'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dropstrategic-form-layout')),
  );

  // --- Main column ---
  $form['main']['left'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dropstrategic-form-main')),
  );

  $form['main']['left']['title'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Goal'),
    '#required'      => TRUE,
    '#default_value' => $is_edit ? $goal->title : '',
    '#placeholder'   => t('State the goal clearly and concisely…'),
    '#maxlength'     => 255,
  );

  $form['main']['left']['body'] = array(
    '#type'          => 'text_format',
    '#title'         => t('Why this matters'),
    '#default_value' => $is_edit ? $goal->body : '',
    '#format'        => $is_edit ? $goal->body_format : 'filtered_html',
    '#rows'          => 4,
  );

  // Key results.
  $existing_krs = $is_edit ? dropstrategic_key_results_load($goal->gid) : array();
  $form['main']['left']['key_results'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Key results'),
    '#description' => t('Measurable outcomes that tell you whether you achieved this goal. State them as outcomes, not tasks.'),
  );

  // Start with existing + 3 empty rows.
  $kr_count = max(count($existing_krs) + 3, 2);
  for ($i = 0; $i < $kr_count; $i++) {
    $kr = isset($existing_krs[$i]) ? $existing_krs[$i] : NULL;
    $form['main']['left']['key_results']['kr_' . $i] = array(
      '#type'       => 'container',
      '#attributes' => array('class' => array('dropstrategic-kr-row')),
    );
    $form['main']['left']['key_results']['kr_' . $i]['kr_title_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Key result @n', array('@n' => $i + 1)),
      '#title_display' => 'invisible',
      '#default_value' => $kr ? $kr->title : '',
      '#placeholder'   => t('Key result @n…', array('@n' => $i + 1)),
      '#maxlength'     => 512,
      '#attributes'    => array('class' => array('dropstrategic-kr-title')),
    );
    $form['main']['left']['key_results']['kr_' . $i]['kr_target_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Target'),
      '#title_display' => 'invisible',
      '#default_value' => $kr ? $kr->target : '',
      '#placeholder'   => t('Target (e.g. 5)'),
      '#size'          => 10,
      '#maxlength'     => 64,
      '#attributes'    => array('class' => array('dropstrategic-kr-target')),
    );
    $form['main']['left']['key_results']['kr_' . $i]['kr_current_' . $i] = array(
      '#type'          => 'textfield',
      '#title'         => t('Current'),
      '#title_display' => 'invisible',
      '#default_value' => $kr ? $kr->current_value : '',
      '#placeholder'   => t('Current (e.g. 2)'),
      '#size'          => 10,
      '#maxlength'     => 64,
      '#attributes'    => array('class' => array('dropstrategic-kr-current')),
    );
    $form['main']['left']['key_results']['kr_' . $i]['kr_done_' . $i] = array(
      '#type'          => 'checkbox',
      '#title'         => t('Done'),
      '#default_value' => $kr ? (int) $kr->done : 0,
    );
  }
  $form['main']['left']['key_results']['kr_count'] = array(
    '#type'  => 'hidden',
    '#value' => $kr_count,
  );

  // Private notes — owner only.
  $can_see_private = !$is_edit
    || user_access('administer dropstrategic')
    || (int) $goal->owner_uid === (int) $user->uid;

  if ($can_see_private) {
    $form['main']['left']['private_notes'] = array(
      '#type'          => 'text_format',
      '#title'         => t('Private notes'),
      '#description'   => t('Only visible to you.'),
      '#default_value' => $is_edit ? $goal->private_notes : '',
      '#format'        => $is_edit ? $goal->private_notes_format : 'filtered_html',
      '#rows'          => 3,
    );
  }

  // --- Sidebar ---
  $form['main']['right'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dropstrategic-form-sidebar')),
  );

  // Type.
  $form['main']['right']['type'] = array(
    '#type'          => 'select',
    '#title'         => t('Type'),
    '#options'       => dropstrategic_type_options(),
    '#default_value' => $is_edit ? $goal->type : 'team',
  );

  // Status.
  $form['main']['right']['status'] = array(
    '#type'          => 'select',
    '#title'         => t('Status'),
    '#options'       => dropstrategic_status_options(),
    '#default_value' => $is_edit ? $goal->status : 'active',
  );

  // Horizon.
  $form['main']['right']['horizon'] = array(
    '#type'          => 'select',
    '#title'         => t('Horizon'),
    '#options'       => dropstrategic_horizon_options(),
    '#default_value' => $is_edit ? $goal->horizon : 'quarterly',
  );

  // Due date.
  $form['main']['right']['due_date'] = array(
    '#type'          => 'date_popup',
    '#title'         => t('Target date'),
    '#default_value' => ($is_edit && $goal->due_date) ? format_date($goal->due_date, 'custom', 'Y-m-d') : '',
    '#date_format'   => 'Y-m-d',
    '#required'      => FALSE,
  );

  // Progress.
  $form['main']['right']['progress'] = array(
    '#type'          => 'select',
    '#title'         => t('Progress'),
    '#options'       => dropstrategic_progress_options(),
    '#default_value' => $is_edit ? $goal->progress : 0,
  );

  // Owner.
  $owner_options = dropstrategic_owner_options();
  $form['main']['right']['owner_uid'] = array(
    '#type'          => 'select',
    '#title'         => t('Owner'),
    '#options'       => $owner_options,
    '#default_value' => $is_edit ? $goal->owner_uid : $user->uid,
    '#access'        => user_access('administer dropstrategic'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Save goal') : t('Add goal'),
  );
  if ($is_edit) {
    $form['actions']['cancel'] = array(
      '#type'       => 'link',
      '#title'      => t('Cancel'),
      '#href'       => 'drop/strategic/' . $goal->gid,
      '#attributes' => array('class' => array('button', 'button-secondary')),
    );
  }

  // Store kr count for submit handler.
  $form['#kr_count'] = $kr_count;

  return $form;
}

/**
 * Submit handler for the goal form.
 */
function dropstrategic_goal_form_submit($form, &$form_state) {
  global $user;

  $values   = $form_state['values'];
  $is_edit  = isset($form['#goal']->gid);
  $goal     = $is_edit ? $form['#goal'] : new DropGoalStrategic();

  $goal->title   = trim($values['title']);
  $goal->type    = $values['type'];
  $goal->status  = $values['status'];
  $goal->horizon = $values['horizon'];
  $goal->progress = (int) $values['progress'];

  if (user_access('administer dropstrategic') && !empty($values['owner_uid'])) {
    $goal->owner_uid = (int) $values['owner_uid'];
  }
  elseif (!$is_edit) {
    $goal->owner_uid = $user->uid;
  }

  $goal->body        = $values['body']['value'];
  $goal->body_format = $values['body']['format'];

  if (isset($values['private_notes'])) {
    $goal->private_notes        = $values['private_notes']['value'];
    $goal->private_notes_format = $values['private_notes']['format'];
  }

  // Due date.
  $goal->due_date = NULL;
  if (!empty($values['due_date'])) {
    $ts = strtotime($values['due_date']);
    if ($ts) {
      $goal->due_date = $ts;
    }
  }

  $goal->save();

  // Save key results.
  $kr_count = $form['#kr_count'];
  $key_results = array();
  for ($i = 0; $i < $kr_count; $i++) {
    $key_results[$i] = array(
      'title'         => isset($values['kr_title_' . $i]) ? $values['kr_title_' . $i] : '',
      'target'        => isset($values['kr_target_' . $i]) ? $values['kr_target_' . $i] : '',
      'current_value' => isset($values['kr_current_' . $i]) ? $values['kr_current_' . $i] : '',
      'done'          => isset($values['kr_done_' . $i]) ? (int) $values['kr_done_' . $i] : 0,
    );
  }
  dropstrategic_key_results_save($goal->gid, $key_results);

  backdrop_set_message($is_edit ? t('Goal updated.') : t('Goal added.'));
  $form_state['redirect'] = 'drop/strategic/' . $goal->gid;
}

/**
 * Form: confirm goal deletion.
 */
function dropstrategic_goal_delete_form($form, &$form_state, DropGoalStrategic $goal) {
  $form['#goal'] = $goal;
  return confirm_form(
    $form,
    t('Delete the goal %title?', array('%title' => $goal->title)),
    'drop/strategic/' . $goal->gid,
    t('This will also delete all key results. This cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for goal deletion.
 */
function dropstrategic_goal_delete_form_submit($form, &$form_state) {
  $goal = $form['#goal'];
  db_delete('drop_strategic_key_result')->condition('gid', $goal->gid)->execute();
  $goal->delete();
  backdrop_set_message(t('Goal deleted.'));
  $form_state['redirect'] = 'drop/strategic';
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Returns progress percentage options in 10% increments.
 */
function dropstrategic_progress_options() {
  $opts = array();
  for ($i = 0; $i <= 100; $i += 10) {
    $opts[$i] = $i . '%';
  }
  return $opts;
}

/**
 * Returns a uid => name map of users who can own goals.
 */
function dropstrategic_owner_options() {
  $accounts = user_load_multiple(array(), array('status' => 1));
  $opts = array();
  foreach ($accounts as $account) {
    if ($account->uid > 0) {
      $opts[$account->uid] = check_plain($account->name);
    }
  }
  asort($opts);
  return $opts;
}
