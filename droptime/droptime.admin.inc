<?php
/**
 * @file
 * Admin callbacks for DropTime.
 *
 * Note: project management forms have moved to dropcrm.admin.inc.
 */

/**
 * Form: reassign time entries from one user to another.
 */
function droptime_reassign_form($form, &$form_state) {
  // Build a list of users who have time entries.
  $uids_with_entries = db_query('SELECT DISTINCT uid FROM {drop_time_entry} ORDER BY uid')
    ->fetchCol();

  // Build full user list for the "to" selector (all active non-anonymous users).
  $all_users = db_query(
    "SELECT uid, name FROM {users} WHERE uid > 0 AND status = 1 ORDER BY name"
  )->fetchAllKeyed();

  // Build "from" options — only users who actually have entries.
  $from_options = array();
  foreach ($uids_with_entries as $uid) {
    $account = user_load($uid);
    if ($account) {
      $count = db_query('SELECT COUNT(*) FROM {drop_time_entry} WHERE uid = :uid', array(':uid' => $uid))->fetchField();
      $from_options[$uid] = $account->name . ' (' . $count . ' entries)';
    }
  }

  if (empty($from_options)) {
    $form['message'] = array(
      '#markup' => '<p>' . t('No time entries found.') . '</p>',
    );
    return $form;
  }

  $form['from_uid'] = array(
    '#type'          => 'select',
    '#title'         => t('Move entries FROM'),
    '#options'       => $from_options,
    '#required'      => TRUE,
    '#description'   => t('The account whose entries will be moved.'),
  );

  $to_options = array('' => t('— Select —')) + $all_users;
  $form['to_uid'] = array(
    '#type'          => 'select',
    '#title'         => t('Move entries TO'),
    '#options'       => $to_options,
    '#required'      => TRUE,
    '#description'   => t('The account that will own the entries after moving.'),
  );

  $form['date_range'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Optional date range'),
    '#description' => t('Leave blank to move ALL entries from the selected account.'),
    '#collapsible' => TRUE,
    '#collapsed'   => TRUE,
  );
  $form['date_range']['date_from'] = array(
    '#type'        => 'textfield',
    '#title'       => t('From date'),
    '#placeholder' => 'YYYY-MM-DD',
    '#size'        => 12,
    '#maxlength'   => 10,
  );
  $form['date_range']['date_to'] = array(
    '#type'        => 'textfield',
    '#title'       => t('To date'),
    '#placeholder' => 'YYYY-MM-DD',
    '#size'        => 12,
    '#maxlength'   => 10,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Preview'),
  );

  return $form;
}

/**
 * Validation for the reassign form.
 */
function droptime_reassign_form_validate($form, &$form_state) {
  $from = $form_state['values']['from_uid'];
  $to   = $form_state['values']['to_uid'];

  if (empty($to)) {
    form_set_error('to_uid', t('Please select a destination account.'));
  }
  if ($from == $to) {
    form_set_error('to_uid', t('Source and destination accounts must be different.'));
  }

  $date_from = trim($form_state['values']['date_from']);
  $date_to   = trim($form_state['values']['date_to']);
  $date_pattern = '/^\d{4}-\d{2}-\d{2}$/';
  if (!empty($date_from) && !preg_match($date_pattern, $date_from)) {
    form_set_error('date_range][date_from', t('From date must be in YYYY-MM-DD format.'));
  }
  if (!empty($date_to) && !preg_match($date_pattern, $date_to)) {
    form_set_error('date_range][date_to', t('To date must be in YYYY-MM-DD format.'));
  }
  if (!empty($date_from) && !empty($date_to) && $date_from > $date_to) {
    form_set_error('date_range][date_to', t('"To date" must be on or after "From date".'));
  }
}

/**
 * Submit handler — shows a confirmation step.
 */
