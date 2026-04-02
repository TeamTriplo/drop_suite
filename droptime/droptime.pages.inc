<?php
/**
 * @file
 * Page callbacks for DropTime.
 */

// ---------------------------------------------------------------------------
// Daily timesheet
// ---------------------------------------------------------------------------

/**
 * Page callback: daily time entry sheet.
 */
function droptime_daily_page() {
  global $user;

  // Resolve "today" in the site timezone.
  $site_tz  = new DateTimeZone(config_get('system.date', 'default_timezone') ?: date_default_timezone_get());
  $today    = (new DateTime('now', $site_tz))->format('Y-m-d');

  // Allow navigating to a specific date via ?date=YYYY-MM-DD.
  $date = (isset($_GET['date']) && preg_match('/^\d{4}-\d{2}-\d{2}$/', $_GET['date']))
    ? $_GET['date']
    : $today;

  backdrop_set_title(t('Time Tracker'));

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'droptime') . '/droptime.css'),
    ),
  );

  // Date navigation bar.
  $output['nav'] = array(
    '#markup' => _droptime_date_nav($date, $today),
    '#weight' => -20,
  );

  // "Back" link when arriving from an activity or task via "Log time" button.
  if (!empty($_GET['return'])) {
    $output['back'] = array(
      '#markup' => '<p class="droptime-back-link">' . l(t('← Back'), $_GET['return']) . '</p>',
      '#weight' => -15,
    );
  }

  // Timer banner area — always rendered so AJAX can populate it.
  $timer = _droptime_get_timer($user->uid);
  $output['timer'] = array(
    '#markup' => '<div id="droptime-banner-area">' . ($timer ? _droptime_timer_banner($timer) : '') . '</div>',
    '#weight' => -10,
  );
  $output['#attached']['js'][] = backdrop_get_path('module', 'droptime') . '/droptime.js';

  // No projects yet? Prompt to create one.
  $projects = droptime_project_options();
  if (empty($projects)) {
    $output['no_projects'] = array(
      '#markup' => '<p class="droptime-notice">'
        . t('No active projects yet. !link to get started.', array(
            '!link' => l(t('Add a project'), 'drop/crm/projects/add'),
          ))
        . '</p>',
      '#weight' => 0,
    );
  }
  else {
    // Quick-add form.
    $add_form = backdrop_get_form('droptime_entry_form', NULL, $date);
    $output['add_form'] = $add_form;
    $output['add_form']['#weight'] = 0;
  }

  // Entries for this date.
  $entries = _droptime_load_day_entries($user->uid, $date);
  $output['entries'] = array(
    '#markup' => _droptime_render_entries($entries, $date),
    '#weight' => 10,
  );

  return $output;
}

/**
 * Renders the date navigation bar.
 */
function _droptime_date_nav($date, $today) {
  $ts       = strtotime($date);
  $prev     = date('Y-m-d', strtotime('-1 day', $ts));
  $next     = date('Y-m-d', strtotime('+1 day', $ts));
  $is_today = ($date === $today);
  $label    = format_date($ts, 'custom', 'l, F j, Y');

  $html  = '<div class="droptime-date-nav">';
  $html .= l('‹', 'drop/time', array('query' => array('date' => $prev), 'attributes' => array('class' => array('droptime-nav-arrow'), 'title' => t('Previous day'))));
  $html .= '<span class="droptime-nav-label">' . check_plain($label) . '</span>';

  if (!$is_today) {
    $html .= l('›', 'drop/time', array('query' => array('date' => $next), 'attributes' => array('class' => array('droptime-nav-arrow'), 'title' => t('Next day'))));
    $html .= ' ' . l(t('Today'), 'drop/time', array('attributes' => array('class' => array('droptime-nav-today'))));
  }
  else {
    $html .= '<span class="droptime-nav-arrow droptime-nav-arrow--disabled">›</span>';
  }

  // Timer start link — hidden via inline style if a timer is already running.
  global $user;
  $start_hidden = _droptime_get_timer($user->uid) ? ' style="display:none"' : '';
  $html .= '<span id="droptime-nav-timer-action"' . $start_hidden . '>';
  $html .= l(t('▶ Start timer'), '#', array(
    'attributes' => array(
      'class'    => array('droptime-timer-start-link', 'js-droptime-start'),
      'data-url' => url('drop/time/timer/start'),
    ),
  ));
  $html .= '</span>';

  $html .= '</div>';
  return $html;
}

/**
 * Loads time entries for a specific user and date, sorted by start time.
 */
function _droptime_load_day_entries($uid, $date) {
  $rows = db_select('drop_time_entry', 'e')
    ->fields('e')
    ->condition('e.uid', $uid)
    ->condition('e.entry_date', $date)
    ->orderBy('e.start_time', 'ASC')
    ->execute()
    ->fetchAll();

  if (empty($rows)) {
    return array();
  }

  // Load project data in one query.
  $pids     = array_unique(array_column((array) $rows, 'pid'));
  $projects = entity_load('drop_project', $pids);

  $entries = array();
  foreach ($rows as $row) {
    $row->project = isset($projects[$row->pid]) ? $projects[$row->pid] : NULL;
    $entries[]    = $row;
  }
  return $entries;
}

