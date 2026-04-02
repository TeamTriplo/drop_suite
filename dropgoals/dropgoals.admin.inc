<?php
/**
 * @file
 * Admin form callbacks for DropGoals.
 */

// ---------------------------------------------------------------------------
// Manage list
// ---------------------------------------------------------------------------

/**
 * Page callback: list all goals for the current user.
 *
 * @return array
 *   A render array.
 */
function dropgoals_goal_manage_list() {
  global $user;

  backdrop_set_title(t('Manage Goals'));

  $goals = db_select('drop_goal', 'g')
    ->fields('g')
    ->condition('g.uid', $user->uid)
    ->orderBy('g.active', 'DESC')
    ->orderBy('g.label', 'ASC')
    ->execute()
    ->fetchAll();

  $freq_labels = dropgoals_frequency_labels();
  $rows = array();
  foreach ($goals as $goal) {
    $actions = l(t('Edit'), 'drop/goals/manage/' . $goal->gid . '/edit')
      . ' &nbsp;·&nbsp; '
      . l(t('History'), 'drop/goals/manage/' . $goal->gid)
      . ' &nbsp;·&nbsp; '
      . l(t('Delete'), 'drop/goals/manage/' . $goal->gid . '/delete');

    $freq = isset($goal->frequency) ? $goal->frequency : 'daily';
    $freq_label = isset($freq_labels[$freq]) ? $freq_labels[$freq] : t('Every day');
    $day_names = array(
      '1' => t('Mon'), '2' => t('Tue'), '3' => t('Wed'), '4' => t('Thu'),
      '5' => t('Fri'), '6' => t('Sat'), '7' => t('Sun'),
    );
    if ($freq === 'weekly' && !empty($goal->days_of_week)) {
      $day_num = trim($goal->days_of_week);
      if (isset($day_names[$day_num])) {
        $freq_label .= ' (by ' . $day_names[$day_num] . ')';
      }
    }
    elseif ($freq === 'specific_days' && !empty($goal->days_of_week)) {
      $labels = array();
      foreach (explode(',', $goal->days_of_week) as $d) {
        if (isset($day_names[$d])) {
          $labels[] = $day_names[$d];
        }
      }
      $freq_label .= ' (' . implode(', ', $labels) . ')';
    }
    $rows[] = array(
      l(check_plain($goal->label), 'drop/goals/manage/' . $goal->gid),
      $freq_label,
      $goal->active ? t('Active') : t('Inactive'),
      $actions,
    );
  }

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropgoals') . '/dropgoals.css'),
    ),
  );

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Goal'), t('Frequency'), t('Status'), t('Actions')),
    '#rows'   => $rows,
    '#empty'  => t('No goals yet. <a href="!url">Add one</a>.', array('!url' => url('drop/goals/manage/add'))),
  );

  return $output;
}

// ---------------------------------------------------------------------------
// Goal form
// ---------------------------------------------------------------------------

/**
 * Form: add or edit a goal.
 *
 * @param array $form
 * @param array $form_state
 * @param DropGoal|null $goal
 *   Existing goal for edit, or NULL for add.
 *
 * @return array
 */