function droptime_reassign_form_submit($form, &$form_state) {
  $from      = $form_state['values']['from_uid'];
  $to        = $form_state['values']['to_uid'];
  $date_from = $form_state['values']['date_from'];
  $date_to   = $form_state['values']['date_to'];

  // Count matching entries.
  $query = db_select('drop_time_entry', 'e')
    ->condition('e.uid', $from)
    ->fields('e', array('eid'));
  if (!empty($date_from)) {
    $query->condition('e.entry_date', $date_from, '>=');
  }
  if (!empty($date_to)) {
    $query->condition('e.entry_date', $date_to, '<=');
  }
  $count = $query->countQuery()->execute()->fetchField();

  $from_account = user_load($from);
  $to_account   = user_load($to);

  // Store in form state for the confirm form.
  $form_state['redirect'] = FALSE;
  backdrop_set_message(t(
    'You are about to move <strong>@count time entries</strong> from <em>@from</em> to <em>@to</em>. <a href="@confirm">Click here to confirm</a>.',
    array(
      '@count'   => $count,
      '@from'    => $from_account->name,
      '@to'      => $to_account->name,
      '@confirm' => url('drop/time/reassign/confirm', array(
        'query' => array(
          'from'      => $from,
          'to'        => $to,
          'date_from' => $date_from,
          'date_to'   => $date_to,
        ),
      )),
    )
  ), 'warning');
}

/**
 * Page callback: confirm and execute the reassignment.
 */
function droptime_reassign_confirm_page() {
  $from      = isset($_GET['from'])      ? (int) $_GET['from']      : 0;
  $to        = isset($_GET['to'])        ? (int) $_GET['to']        : 0;
  $date_from = isset($_GET['date_from']) ? $_GET['date_from']       : '';
  $date_to   = isset($_GET['date_to'])   ? $_GET['date_to']         : '';

  if (!$from || !$to || $from === $to) {
    return backdrop_access_denied();
  }

  $from_account = user_load($from);
  $to_account   = user_load($to);
  if (!$from_account || !$to_account) {
    return backdrop_not_found();
  }

  return backdrop_get_form('droptime_reassign_confirm_form', $from, $to, $date_from, $date_to);
}

/**
 * Confirmation form before executing the reassignment.
 */
function droptime_reassign_confirm_form($form, &$form_state, $from, $to, $date_from, $date_to) {
  $from_account = user_load($from);
  $to_account   = user_load($to);

  // Count entries that will move.
  $query = db_select('drop_time_entry', 'e')
    ->condition('e.uid', $from);
  if (!empty($date_from)) {
    $query->condition('e.entry_date', $date_from, '>=');
  }
  if (!empty($date_to)) {
    $query->condition('e.entry_date', $date_to, '<=');
  }
  $count = $query->countQuery()->execute()->fetchField();

  $form_state['storage']['from']      = $from;
  $form_state['storage']['to']        = $to;
  $form_state['storage']['date_from'] = $date_from;
  $form_state['storage']['date_to']   = $date_to;

  $range_text = '';
  if (!empty($date_from) || !empty($date_to)) {
    $range_text = ' ' . t('(date range: @from to @to)', array(
      '@from' => $date_from ?: t('beginning'),
      '@to'   => $date_to   ?: t('end'),
    ));
  }

  return confirm_form(
    $form,
    t('Move @count time entries from @from to @to?', array(
      '@count' => $count,
      '@from'  => $from_account->name,
      '@to'    => $to_account->name,
    )),
    'drop/time/reassign',
    t('This will reassign @count entries@range from <strong>@from</strong> to <strong>@to</strong>. This cannot be undone.', array(
      '@count' => $count,
      '@range' => $range_text,
      '@from'  => $from_account->name,
      '@to'    => $to_account->name,
    )),
    t('Move entries'),
    t('Cancel')
  );
}

/**
 * Submit handler: execute the reassignment.
 */
function droptime_reassign_confirm_form_submit($form, &$form_state) {
  $from      = $form_state['storage']['from'];
  $to        = $form_state['storage']['to'];
  $date_from = $form_state['storage']['date_from'];
  $date_to   = $form_state['storage']['date_to'];

  $query = db_update('drop_time_entry')
    ->fields(array('uid' => $to))
    ->condition('uid', $from);
  if (!empty($date_from)) {
    $query->condition('entry_date', $date_from, '>=');
  }
  if (!empty($date_to)) {
    $query->condition('entry_date', $date_to, '<=');
  }
  $count = $query->execute();

  $from_account = user_load($from);
  $to_account   = user_load($to);
  backdrop_set_message(t('Moved @count time entries from @from to @to.', array(
    '@count' => $count,
    '@from'  => $from_account->name,
    '@to'    => $to_account->name,
  )));

  watchdog('droptime', 'Reassigned @count time entries from uid @from to uid @to.', array(
    '@count' => $count,
    '@from'  => $from,
    '@to'    => $to,
  ), WATCHDOG_NOTICE);

  $form_state['redirect'] = 'drop/time/reassign';
}