/**
 * Renders the entries list and daily totals for a given date.
 */
function _droptime_render_entries($entries, $date) {
  if (empty($entries)) {
    return '<p class="droptime-no-entries">' . t('No time logged for this day yet.') . '</p>';
  }

  $total_mins    = 0;
  $billable_mins = 0;

  $html  = '<div class="droptime-entries">';
  $html .= '<h3 class="droptime-entries-heading">' . t("Today's entries") . '</h3>';
  $html .= '<ul class="droptime-entry-list">';

  foreach ($entries as $entry) {
    $project = $entry->project;
    $color   = $project ? check_plain($project->color) : '#ccc';
    $name    = $project ? check_plain($project->name) : t('Unknown project');
    $swatch  = '<span class="droptime-swatch" style="background:' . $color . ';"></span>';
    $dur     = droptime_format_duration($entry->duration);
    $bill    = $entry->billable
      ? '<span class="droptime-bill droptime-bill--yes" title="' . t('Billable') . '">$</span>'
      : '<span class="droptime-bill droptime-bill--no"></span>';
    $note    = !empty($entry->notes) ? '<span class="droptime-entry-note">' . check_plain($entry->notes) . '</span>' : '';

    $edit_link   = l(t('Edit'), 'drop/time/entry/' . $entry->eid . '/edit');
    $delete_link = l(t('Delete'), 'drop/time/entry/' . $entry->eid . '/delete', array('query' => array('date' => $date)));

    $html .= '<li class="droptime-entry">';
    $html .= '<span class="droptime-entry-time">' . check_plain($entry->start_time) . '–' . check_plain($entry->end_time) . '</span>';
    $html .= '<span class="droptime-entry-project">' . $swatch . $name . '</span>';
    $html .= $note;
    $html .= $bill;
    $html .= '<span class="droptime-entry-dur">' . $dur . '</span>';
    $html .= '<span class="droptime-entry-actions">' . $edit_link . ' · ' . $delete_link . '</span>';
    $html .= '</li>';

    $total_mins    += $entry->duration;
    $billable_mins += $entry->billable ? $entry->duration : 0;
  }

  $html .= '</ul>';

  // Totals bar.
  $non_bill = $total_mins - $billable_mins;
  $html .= '<div class="droptime-totals">';
  $html .= '<span class="droptime-total-total">' . t('Total: @t', array('@t' => droptime_format_duration($total_mins))) . '</span>';
  $html .= '<span class="droptime-total-bill">' . t('Billable: @b', array('@b' => droptime_format_duration($billable_mins))) . '</span>';
  if ($non_bill > 0) {
    $html .= '<span class="droptime-total-nonbill">' . t('Non-billable: @n', array('@n' => droptime_format_duration($non_bill))) . '</span>';
  }
  $html .= '</div>';
  $html .= '</div>';

  return $html;
}

// ---------------------------------------------------------------------------
// Add / edit entry form
// ---------------------------------------------------------------------------

/**
 * Page callback: edit an existing time entry.
 */
function droptime_entry_edit_page(DropTimeEntry $entry) {
  backdrop_set_title(t('Edit time entry'));
  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'droptime') . '/droptime.css'),
    ),
  );
  $output['form'] = backdrop_get_form('droptime_entry_form', $entry, $entry->entry_date);
  return $output;
}

/**
 * Form: add or edit a time entry.
 *
 * @param DropTimeEntry|null $entry
 *   Existing entry for editing, or NULL to add new.
 * @param string $date
 *   The entry date in YYYY-MM-DD format.
 */
