<?php
/**
 * @file
 * Form callbacks for DropTasks.
 */

/**
 * Form: add or edit a task.
 *
 * @param array $form
 * @param array $form_state
 * @param DroptTask|null $task
 *   Existing task for edit, or NULL for add.
 *
 * @return array
 */
function droptasks_task_form($form, &$form_state, $task = NULL) {
  if (empty($task)) {
    $task = new DroptTask();
  }
  $form_state['task'] = $task;

  // Pre-fill title from GET param when creating from an activity.
  $title_default = $task->title;
  if (empty($task->tid) && !empty($_GET['title'])) {
    $title_default = check_plain($_GET['title']);
  }

  $form['title'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Task'),
    '#default_value' => $title_default,
    '#required'      => TRUE,
    '#maxlength'     => 255,
    '#autofocus'     => TRUE,
  );

  // Linked contact — pre-filled from GET param or existing task value.
  $contact_id = !empty($task->contact_id) ? $task->contact_id : NULL;
  if (empty($task->tid) && !empty($_GET['contact'])) {
    $contact_id = (int) $_GET['contact'];
  }
  if ($contact_id && module_exists('dropcrm')) {
    $contacts = entity_load('crm_contact', array($contact_id));
    $contact  = $contacts ? reset($contacts) : NULL;
    if ($contact) {
      $form['contact_display'] = array(
        '#markup' => '<p class="droptasks-linked-contact">'
          . t('Linked contact:') . ' '
          . l(entity_label('crm_contact', $contact), 'drop/crm/contact/' . $contact_id)
          . '</p>',
      );
    }
  }
  $form['contact_id'] = array(
    '#type'  => 'hidden',
    '#value' => $contact_id ? $contact_id : '',
  );

  // Source activity — stored silently from GET param or existing task value.
  $activity_id = !empty($task->source_activity_id) ? $task->source_activity_id : NULL;
  if (empty($task->tid) && !empty($_GET['activity'])) {
    $activity_id = (int) $_GET['activity'];
  }
  $form['source_activity_id'] = array(
    '#type'  => 'hidden',
    '#value' => $activity_id ? $activity_id : '',
  );

  $form['status'] = array(
    '#type'          => 'select',
    '#title'         => t('Status'),
    '#options'       => array(
      'pending'  => t('Pending'),
      'complete' => t('Complete'),
    ),
    '#default_value' => $task->status,
  );

  $form['priority'] = array(
    '#type'          => 'select',
    '#title'         => t('Priority'),
    '#options'       => array_reverse(drop_base_priority_labels(), TRUE),
    '#default_value' => $task->priority,
  );

  $due_default = !empty($task->due_date) ? format_date($task->due_date, 'custom', 'Y-m-d') : '';
  $form['due_date'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Due date'),
    '#default_value' => $due_default,
    '#description'   => t('Format: YYYY-MM-DD. Leave blank for no due date.'),
    '#size'          => 12,
    '#maxlength'     => 10,
    '#attributes'    => array('class' => array('droptasks-datepicker'), 'autocomplete' => 'off'),
  );

  $form['#attached']['js'][]  = 'core/misc/ui/jquery.ui.datepicker.min.js';
  $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.core.css';
  $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.datepicker.css';
  $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.theme.css';
  $form['#attached']['js'][]  = backdrop_get_path('module', 'droptasks') . '/droptasks.datepicker.js';

  // Recurrence.
  $day_names = array(
    0 => t('Sun'), 1 => t('Mon'), 2 => t('Tue'), 3 => t('Wed'),
    4 => t('Thu'), 5 => t('Fri'), 6 => t('Sat'),
  );
  $recurrence_days_default = array();
  if (!empty($task->recurrence_days)) {
    $recurrence_days_default = array_map('intval', explode(',', $task->recurrence_days));
  }

  $form['completion_mode'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Individual completion — each assignee marks done for themselves'),
    '#description'   => t('When checked, each assignee must mark this task complete separately. When unchecked, one person completing it marks it done for everyone.'),
    '#default_value' => !empty($task->completion_mode) ? 1 : 0,
  );

  $form['recurrence_fieldset'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Recurrence'),
    '#collapsible' => TRUE,
    '#collapsed'   => empty($task->recurrence),
  );
  $form['recurrence_fieldset']['recurrence'] = array(
    '#type'          => 'select',
    '#title'         => t('Repeat'),
    '#options'       => array(
      ''        => t('None'),
      'daily'   => t('Daily'),
      'weekly'  => t('Weekly'),
      'monthly' => t('Monthly'),
    ),
    '#default_value' => $task->recurrence,
  );
  $form['recurrence_fieldset']['recurrence_days'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Repeat on'),
    '#options'       => $day_names,
    '#default_value' => $recurrence_days_default,
    '#states'        => array(
      'visible' => array(':input[name="recurrence"]' => array('value' => 'weekly')),
    ),
  );
  $form['recurrence_fieldset']['recurrence_month_type'] = array(
    '#type'          => 'radios',
    '#title'         => t('Monthly repeat'),
    '#options'       => array(
      'day_of_month' => t('On the same day of the month as the due date'),
      'nth_weekday'  => t('On the Nth weekday of the month'),
    ),
    '#default_value' => $task->recurrence_month_type ?: 'day_of_month',
    '#states'        => array(
      'visible' => array(':input[name="recurrence"]' => array('value' => 'monthly')),
    ),
  );
  $form['recurrence_fieldset']['recurrence_month_week'] = array(
    '#type'          => 'select',
    '#title'         => t('Which week'),
    '#options'       => array(
      1  => t('1st'),
      2  => t('2nd'),
      3  => t('3rd'),
      4  => t('4th'),
      -1 => t('Last'),
    ),
    '#default_value' => $task->recurrence_month_week ?: 1,
    '#states'        => array(
      'visible' => array(
        ':input[name="recurrence"]'            => array('value' => 'monthly'),
        ':input[name="recurrence_month_type"]' => array('value' => 'nth_weekday'),
      ),
    ),
  );
  $form['recurrence_fieldset']['recurrence_month_weekday'] = array(
    '#type'          => 'select',
    '#title'         => t('Day of week'),
    '#options'       => $day_names,
    '#default_value' => $task->recurrence_month_weekday,
    '#states'        => array(
      'visible' => array(
        ':input[name="recurrence"]'            => array('value' => 'monthly'),
        ':input[name="recurrence_month_type"]' => array('value' => 'nth_weekday'),
      ),
    ),
  );

  $form['notes'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Notes'),
    '#default_value' => $task->notes,
    '#rows'          => 5,
    '#required'      => FALSE,
  );

  $is_new = empty($task->tid);

  // Assignees — admin/PM only.
  if (user_access('administer tasks')) {
    // Find roles that have 'access tasks' permission.
    $roles_with_access = array();
    foreach (user_roles(TRUE, 'access tasks') as $role_name => $role_label) {
      $roles_with_access[] = $role_name;
    }
    // Load users who have at least one of those roles.
    $team_members = array();
    if ($roles_with_access) {
      $team_members = db_query(
        "SELECT DISTINCT u.uid, u.name FROM {users} u
         INNER JOIN {users_roles} ur ON ur.uid = u.uid
         WHERE ur.role IN (:roles) AND u.status = 1 AND u.uid > 0
         ORDER BY u.name",
        array(':roles' => $roles_with_access)
      )->fetchAllKeyed();
    }

    $current_assignees = array();
    if (!empty($task->tid)) {
      $current_assignees = db_select('drop_task_assignee', 'a')
        ->fields('a', array('uid'))
        ->condition('a.tid', $task->tid)
        ->execute()
        ->fetchCol();
      $current_assignees = array_map('intval', $current_assignees);
    }

    $form['assignees'] = array(
      '#type'          => 'checkboxes',
      '#title'         => t('Assignees'),
      '#options'       => $team_members,
      '#default_value' => $current_assignees,
      '#description'   => t('This task will appear on each selected person\'s task list.'),
      '#weight'        => 44,
    );
  }

  // Owner — admin only, shown when editing an existing task.
  if (!$is_new && user_access('administer tasks')) {
    $all_users = db_query("SELECT uid, name FROM {users} WHERE uid > 0 AND status = 1 ORDER BY name")->fetchAllKeyed();
    $form['uid'] = array(
      '#type'          => 'select',
      '#title'         => t('Owner'),
      '#options'       => $all_users,
      '#default_value' => $task->uid,
      '#description'   => t('Reassign this task to a different person.'),
      '#weight'        => 45,
    );
  }

  // Attach field widgets (includes field_drop_tags taxonomy autocomplete).
  field_attach_form('drop_task', $task, $form, $form_state);

  // Push tags below all hand-coded fields.
  if (isset($form['field_drop_tags'])) {
    $form['field_drop_tags']['#weight'] = 50;
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_new ? t('Add task') : t('Save task'),
  );
  if (!$is_new) {
    $form['actions']['cancel'] = array(
      '#type'       => 'link',
      '#title'      => t('Cancel'),
      '#href'       => 'drop/tasks/' . $task->tid,
      '#attributes' => array('class' => array('button', 'button-secondary')),
    );
  }

  // "Back to activity" link when creating a follow-up from an activity view.
  if ($is_new && $activity_id) {
    $form['actions']['back'] = array(
      '#markup' => ' ' . l(t('← Back to activity'), 'drop/crm/activity/' . $activity_id),
    );
  }

  return $form;
}