function dropgoals_goal_form($form, &$form_state, $goal = NULL) {
  if (empty($goal)) {
    $goal = new DropGoal();
  }
  $form_state['goal'] = $goal;

  $form['#attached']['css'][] = backdrop_get_path('module', 'dropgoals') . '/dropgoals.css';

  $form['label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Goal'),
    '#description'   => t('Describe the goal, including the target if relevant (e.g. "8,000 steps", "In bed by 10pm", "6 hours billable").'),
    '#default_value' => $goal->label,
    '#required'      => TRUE,
    '#maxlength'     => 255,
    '#autofocus'     => TRUE,
  );

  $form['description'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Notes'),
    '#default_value' => $goal->description,
    '#rows'          => 3,
    '#required'      => FALSE,
  );

  $form['frequency'] = array(
    '#type'          => 'select',
    '#title'         => t('Frequency'),
    '#options'       => dropgoals_frequency_labels(),
    '#default_value' => isset($goal->frequency) ? $goal->frequency : 'daily',
    '#description'   => t('How often you want to complete this goal. Weekly goals disappear from the dashboard once completed for that week.'),
  );

  $day_options = array(
    '1' => t('Mon'), '2' => t('Tue'), '3' => t('Wed'), '4' => t('Thu'),
    '5' => t('Fri'), '6' => t('Sat'), '7' => t('Sun'),
  );

  // Weekly: single target day (the "due by" day within the week).
  $weekly_day_default = (isset($goal->frequency) && $goal->frequency === 'weekly' && !empty($goal->days_of_week))
    ? $goal->days_of_week
    : '5'; // Default Friday.
  $form['weekly_day'] = array(
    '#type'          => 'radios',
    '#title'         => t('Target day'),
    '#options'       => $day_options,
    '#default_value' => $weekly_day_default,
    '#description'   => t('The goal appears all week and is due by this day. You can check it off on any earlier day.'),
    '#states'        => array(
      'visible' => array(
        ':input[name="frequency"]' => array('value' => 'weekly'),
      ),
    ),
    '#prefix'        => '<div class="dropgoals-days-wrapper">',
    '#suffix'        => '</div>',
  );

  // Specific days: one or more days per week.
  $default_days = (isset($goal->frequency) && $goal->frequency === 'specific_days' && !empty($goal->days_of_week))
    ? explode(',', $goal->days_of_week)
    : array();
  $form['days_of_week'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Active days'),
    '#options'       => $day_options,
    '#default_value' => $default_days,
    '#description'   => t('Select which days of the week this goal applies to.'),
    '#states'        => array(
      'visible' => array(
        ':input[name="frequency"]' => array('value' => 'specific_days'),
      ),
    ),
    '#prefix'        => '<div class="dropgoals-days-wrapper">',
    '#suffix'        => '</div>',
  );

  $form['active'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Show on daily dashboard'),
    '#default_value' => isset($goal->gid) ? $goal->active : 1,
  );

  // Dev-only: allow backdating the creation date for testing and historical entry.
  $created_ts = !empty($goal->created) ? $goal->created : REQUEST_TIME;
  $form['dev'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Developer settings'),
    '#collapsible' => TRUE,
    '#collapsed'   => TRUE,
  );
  $form['dev']['created_date'] = array(
    '#type'          => 'date',
    '#title'         => t('Goal start date'),
    '#default_value' => array(
      'year'  => (int) date('Y', $created_ts),
      'month' => (int) date('n', $created_ts),
      'day'   => (int) date('j', $created_ts),
    ),
    '#description'   => t('Sets the creation date. Used for streak and completion rate calculations. Useful for backdating or testing.'),
  );

  $is_new = empty($goal->gid);

  // Owner — admin only, shown when editing an existing goal.
  if (!$is_new && user_access('administer goals')) {
    $all_users = db_query("SELECT uid, name FROM {users} WHERE uid > 0 AND status = 1 ORDER BY name")->fetchAllKeyed();
    $form['uid'] = array(
      '#type'          => 'select',
      '#title'         => t('Owner'),
      '#options'       => $all_users,
      '#default_value' => $goal->uid,
      '#description'   => t('Reassign this goal to a different person.'),
    );
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_new ? t('Add goal') : t('Save goal'),
  );
  if (!$is_new) {
    $form['actions']['cancel'] = array(
      '#type'       => 'link',
      '#title'      => t('Cancel'),
      '#href'       => 'drop/goals/manage/' . $goal->gid,
      '#attributes' => array('class' => array('button', 'button-secondary')),
    );
  }

  return $form;
}

/**
 * Submit handler for dropgoals_goal_form().
 */
function dropgoals_goal_form_submit($form, &$form_state) {
  $goal = $form_state['goal'];

  $goal->label       = $form_state['values']['label'];
  $goal->description = $form_state['values']['description'];
  $goal->frequency   = $form_state['values']['frequency'];
  $goal->active      = (int) $form_state['values']['active'];

  if ($goal->frequency === 'weekly') {
    $goal->days_of_week = $form_state['values']['weekly_day'];
  }
  elseif ($goal->frequency === 'specific_days') {
    $selected = array_filter($form_state['values']['days_of_week']);
    $goal->days_of_week = implode(',', array_keys($selected));
  }
  else {
    $goal->days_of_week = '';
  }

  // Dev: override the created timestamp from the date array.
  $cd = $form_state['values']['created_date'];
  if (!empty($cd) && is_array($cd)) {
    $date_str = sprintf('%04d-%02d-%02d', $cd['year'], $cd['month'], $cd['day']);
    $goal->created = strtotime($date_str . ' midnight');
  }

  if (isset($form_state['values']['uid']) && user_access('administer goals')) {
    $goal->uid = (int) $form_state['values']['uid'];
  }

  $goal->save();

  backdrop_set_message(t('Goal %label saved.', array('%label' => $goal->label)));
  $form_state['redirect'] = 'drop/goals/manage';
}

// ---------------------------------------------------------------------------
// Delete form
// ---------------------------------------------------------------------------

/**
 * Form: confirm goal deletion.
 *
 * @param array $form
 * @param array $form_state
 * @param DropGoal $goal
 *
 * @return array
 */
function dropgoals_goal_delete_form($form, &$form_state, DropGoal $goal) {
  $form_state['goal'] = $goal;
  return confirm_form(
    $form,
    t('Are you sure you want to delete the goal %label?', array('%label' => $goal->label)),
    'drop/goals/manage',
    t('All completion history for this goal will also be deleted. This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for dropgoals_goal_delete_form().
 */
function dropgoals_goal_delete_form_submit($form, &$form_state) {
  $goal = $form_state['goal'];

  // Delete all log entries for this goal.
  db_delete('drop_goal_log')
    ->condition('gid', $goal->gid)
    ->execute();

  $goal->delete();
  backdrop_set_message(t('Goal %label and its history have been deleted.', array('%label' => $goal->label)));
  $form_state['redirect'] = 'drop/goals/manage';
}