function droptime_entry_form($form, &$form_state, $entry = NULL, $date = NULL) {
  global $user;

  $is_edit = !empty($entry);
  if (!$date) {
    $date = $is_edit ? $entry->entry_date : date('Y-m-d');
  }

  // Pre-fill start/end from GET params (timer stop).
  $default_start = $is_edit ? $entry->start_time : (isset($_GET['start']) ? check_plain($_GET['start']) : '');
  $default_end   = $is_edit ? $entry->end_time   : (isset($_GET['end'])   ? check_plain($_GET['end'])   : '');

  // Pre-fill notes from GET param (e.g. from "Log time" button on activity/task).
  $default_notes = $is_edit ? $entry->notes : (isset($_GET['notes']) ? check_plain($_GET['notes']) : '');

  // Return URL — redirect back to source after saving (e.g. the activity page).
  $source_return = !$is_edit && !empty($_GET['return']) ? $_GET['return'] : '';

  $projects = droptime_project_options();

  $form['#attributes'] = array('class' => array('droptime-entry-form'));

  $form['pid'] = array(
    '#type'          => 'select',
    '#title'         => t('Project'),
    '#options'       => $projects,
    '#empty_option'  => t('— Select —'),
    '#required'      => TRUE,
    '#default_value' => $is_edit ? $entry->pid : NULL,
  );

  $form['start_time'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Start'),
    '#default_value' => $default_start,
    '#size'          => 6,
    '#maxlength'     => 5,
    '#required'      => TRUE,
    '#attributes'    => array('placeholder' => '09:00', 'class' => array('droptime-time-field')),
    '#description'   => t('24h format'),
  );

  $form['end_time'] = array(
    '#type'          => 'textfield',
    '#title'         => t('End'),
    '#default_value' => $default_end,
    '#size'          => 6,
    '#maxlength'     => 5,
    '#required'      => TRUE,
    '#attributes'    => array('placeholder' => '10:30', 'class' => array('droptime-time-field')),
    '#description'   => t('24h format'),
  );

  if (user_access('choose billable status')) {
    $form['billable'] = array(
      '#type'          => 'checkbox',
      '#title'         => t('Billable'),
      '#default_value' => $is_edit ? $entry->billable : 1,
    );
  }
  else {
    $form['billable'] = array(
      '#type'  => 'hidden',
      '#value' => 1,
    );
  }

  $form['notes'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Note'),
    '#default_value' => $default_notes,
    '#size'          => 40,
    '#maxlength'     => 500,
    '#attributes'    => array('placeholder' => t('Brief description…')),
  );

  $form['source_return'] = array(
    '#type'  => 'hidden',
    '#value' => $source_return,
  );

  if ($is_edit) {
    $form['entry_date'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Date'),
      '#default_value' => $date,
      '#size'          => 12,
      '#maxlength'     => 10,
      '#required'      => TRUE,
      '#attributes'    => array('class' => array('droptime-datepicker')),
    );
    $form['#attached']['js'][] = 'core/misc/ui/jquery.ui.datepicker.min.js';
    $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.core.css';
    $form['#attached']['css'][] = 'core/misc/ui/jquery.ui.theme.css';
    $form['#attached']['js'][] = backdrop_get_path('module', 'droptime') . '/droptime.js';
  }
  else {
    $form['entry_date'] = array(
      '#type'  => 'hidden',
      '#value' => $date,
    );
  }

  if ($is_edit) {
    $form['eid'] = array('#type' => 'hidden', '#value' => $entry->eid);
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Update entry') : t('Add entry'),
  );
  if ($is_edit) {
    $form['actions']['cancel'] = array(
      '#markup' => ' ' . l(t('Cancel'), 'drop/time', array('query' => array('date' => $date))),
    );
  }

  return $form;
}

/**
 * Validation handler for droptime_entry_form().
 */
function droptime_entry_form_validate($form, &$form_state) {
  $start = trim($form_state['values']['start_time']);
  $end   = trim($form_state['values']['end_time']);

  $date = trim($form_state['values']['entry_date']);
  if (!empty($date) && !preg_match('/^\d{4}-\d{2}-\d{2}$/', $date)) {
    form_set_error('entry_date', t('Date must be in YYYY-MM-DD format.'));
  }

  if (!preg_match('/^\d{2}:\d{2}$/', $start)) {
    form_set_error('start_time', t('Start time must be in HH:MM format (e.g. 09:00).'));
  }
  elseif ((int) substr($start, 0, 2) > 23 || (int) substr($start, 3, 2) > 59) {
    form_set_error('start_time', t('Start time is not a valid time (hours 00–23, minutes 00–59).'));
  }
  if (!preg_match('/^\d{2}:\d{2}$/', $end)) {
    form_set_error('end_time', t('End time must be in HH:MM format (e.g. 17:30).'));
  }
  elseif ((int) substr($end, 0, 2) > 23 || (int) substr($end, 3, 2) > 59) {
    form_set_error('end_time', t('End time is not a valid time (hours 00–23, minutes 00–59).'));
  }
  if (!form_get_errors()) {
    list($sh, $sm) = explode(':', $start);
    list($eh, $em) = explode(':', $end);
    if (($eh * 60 + $em) <= ($sh * 60 + $sm)) {
      form_set_error('end_time', t('End time must be after start time.'));
    }
  }
}

/**
 * Submit handler for droptime_entry_form().
 */
function droptime_entry_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $date   = $values['entry_date'];

  if (!empty($values['eid'])) {
    $entries = entity_load('drop_time_entry', array($values['eid']));
    $entry   = $entries ? reset($entries) : entity_create('drop_time_entry', array());
  }
  else {
    $entry = entity_create('drop_time_entry', array());
  }

  $entry->pid        = (int) $values['pid'];
  $entry->entry_date = $date;
  $entry->start_time = trim($values['start_time']);
  $entry->end_time   = trim($values['end_time']);
  $entry->billable   = (int) $values['billable'];
  $entry->notes      = trim($values['notes']);

  $entry->save();

  backdrop_set_message(t('Time entry saved.'));
  $return = !empty($values['source_return']) ? $values['source_return'] : NULL;
  if ($return) {
    $form_state['redirect'] = $return;
  }
  else {
    $form_state['redirect'] = array('drop/time', array('query' => array('date' => $date)));
  }
}

/**
 * Form: confirm deletion of a time entry.
 */