/**
 * Validate handler for droptasks_task_form().
 */
function droptasks_task_form_validate($form, &$form_state) {
  $due = trim($form_state['values']['due_date']);
  if ($due !== '') {
    $ts = strtotime($due . ' midnight');
    if ($ts === FALSE || !preg_match('/^\d{4}-\d{2}-\d{2}$/', $due)) {
      form_set_error('due_date', t('Due date must be in YYYY-MM-DD format.'));
    }
  }

  $recurrence = $form_state['values']['recurrence'];
  if ($recurrence === 'weekly') {
    $days = array_filter($form_state['values']['recurrence_days']);
    if (empty($days)) {
      form_set_error('recurrence_days', t('Please select at least one day for weekly recurrence.'));
    }
  }
  if ($recurrence === 'monthly'
    && $form_state['values']['recurrence_month_type'] === 'day_of_month'
    && $due === '') {
    form_set_error('due_date', t('A due date is required to determine which day of the month to repeat on.'));
  }
}

/**
 * Submit handler for droptasks_task_form().
 */
function droptasks_task_form_submit($form, &$form_state) {
  $task = $form_state['task'];

  $task->title    = $form_state['values']['title'];
  $task->status   = $form_state['values']['status'];
  $task->priority = (int) $form_state['values']['priority'];
  $task->notes    = $form_state['values']['notes'];

  $due = trim($form_state['values']['due_date']);
  $task->due_date = ($due !== '') ? strtotime($due . ' midnight') : NULL;

  $cid = $form_state['values']['contact_id'];
  $task->contact_id = ($cid !== '') ? (int) $cid : NULL;

  $aid = $form_state['values']['source_activity_id'];
  $task->source_activity_id = ($aid !== '') ? (int) $aid : NULL;

  if (isset($form_state['values']['uid']) && user_access('administer tasks')) {
    $task->uid = (int) $form_state['values']['uid'];
  }

  // Recurrence.
  $task->completion_mode = empty($form_state['values']['completion_mode']) ? 0 : 1;

  $task->recurrence = $form_state['values']['recurrence'];
  if ($task->recurrence === 'weekly') {
    $checked = array_keys(array_filter($form_state['values']['recurrence_days']));
    sort($checked);
    $task->recurrence_days = implode(',', $checked);
  }
  else {
    $task->recurrence_days = '';
  }
  if ($task->recurrence === 'monthly') {
    $task->recurrence_month_type    = $form_state['values']['recurrence_month_type'];
    $task->recurrence_month_week    = (int) $form_state['values']['recurrence_month_week'];
    $task->recurrence_month_weekday = (int) $form_state['values']['recurrence_month_weekday'];
  }
  else {
    $task->recurrence_month_type    = 'day_of_month';
    $task->recurrence_month_week    = 1;
    $task->recurrence_month_weekday = 1;
  }

  // Copy field widget values (tags etc.) onto the entity before saving.
  field_attach_submit('drop_task', $task, $form, $form_state);

  $task->save();

  // Save assignees (delete + re-insert).
  if (user_access('administer tasks') && isset($form_state['values']['assignees'])) {
    $checked_uids = array_keys(array_filter($form_state['values']['assignees']));
    if (!empty($task->tid)) {
      db_delete('drop_task_assignee')->condition('tid', $task->tid)->execute();
      foreach ($checked_uids as $auid) {
        db_insert('drop_task_assignee')->fields(array(
          'tid' => $task->tid,
          'uid' => (int) $auid,
        ))->execute();
      }
    }
  }

  backdrop_set_message(t('Task saved.'));
  // Return to the source activity so follow-up tasks can be added easily.
  if (!empty($task->source_activity_id)) {
    $form_state['redirect'] = 'drop/crm/activity/' . $task->source_activity_id;
  }
  else {
    $form_state['redirect'] = 'drop/tasks';
  }
}

/**
 * Form: confirm task deletion.
 *
 * @param array $form
 * @param array $form_state
 * @param DroptTask $task
 *
 * @return array
 */
function droptasks_task_delete_form($form, &$form_state, DroptTask $task) {
  $form_state['task'] = $task;
  return confirm_form(
    $form,
    t('Are you sure you want to delete the task %title?', array('%title' => $task->title)),
    'tasks',
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for droptasks_task_delete_form().
 */
function droptasks_task_delete_form_submit($form, &$form_state) {
  $task = $form_state['task'];
  db_delete('drop_task_assignee')->condition('tid', $task->tid)->execute();
  db_delete('drop_task_completion')->condition('tid', $task->tid)->execute();
  $task->delete();
  backdrop_set_message(t('Task %title deleted.', array('%title' => $task->title)));
  $form_state['redirect'] = 'drop/tasks';
}