function droptime_entry_delete_form($form, &$form_state, DropTimeEntry $entry) {
  $form['#entry'] = $entry;
  return confirm_form(
    $form,
    t('Delete this time entry?'),
    isset($_GET['date']) ? 'drop/time?date=' . check_plain($_GET['date']) : 'drop/time',
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for droptime_entry_delete_form().
 */
function droptime_entry_delete_form_submit($form, &$form_state) {
  $entry = $form['#entry'];
  $date  = $entry->entry_date;
  $entry->delete();
  backdrop_set_message(t('Time entry deleted.'));
  $form_state['redirect'] = array('drop/time', array('query' => array('date' => $date)));
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------

/**
 * Returns the running timer state for a user, or NULL if none.
 */
function _droptime_get_timer($uid) {
  return state_get('droptime_timer_' . $uid, NULL);
}

/**
 * Page callback: start the timer.
 */
function droptime_timer_start() {
  global $user;

  $site_tz = new DateTimeZone(config_get('system.date', 'default_timezone') ?: date_default_timezone_get());
  $now     = new DateTime('now', $site_tz);

  $timer = array(
    'date'  => $now->format('Y-m-d'),
    'start' => $now->format('H:i'),
    'ts'    => REQUEST_TIME,
  );
  state_set('droptime_timer_' . $user->uid, $timer);

  // AJAX response: return banner HTML as JSON.
  if (!empty($_POST['_dt_ajax'])) {
    backdrop_add_http_header('Content-Type', 'application/json');
    print backdrop_json_encode(array(
      'ts'         => REQUEST_TIME,
      'bannerHtml' => _droptime_timer_banner($timer),
    ));
    backdrop_exit();
  }

  backdrop_set_message(t('Timer started at @time.', array('@time' => $now->format('g:i A'))));
  backdrop_goto('drop/time');
}

/**
 * Page callback: stop the timer and pre-fill the add form.
 */
function droptime_timer_stop() {
  global $user;

  $timer = _droptime_get_timer($user->uid);
  state_del('droptime_timer_' . $user->uid);

  if (!$timer) {
    backdrop_goto('drop/time');
  }

  $site_tz = new DateTimeZone(config_get('system.date', 'default_timezone') ?: date_default_timezone_get());
  $end     = (new DateTime('now', $site_tz))->format('H:i');

  backdrop_goto('drop/time', array('query' => array(
    'date'  => $timer['date'],
    'start' => $timer['start'],
    'end'   => $end,
  )));
}

/**
 * Renders the timer running banner.
 */
function _droptime_timer_banner($timer) {
  $html  = '<div class="droptime-timer-running" data-timer-start="' . (int) $timer['ts'] . '">';
  $html .= icon('timer', array('attributes' => array('width' => '16', 'height' => '16')));
  $html .= ' ' . t('Timer started at @time', array('@time' => check_plain($timer['start'])));
  $html .= ' — <span class="droptime-elapsed"></span>';
  $html .= ' ' . l(t('■ Stop & log'), 'drop/time/timer/stop', array('attributes' => array('class' => array('droptime-timer-stop'))));
  $html .= '</div>';
  return $html;
}

// ---------------------------------------------------------------------------
// Reports
// ---------------------------------------------------------------------------

/**
 * Page callback: Reports parent tab — delegates to personal reports.
 */
function droptime_reports_page() {
  return droptime_report_personal();
}

/**
 * Page callback: personal time reports.
 */
function droptime_report_personal() {
  global $user;

  backdrop_set_title(t('Time Reports'));

  $period = (isset($_GET['period']) && in_array($_GET['period'], array('day', 'week', 'month')))
    ? $_GET['period']
    : 'week';

  $site_tz = new DateTimeZone(config_get('system.date', 'default_timezone') ?: date_default_timezone_get());
  $today   = (new DateTime('now', $site_tz))->format('Y-m-d');
  $date    = (isset($_GET['date']) && preg_match('/^\d{4}-\d{2}-\d{2}$/', $_GET['date']))
    ? $_GET['date']
    : $today;

  // Determine date range and labels.
  $ts = strtotime($date);
  switch ($period) {
    case 'day':
      $range_start = $date;
      $range_end   = $date;
      $label       = format_date($ts, 'custom', 'l, F j, Y');
      $prev_date   = date('Y-m-d', strtotime('-1 day', $ts));
      $next_date   = date('Y-m-d', strtotime('+1 day', $ts));
      break;

    case 'month':
      $range_start = date('Y-m-01', $ts);
      $range_end   = date('Y-m-t', $ts);
      $label       = format_date($ts, 'custom', 'F Y');
      $prev_date   = date('Y-m-d', strtotime('-1 month', $ts));
      $next_date   = date('Y-m-d', strtotime('+1 month', $ts));
      break;

    case 'week':
    default:
      $period    = 'week';
      $dow       = (int) date('N', $ts); // 1=Mon
      $range_start = date('Y-m-d', strtotime('-' . ($dow - 1) . ' days', $ts));
      $range_end   = date('Y-m-d', strtotime('+' . (7 - $dow) . ' days', $ts));
      $label       = t('Week of @s – @e', array(
        '@s' => format_date(strtotime($range_start), 'custom', 'M j'),
        '@e' => format_date(strtotime($range_end),   'custom', 'M j, Y'),
      ));
      $prev_date = date('Y-m-d', strtotime('-7 days', $ts));
      $next_date = date('Y-m-d', strtotime('+7 days', $ts));
      break;
  }

  // Load entries in range.
  $rows = db_select('drop_time_entry', 'e')
    ->fields('e')
    ->condition('e.uid', $user->uid)
    ->condition('e.entry_date', $range_start, '>=')
    ->condition('e.entry_date', $range_end,   '<=')
    ->orderBy('e.entry_date', 'ASC')
    ->orderBy('e.start_time', 'ASC')
    ->execute()
    ->fetchAll();

  // Aggregate by project.
  $project_totals = array();
  $grand_total    = 0;
  $grand_billable = 0;

  foreach ($rows as $row) {
    $pid = $row->pid;
    if (!isset($project_totals[$pid])) {
      $project_totals[$pid] = array('total' => 0, 'billable' => 0);
    }
    $project_totals[$pid]['total']    += $row->duration;
    $project_totals[$pid]['billable'] += $row->billable ? $row->duration : 0;
    $grand_total    += $row->duration;
    $grand_billable += $row->billable ? $row->duration : 0;
  }

  // Sort by total descending.
  uasort($project_totals, function ($a, $b) { return $b['total'] - $a['total']; });

  // Load projects.
  $pids     = array_keys($project_totals);
  $projects = $pids ? entity_load('drop_project', $pids) : array();

  // Build output.
  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'droptime') . '/droptime.css'),
    ),
  );

  // Period tabs.
  $period_links = array();
  foreach (array('day' => t('Day'), 'week' => t('Week'), 'month' => t('Month')) as $p => $plabel) {
    $attr = $p === $period ? array('class' => array('droptime-period-active')) : array();
    $period_links[] = l($plabel, 'drop/time/reports', array(
      'query'      => array('period' => $p, 'date' => $date),
      'attributes' => $attr,
    ));
  }
  $output['periods'] = array(
    '#markup' => '<div class="droptime-period-tabs">' . implode(' ', $period_links) . '</div>',
    '#weight' => -10,
  );

  // Date nav.
  $html_nav  = '<div class="droptime-date-nav">';
  $html_nav .= l('‹', 'drop/time/reports', array('query' => array('period' => $period, 'date' => $prev_date), 'attributes' => array('class' => array('droptime-nav-arrow'))));
  $html_nav .= '<span class="droptime-nav-label">' . check_plain($label) . '</span>';
  $html_nav .= l('›', 'drop/time/reports', array('query' => array('period' => $period, 'date' => $next_date), 'attributes' => array('class' => array('droptime-nav-arrow'))));
  $html_nav .= '</div>';
  $output['nav'] = array('#markup' => $html_nav, '#weight' => -5);

  if (empty($project_totals)) {
    $output['empty'] = array(
      '#markup' => '<p class="droptime-no-entries">' . t('No time logged for this period.') . '</p>',
    );
    return $output;
  }

  // Report table.
  $table_rows = array();
  foreach ($project_totals as $pid => $data) {
    $project = isset($projects[$pid]) ? $projects[$pid] : NULL;
    $color   = $project ? check_plain($project->color) : '#ccc';
    $name    = $project ? check_plain($project->name)  : t('(unknown)');
    $swatch  = '<span class="droptime-swatch" style="background:' . $color . ';"></span>';
    $pct     = $grand_total > 0 ? round(($data['total'] / $grand_total) * 100) : 0;

    $table_rows[] = array(
      array('data' => $swatch . ' ' . $name, 'class' => array('droptime-report-project')),
      droptime_format_duration($data['total']),
      droptime_format_duration($data['billable']),
      $pct . '%',
    );
  }

  // Totals row.
  $table_rows[] = array(
    array('data' => '<strong>' . t('Total') . '</strong>'),
    array('data' => '<strong>' . droptime_format_duration($grand_total)    . '</strong>'),
    array('data' => '<strong>' . droptime_format_duration($grand_billable) . '</strong>'),
    '',
  );

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Project'), t('Total'), t('Billable'), t('%')),
    '#rows'   => $table_rows,
    '#attributes' => array('class' => array('droptime-report-table')),
  );

  return $output;
}

// ---------------------------------------------------------------------------
// Team Reports
// ---------------------------------------------------------------------------

/**
 * Page callback: team-wide time reports for admins/project managers.
 */
function droptime_team_reports_page() {
  backdrop_set_title(t('Team Reports'));

  $site_tz = new DateTimeZone(config_get('system.date', 'default_timezone') ?: date_default_timezone_get());
  $today   = (new DateTime('now', $site_tz))->format('Y-m-d');

  // Period.
  $period = (isset($_GET['period']) && in_array($_GET['period'], array('day', 'week', 'month')))
    ? $_GET['period']
    : 'week';

  // Anchor date.
  $date = (isset($_GET['date']) && preg_match('/^\d{4}-\d{2}-\d{2}$/', $_GET['date']))
    ? $_GET['date']
    : $today;

  // Filters.
  $filter_uid = (isset($_GET['uid']) && is_numeric($_GET['uid'])) ? (int) $_GET['uid'] : 0;
  $filter_pid = (isset($_GET['pid']) && is_numeric($_GET['pid'])) ? (int) $_GET['pid'] : 0;
  $group_by   = (isset($_GET['group']) && $_GET['group'] === 'person') ? 'person' : 'project';

  // Compute date range.
  $ts = strtotime($date);
  switch ($period) {
    case 'day':
      $range_start = $date;
      $range_end   = $date;
      $label       = format_date($ts, 'custom', 'l, F j, Y');
      $prev_date   = date('Y-m-d', strtotime('-1 day', $ts));
      $next_date   = date('Y-m-d', strtotime('+1 day', $ts));
      break;

    case 'month':
      $range_start = date('Y-m-01', $ts);
      $range_end   = date('Y-m-t', $ts);
      $label       = format_date($ts, 'custom', 'F Y');
      $prev_date   = date('Y-m-d', strtotime('-1 month', $ts));
      $next_date   = date('Y-m-d', strtotime('+1 month', $ts));
      break;

    case 'week':
    default:
      $period      = 'week';
      $dow         = (int) date('N', $ts);
      $range_start = date('Y-m-d', strtotime('-' . ($dow - 1) . ' days', $ts));
      $range_end   = date('Y-m-d', strtotime('+' . (7 - $dow) . ' days', $ts));
      $label       = t('Week of @s – @e', array(
        '@s' => format_date(strtotime($range_start), 'custom', 'M j'),
        '@e' => format_date(strtotime($range_end),   'custom', 'M j, Y'),
      ));
      $prev_date   = date('Y-m-d', strtotime('-7 days', $ts));
      $next_date   = date('Y-m-d', strtotime('+7 days', $ts));
      break;
  }

  // Base query params carried through all nav links.
  $base_query = array('period' => $period, 'date' => $date, 'uid' => $filter_uid, 'pid' => $filter_pid, 'group' => $group_by);

  // Build output.
  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'droptime') . '/droptime.css'),
    ),
  );

  // Period tabs.
  $period_links = array();
  foreach (array('day' => t('Day'), 'week' => t('Week'), 'month' => t('Month')) as $p => $plabel) {
    $attr = $p === $period ? array('class' => array('droptime-period-active')) : array();
    $period_links[] = l($plabel, 'drop/time/reports/team', array(
      'query'      => array('period' => $p, 'date' => $date, 'uid' => $filter_uid, 'pid' => $filter_pid, 'group' => $group_by),
      'attributes' => $attr,
    ));
  }
  $output['periods'] = array(
    '#markup' => '<div class="droptime-period-tabs">' . implode(' ', $period_links) . '</div>',
    '#weight' => -20,
  );

  // Date nav.
  $html_nav  = '<div class="droptime-date-nav">';
  $html_nav .= l('‹', 'drop/time/reports/team', array('query' => array_merge($base_query, array('date' => $prev_date)), 'attributes' => array('class' => array('droptime-nav-arrow'))));
  $html_nav .= '<span class="droptime-nav-label">' . check_plain($label) . '</span>';
  $html_nav .= l('›', 'drop/time/reports/team', array('query' => array_merge($base_query, array('date' => $next_date)), 'attributes' => array('class' => array('droptime-nav-arrow'))));
  $html_nav .= '</div>';
  $output['nav'] = array('#markup' => $html_nav, '#weight' => -15);

  // Filter bar.
  $output['filters'] = array(
    '#markup' => _droptime_team_filter_bar($filter_uid, $filter_pid, $group_by, $period, $date),
    '#weight' => -10,
  );

  // Load entries.
  $query = db_select('drop_time_entry', 'e')->fields('e')
    ->condition('e.entry_date', $range_start, '>=')
    ->condition('e.entry_date', $range_end,   '<=')
    ->orderBy('e.entry_date', 'ASC')
    ->orderBy('e.start_time', 'ASC');
  if ($filter_uid) {
    $query->condition('e.uid', $filter_uid);
  }
  if ($filter_pid) {
    $query->condition('e.pid', $filter_pid);
  }
  $rows = $query->execute()->fetchAll();

  if (empty($rows)) {
    $output['empty'] = array(
      '#markup' => '<p class="droptime-no-entries">' . t('No time logged for this period.') . '</p>',
      '#weight' => 0,
    );
    return $output;
  }

  // Pre-load all projects and users referenced in entries.
  $pids     = array_unique(array_column((array) $rows, 'pid'));
  $uids     = array_unique(array_column((array) $rows, 'uid'));
  $projects = $pids ? entity_load('drop_project', $pids) : array();
  $users    = $uids ? user_load_multiple($uids) : array();

  // Group-by toggle.
  $group_links  = l(t('By Project'), 'drop/time/reports/team', array(
    'query'      => array_merge($base_query, array('group' => 'project')),
    'attributes' => $group_by === 'project' ? array('class' => array('droptime-period-active')) : array(),
  ));
  $group_links .= ' ';
  $group_links .= l(t('By Person'), 'drop/time/reports/team', array(
    'query'      => array_merge($base_query, array('group' => 'person')),
    'attributes' => $group_by === 'person' ? array('class' => array('droptime-period-active')) : array(),
  ));
  $output['group_toggle'] = array(
    '#markup' => '<div class="droptime-period-tabs droptime-group-toggle">' . $group_links . '</div>',
    '#weight' => -5,
  );

  // Render report.
  if ($group_by === 'person') {
    $output['report'] = array(
      '#markup' => _droptime_team_report_by_person($rows, $projects, $users),
      '#weight' => 0,
    );
  }
  else {
    $output['report'] = array(
      '#markup' => _droptime_team_report_by_project($rows, $projects, $users),
      '#weight' => 0,
    );
  }

  return $output;
}

/**
 * Renders the filter bar for team reports.
 */
function _droptime_team_filter_bar($filter_uid, $filter_pid, $group_by, $period, $date) {
  // All users who have ever logged time.
  $uids = db_select('drop_time_entry', 'e')
    ->fields('e', array('uid'))
    ->groupBy('e.uid')
    ->execute()
    ->fetchCol();
  $team = $uids ? user_load_multiple($uids) : array();

  // All active projects.
  $projects = droptime_project_options();

  $member_options = '<option value="0"' . ($filter_uid === 0 ? ' selected' : '') . '>' . t('All members') . '</option>';
  foreach ($team as $u) {
    $sel = ($filter_uid === (int) $u->uid) ? ' selected' : '';
    $member_options .= '<option value="' . (int) $u->uid . '"' . $sel . '>' . check_plain($u->name) . '</option>';
  }

  $proj_options = '<option value="0"' . ($filter_pid === 0 ? ' selected' : '') . '>' . t('All projects') . '</option>';
  foreach ($projects as $pid => $name) {
    $sel = ($filter_pid === (int) $pid) ? ' selected' : '';
    $proj_options .= '<option value="' . (int) $pid . '"' . $sel . '>' . check_plain($name) . '</option>';
  }

  $action = url('drop/time/reports/team');
  $html   = '<form class="droptime-filter-bar" method="get" action="' . $action . '">';
  $html  .= '<input type="hidden" name="period" value="' . check_plain($period) . '">';
  $html  .= '<input type="hidden" name="date" value="' . check_plain($date) . '">';
  $html  .= '<input type="hidden" name="group" value="' . check_plain($group_by) . '">';
  $html  .= '<label>' . t('Member') . ' <select name="uid" onchange="this.form.submit()">' . $member_options . '</select></label>';
  $html  .= ' <label>' . t('Project') . ' <select name="pid" onchange="this.form.submit()">' . $proj_options . '</select></label>';
  $html  .= '<noscript><button type="submit">' . t('Filter') . '</button></noscript>';
  $html  .= '</form>';

  return $html;
}

/**
 * Renders the team report grouped by project.
 */
function _droptime_team_report_by_project($rows, $projects, $users) {
  $groups      = array();
  $grand_total = 0;
  $grand_bill  = 0;

  foreach ($rows as $row) {
    $pid = $row->pid;
    if (!isset($groups[$pid])) {
      $groups[$pid] = array('total' => 0, 'billable' => 0, 'entries' => array());
    }
    $groups[$pid]['total']    += $row->duration;
    $groups[$pid]['billable'] += $row->billable ? $row->duration : 0;
    $groups[$pid]['entries'][] = $row;
    $grand_total += $row->duration;
    $grand_bill  += $row->billable ? $row->duration : 0;
  }
  uasort($groups, function($a, $b) { return $b['total'] - $a['total']; });

  // Summary table.
  $table_rows = array();
  foreach ($groups as $pid => $data) {
    $project = isset($projects[$pid]) ? $projects[$pid] : NULL;
    $color   = $project ? check_plain($project->color) : '#ccc';
    $name    = $project ? check_plain($project->name)  : t('(unknown)');
    $swatch  = '<span class="droptime-swatch" style="background:' . $color . ';"></span>';
    $pct     = $grand_total > 0 ? round(($data['total'] / $grand_total) * 100) : 0;
    $table_rows[] = array(
      array('data' => $swatch . ' ' . $name, 'class' => array('droptime-report-project')),
      droptime_format_duration($data['total']),
      droptime_format_duration($data['billable']),
      $pct . '%',
    );
  }
  $table_rows[] = array(
    array('data' => '<strong>' . t('Total') . '</strong>'),
    array('data' => '<strong>' . droptime_format_duration($grand_total) . '</strong>'),
    array('data' => '<strong>' . droptime_format_duration($grand_bill) . '</strong>'),
    '',
  );

  $html = theme('table', array(
    'header'     => array(t('Project'), t('Total'), t('Billable'), t('%')),
    'rows'       => $table_rows,
    'attributes' => array('class' => array('droptime-report-table')),
  ));

  // Per-project accordions.
  foreach ($groups as $pid => $data) {
    $project = isset($projects[$pid]) ? $projects[$pid] : NULL;
    $color   = $project ? check_plain($project->color) : '#ccc';
    $name    = $project ? check_plain($project->name)  : t('(unknown)');
    $swatch  = '<span class="droptime-swatch" style="background:' . $color . ';"></span>';
    $count   = count($data['entries']);

    $html .= '<details class="droptime-team-accordion">';
    $html .= '<summary class="droptime-team-accordion-summary">'
      . $swatch . ' ' . $name
      . ' <span class="droptime-team-accordion-count">'
      . format_plural($count, '1 entry', '@count entries') . ' &middot; ' . droptime_format_duration($data['total'])
      . '</span></summary>';
    $html .= _droptime_team_entries_table($data['entries'], $projects, $users, 'person');
    $html .= '</details>';
  }

  return $html;
}

/**
 * Renders the team report grouped by person.
 */
function _droptime_team_report_by_person($rows, $projects, $users) {
  $groups      = array();
  $grand_total = 0;
  $grand_bill  = 0;

  foreach ($rows as $row) {
    $uid = $row->uid;
    if (!isset($groups[$uid])) {
      $groups[$uid] = array('total' => 0, 'billable' => 0, 'entries' => array());
    }
    $groups[$uid]['total']    += $row->duration;
    $groups[$uid]['billable'] += $row->billable ? $row->duration : 0;
    $groups[$uid]['entries'][] = $row;
    $grand_total += $row->duration;
    $grand_bill  += $row->billable ? $row->duration : 0;
  }
  uasort($groups, function($a, $b) { return $b['total'] - $a['total']; });

  // Summary table.
  $table_rows = array();
  foreach ($groups as $uid => $data) {
    $name = isset($users[$uid]) ? check_plain($users[$uid]->name) : t('(unknown)');
    $pct  = $grand_total > 0 ? round(($data['total'] / $grand_total) * 100) : 0;
    $table_rows[] = array(
      $name,
      droptime_format_duration($data['total']),
      droptime_format_duration($data['billable']),
      $pct . '%',
    );
  }
  $table_rows[] = array(
    array('data' => '<strong>' . t('Total') . '</strong>'),
    array('data' => '<strong>' . droptime_format_duration($grand_total) . '</strong>'),
    array('data' => '<strong>' . droptime_format_duration($grand_bill) . '</strong>'),
    '',
  );

  $html = theme('table', array(
    'header'     => array(t('Person'), t('Total'), t('Billable'), t('%')),
    'rows'       => $table_rows,
    'attributes' => array('class' => array('droptime-report-table')),
  ));

  // Per-person accordions.
  foreach ($groups as $uid => $data) {
    $name  = isset($users[$uid]) ? check_plain($users[$uid]->name) : t('(unknown)');
    $count = count($data['entries']);

    $html .= '<details class="droptime-team-accordion">';
    $html .= '<summary class="droptime-team-accordion-summary">'
      . $name
      . ' <span class="droptime-team-accordion-count">'
      . format_plural($count, '1 entry', '@count entries') . ' &middot; ' . droptime_format_duration($data['total'])
      . '</span></summary>';
    $html .= _droptime_team_entries_table($data['entries'], $projects, $users, 'project');
    $html .= '</details>';
  }

  return $html;
}

/**
 * Renders a table of individual entries inside a team accordion.
 *
 * @param array $entries
 *   Time entry rows.
 * @param array $projects
 *   Loaded project entities keyed by pid.
 * @param array $users
 *   Loaded user objects keyed by uid.
 * @param string $show_col
 *   'person' to show the person column, 'project' to show the project column.
 */
function _droptime_team_entries_table($entries, $projects, $users, $show_col) {
  $rows = array();
  foreach ($entries as $entry) {
    $project   = isset($projects[$entry->pid]) ? $projects[$entry->pid] : NULL;
    $color     = $project ? check_plain($project->color) : '#ccc';
    $swatch    = '<span class="droptime-swatch" style="background:' . $color . ';"></span>';
    $proj_name = $project ? check_plain($project->name) : t('(unknown)');
    $person    = isset($users[$entry->uid]) ? check_plain($users[$entry->uid]->name) : t('(unknown)');
    $bill      = $entry->billable
      ? '<span class="droptime-bill droptime-bill--yes" title="' . t('Billable') . '">$</span>'
      : '';

    $row   = array();
    $row[] = format_date(strtotime($entry->entry_date), 'custom', 'M j');
    $row[] = check_plain($entry->start_time) . '–' . check_plain($entry->end_time);
    $row[] = ($show_col === 'project') ? $swatch . ' ' . $proj_name : $person;
    $row[] = check_plain($entry->notes);
    $row[] = $bill . ' ' . droptime_format_duration($entry->duration);
    $rows[] = $row;
  }

  $header   = array(t('Date'), t('Time'));
  $header[] = $show_col === 'project' ? t('Project') : t('Person');
  $header[] = t('Note');
  $header[] = t('Duration');

  return theme('table', array(
    'header'     => $header,
    'rows'       => $rows,
    'attributes' => array('class' => array('droptime-team-entries-table')),
  ));
}
