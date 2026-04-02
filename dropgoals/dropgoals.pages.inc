<?php
/**
 * @file
 * Page callbacks for DropGoals.
 */

// ---------------------------------------------------------------------------
// Dashboard
// ---------------------------------------------------------------------------

/**
 * Page callback: today's goal dashboard.
 *
 * @return array
 *   A render array.
 */
function dropgoals_dashboard() {
  global $user;

  backdrop_set_title(t('Goals'));

  $today = date('Y-m-d');

  // Load all active goals for this user.
  $gids = db_select('drop_goal', 'g')
    ->fields('g', array('gid'))
    ->condition('g.uid', $user->uid)
    ->condition('g.active', 1)
    ->orderBy('g.label', 'ASC')
    ->execute()
    ->fetchCol();

  $all_goals = $gids ? entity_load('drop_goal', $gids) : array();

  // Filter to goals that are due today and not yet completed for their period.
  $goals = array();
  foreach ($all_goals as $gid => $goal) {
    if (!dropgoals_goal_is_due($goal, $today)) {
      continue; // Not a due day (e.g. every_other_day off-day).
    }
    $frequency = isset($goal->frequency) ? $goal->frequency : 'daily';
    if ($frequency === 'weekly' || $frequency === 'monthly') {
      if (_dropgoals_period_completed($goal, $today)) {
        continue; // Already done for this week/month.
      }
    }
    $goals[$gid] = $goal;
  }

  // Load today's log entries keyed by gid for visible goals only.
  $log = array();
  if ($goals) {
    $visible_gids = array_keys($goals);
    $rows = db_select('drop_goal_log', 'l')
      ->fields('l')
      ->condition('l.gid', $visible_gids, 'IN')
      ->condition('l.log_date', $today)
      ->execute()
      ->fetchAllAssoc('gid');
    $log = $rows;
  }

  $output = array(
    '#theme'  => 'dropgoals_dashboard',
    '#goals'  => $goals,
    '#log'    => $log,
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropgoals') . '/dropgoals.css'),
    ),
  );

  return $output;
}

/**
 * Theme function: render the daily goal dashboard.
 *
 * @param array $variables
 *   - goals: array of DropGoal entities (visible goals for today).
 *   - log: array of log rows keyed by gid for today.
 *
 * @return string
 *   Rendered HTML.
 */
function theme_dropgoals_dashboard($variables) {
  $goals = $variables['goals'];
  $log   = $variables['log'];
  $today = date('l, F j, Y');
  $freq_labels = dropgoals_frequency_labels();

  if (empty($goals)) {
    $add_link = l(t('Add your first goal'), 'drop/goals/manage/add');
    return '<p>' . t('No active goals. !link.', array('!link' => $add_link)) . '</p>';
  }

  $output = '<h2 class="dropgoals-date">' . check_plain($today) . '</h2>';
  $output .= '<ul class="dropgoals-dashboard-list">';

  foreach ($goals as $gid => $goal) {
    $completed = isset($log[$gid]) && $log[$gid]->completed;
    $item_class = 'dropgoals-item' . ($completed ? ' dropgoals-item--done' : '');

    $toggle_url = 'drop/goals/toggle/' . $gid;
    $toggle_label = $completed ? t('Mark incomplete') : t('Mark done');

    $icon = $completed
      ? '<span class="dropgoals-check dropgoals-check--done" aria-hidden="true">&#10003;</span>'
      : '<span class="dropgoals-check dropgoals-check--empty" aria-hidden="true">&#9675;</span>';

    $label = check_plain($goal->label);
    $frequency = isset($goal->frequency) ? $goal->frequency : 'daily';
    $freq_label = ($frequency !== 'daily' && isset($freq_labels[$frequency])) ? $freq_labels[$frequency] : '';
    $history_link = l(t('history'), 'drop/goals/manage/' . $gid, array('attributes' => array('class' => array('dropgoals-history-link'))));

    $output .= '<li class="' . $item_class . '">';
    $output .= '<span class="dropgoals-item-icon">' . $icon . '</span>';
    $output .= '<span class="dropgoals-item-label">' . $label;
    if ($freq_label) {
      $output .= ' <span class="dropgoals-item-freq">' . check_plain($freq_label) . '</span>';
    }
    $output .= '</span>';
    $output .= '<span class="dropgoals-item-actions">';
    $output .= l($toggle_label, $toggle_url, array('attributes' => array('class' => array('dropgoals-toggle'))));
    $output .= ' &nbsp;·&nbsp; ' . $history_link;
    $output .= '</span>';
    $output .= '</li>';
  }

  $output .= '</ul>';

  // Summary count.
  $done_count  = count(array_filter((array) $log, function($entry) { return $entry->completed; }));
  $total_count = count($goals);
  $output .= '<p class="dropgoals-summary">' . t('@done of @total goals completed today.', array('@done' => $done_count, '@total' => $total_count)) . '</p>';

  return $output;
}

// ---------------------------------------------------------------------------
// Toggle
// ---------------------------------------------------------------------------

/**
 * Page callback: toggle today's completion for a goal.
 *
 * @param DropGoal $goal
 *   The goal entity.
 */
function dropgoals_toggle_today(DropGoal $goal) {
  global $user;
  $today = date('Y-m-d');

  $existing = db_select('drop_goal_log', 'l')
    ->fields('l')
    ->condition('l.gid', $goal->gid)
    ->condition('l.log_date', $today)
    ->execute()
    ->fetchObject();

  if ($existing) {
    $new_value = $existing->completed ? 0 : 1;
    db_update('drop_goal_log')
      ->fields(array('completed' => $new_value))
      ->condition('lid', $existing->lid)
      ->execute();
    $label = $new_value ? t('marked done') : t('marked incomplete');
  }
  else {
    db_insert('drop_goal_log')
      ->fields(array(
        'gid'      => $goal->gid,
        'log_date' => $today,
        'completed' => 1,
        'uid'      => $user->uid,
        'created'  => REQUEST_TIME,
      ))
      ->execute();
    $label = t('marked done');
  }

  backdrop_set_message(t('%label @status.', array('%label' => $goal->label, '@status' => $label)));
  backdrop_goto('drop/goals');
}

/**
 * Page callback: toggle completion for a specific date.
 *
 * Allows retroactive logging — e.g. marking yesterday complete if you forgot.
 * Rejects future dates.
 *
 * @param DropGoal $goal
 *   The goal entity.
 * @param string $date
 *   Date string in 'Y-m-d' format.
 */
function dropgoals_toggle_date(DropGoal $goal, $date) {
  global $user;

  // Validate format.
  if (!preg_match('/^\d{4}-\d{2}-\d{2}$/', $date) || !strtotime($date)) {
    backdrop_set_message(t('Invalid date.'), 'error');
    backdrop_goto('drop/goals/manage/' . $goal->gid);
  }

  $today = date('Y-m-d');
  if ($date > $today) {
    backdrop_set_message(t('Cannot log future dates.'), 'warning');
    backdrop_goto('drop/goals/manage/' . $goal->gid);
  }

  $existing = db_select('drop_goal_log', 'l')
    ->fields('l')
    ->condition('l.gid', $goal->gid)
    ->condition('l.log_date', $date)
    ->execute()
    ->fetchObject();

  if ($existing) {
    $new_value = $existing->completed ? 0 : 1;
    db_update('drop_goal_log')
      ->fields(array('completed' => $new_value))
      ->condition('lid', $existing->lid)
      ->execute();
    $label = $new_value ? t('marked done') : t('marked incomplete');
  }
  else {
    db_insert('drop_goal_log')
      ->fields(array(
        'gid'       => $goal->gid,
        'log_date'  => $date,
        'completed' => 1,
        'uid'       => $user->uid,
        'created'   => REQUEST_TIME,
      ))
      ->execute();
    $label = t('marked done');
  }

  $date_label = format_date(strtotime($date), 'custom', 'M j, Y');
  backdrop_set_message(t('%goal on @date @status.', array(
    '%goal'  => $goal->label,
    '@date'  => $date_label,
    '@status' => $label,
  )));
  backdrop_goto('drop/goals/manage/' . $goal->gid);
}

// ---------------------------------------------------------------------------
// Goal history
// ---------------------------------------------------------------------------

/**
 * Page callback: view a goal's details and completion history.
 *
 * @param DropGoal $goal
 *   The goal entity.
 *
 * @return array
 *   A render array.
 */
function dropgoals_goal_history(DropGoal $goal) {
  backdrop_set_title(check_plain($goal->label));

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropgoals') . '/dropgoals.css'),
    ),
  );

  // Actions bar.
  $actions = array();
  if (dropgoals_goal_access('update', $goal)) {
    $actions[] = l(t('Edit goal'), 'drop/goals/manage/' . $goal->gid . '/edit');
  }
  if (dropgoals_goal_access('delete', $goal)) {
    $actions[] = l(t('Delete'), 'drop/goals/manage/' . $goal->gid . '/delete');
  }
  $output['actions'] = array(
    '#markup' => drop_base_page_actions($actions),
    '#weight' => -100,
  );

  // Goal details.
  $freq_labels = dropgoals_frequency_labels();
  $frequency = isset($goal->frequency) ? $goal->frequency : 'daily';
  $rows = array();
  $rows[] = array(t('Status'), $goal->active ? t('Active') : t('Inactive'));
  $rows[] = array(t('Frequency'), isset($freq_labels[$frequency]) ? $freq_labels[$frequency] : t('Every day'));

  $day_names = array(
    '1' => t('Mon'), '2' => t('Tue'), '3' => t('Wed'), '4' => t('Thu'),
    '5' => t('Fri'), '6' => t('Sat'), '7' => t('Sun'),
  );
  if ($frequency === 'weekly' && !empty($goal->days_of_week)) {
    $day_num = trim($goal->days_of_week);
    if (isset($day_names[$day_num])) {
      $rows[] = array(t('Target day'), t('Due by @day each week', array('@day' => $day_names[$day_num])));
    }
  }
  elseif ($frequency === 'specific_days' && !empty($goal->days_of_week)) {
    $selected = explode(',', $goal->days_of_week);
    $labels = array();
    foreach ($selected as $d) {
      if (isset($day_names[$d])) {
        $labels[] = $day_names[$d];
      }
    }
    $rows[] = array(t('Active days'), implode(', ', $labels));
  }

  if (!empty($goal->description)) {
    $rows[] = array(t('Description'), check_plain($goal->description));
  }
  $rows[] = array(t('Created'), format_date($goal->created, 'custom', 'M j, Y'));

  $output['details_heading'] = array(
    '#markup' => drop_base_section_header(t('Details')),
    '#weight' => 0,
  );
  $output['details'] = array(
    '#theme'      => 'table',
    '#rows'       => $rows,
    '#attributes' => array('class' => array('dropgoals-detail-table')),
    '#weight'     => 1,
  );

  // History: last 35 days (5 weeks).
  $output['history_heading'] = array(
    '#markup' => drop_base_section_header(t('History')),
    '#weight' => 10,
  );
  $output['history'] = array(
    '#markup' => _dropgoals_render_history($goal),
    '#weight' => 11,
  );

  return $output;
}

// ---------------------------------------------------------------------------
// History helpers
// ---------------------------------------------------------------------------

/**
 * Returns TRUE if a weekly or monthly goal has been completed for its current
 * period (Mon–Sun for weekly; calendar month for monthly).
 *
 * Returns FALSE for all other frequency types.
 *
 * @param DropGoal $goal
 * @param string $date
 *   Reference date in 'Y-m-d' format (typically today).
 *
 * @return bool
 */
function _dropgoals_period_completed(DropGoal $goal, $date) {
  $frequency = isset($goal->frequency) ? $goal->frequency : 'daily';

  if ($frequency !== 'weekly') {
    return FALSE;
  }

  $ts  = strtotime($date);
  $dow = (int) date('N', $ts); // 1 = Mon, 7 = Sun
  $period_start = date('Y-m-d', strtotime('-' . ($dow - 1) . ' days', $ts));

  $count = db_select('drop_goal_log', 'l')
    ->condition('l.gid', $goal->gid)
    ->condition('l.log_date', $period_start, '>=')
    ->condition('l.log_date', $date, '<=')
    ->condition('l.completed', 1)
    ->countQuery()
    ->execute()
    ->fetchField();

  return (int) $count > 0;
}

/**
 * Renders a completion history grid and stats for a goal.
 *
 * Grid always shows 35 days (5 weeks). Stats are frequency-aware:
 * - daily/every_other_day: streak + 30-day rate.
 * - weekly: completed weeks out of last 12.
 * - monthly: completed months out of last 6.
 *
 * @param DropGoal $goal
 *   The goal entity.
 *
 * @return string
 *   HTML string.
 */
function _dropgoals_render_history(DropGoal $goal) {
  $today = date('Y-m-d');
  $frequency = isset($goal->frequency) ? $goal->frequency : 'daily';

  // Load enough history for both the grid and frequency-specific stats.
  $stats_days_back = ($frequency === 'weekly') ? 91 : 35; // weekly: 13 weeks

  $log_start = date('Y-m-d', strtotime('-' . ($stats_days_back - 1) . ' days'));
  $log_rows = db_select('drop_goal_log', 'l')
    ->fields('l', array('log_date', 'completed'))
    ->condition('l.gid', $goal->gid)
    ->condition('l.log_date', $log_start, '>=')
    ->condition('l.log_date', $today, '<=')
    ->execute()
    ->fetchAllKeyed();

  // ---------- Stats ----------
  $html = '<div class="dropgoals-stats">';

  if ($frequency === 'daily' || $frequency === 'every_other_day' || $frequency === 'specific_days') {
    // Streak: consecutive completed due-days going back from today.
    $streak = 0;
    $check_date = $today;
    // If today isn't logged yet, start streak check from yesterday.
    if (!isset($log_rows[$today]) || !$log_rows[$today]) {
      $check_date = date('Y-m-d', strtotime('-1 day'));
    }
    $guard = 0;
    while ($guard++ < 400) {
      // Skip non-due days silently so they don't break the streak.
      if ($frequency !== 'daily' && !dropgoals_goal_is_due($goal, $check_date)) {
        $check_date = date('Y-m-d', strtotime($check_date . ' -1 day'));
        continue;
      }
      if (isset($log_rows[$check_date]) && $log_rows[$check_date]) {
        $streak++;
        $check_date = date('Y-m-d', strtotime($check_date . ' -1 day'));
      }
      else {
        break;
      }
    }

    // Completion rate: look back min(days since creation, 30) days, counting
    // only due days.
    $total_due = 0;
    $completed_due = 0;
    $days_since_creation = (int) floor((strtotime($today) - $goal->created) / 86400);
    $lookback = max(1, min(30, $days_since_creation));
    for ($i = 1; $i <= $lookback; $i++) {
      $d = date('Y-m-d', strtotime("-$i days"));
      if ($frequency !== 'daily' && !dropgoals_goal_is_due($goal, $d)) {
        continue;
      }
      $total_due++;
      if (isset($log_rows[$d]) && $log_rows[$d]) {
        $completed_due++;
      }
    }
    $rate = $total_due > 0 ? round(($completed_due / $total_due) * 100) : 0;
    $rate_label = ($lookback < 30)
      ? t('@n-day completion: @rate%', array('@n' => $lookback, '@rate' => $rate))
      : t('30-day completion: @rate%', array('@rate' => $rate));

    $streak_label = ($frequency === 'daily')
      ? t('Current streak: @n day(s)', array('@n' => $streak))
      : t('Current streak: @n due day(s)', array('@n' => $streak));
    $html .= '<span class="dropgoals-streak">' . $streak_label . '</span>';
    $html .= '<span class="dropgoals-rate">' . $rate_label . '</span>';
  }
  elseif ($frequency === 'weekly') {
    $today_ts          = strtotime($today);
    $today_dow         = (int) date('N', $today_ts);
    $cur_week_mon_ts   = strtotime('-' . ($today_dow - 1) . ' days', $today_ts);

    $total_past_weeks = 0;
    $completed_weeks  = 0;
    for ($w = 1; $w <= 12; $w++) {
      $ws_ts = strtotime('-' . ($w * 7) . ' days', $cur_week_mon_ts);
      // Stop if this week predates goal creation.
      if ($ws_ts < strtotime(date('Y-m-d', $goal->created))) {
        break;
      }
      $total_past_weeks++;
      for ($d = 0; $d < 7; $d++) {
        $day = date('Y-m-d', strtotime("+$d days", $ws_ts));
        if (isset($log_rows[$day]) && $log_rows[$day]) {
          $completed_weeks++;
          break;
        }
      }
    }
    $rate = $total_past_weeks > 0 ? round(($completed_weeks / $total_past_weeks) * 100) : 0;
    $html .= '<span class="dropgoals-streak">' . t('@done / @total past weeks', array('@done' => $completed_weeks, '@total' => $total_past_weeks)) . '</span>';
    $html .= '<span class="dropgoals-rate">' . t('@rate% completion rate', array('@rate' => $rate)) . '</span>';
  }

  $html .= '</div>'; // .dropgoals-stats

  // ---------- Grid ----------
  if ($frequency === 'weekly') {
    // Week-based grid: one cell per week, oldest left, newest right.
    $today_ts  = strtotime($today);
    $today_dow = (int) date('N', $today_ts);
    $cur_week_mon_ts  = strtotime('-' . ($today_dow - 1) . ' days', $today_ts);
    $cur_week_mon_str = date('Y-m-d', $cur_week_mon_ts);

    // Find the Monday of the goal's creation week.
    $creation_dow          = (int) date('N', $goal->created);
    $creation_week_mon_ts  = strtotime('-' . ($creation_dow - 1) . ' days', $goal->created);
    $creation_week_mon_str = date('Y-m-d', $creation_week_mon_ts);
    $creation_date_str     = date('Y-m-d', $goal->created);

    $html .= '<div class="dropgoals-week-grid">';

    // Target day number (1=Mon…7=Sun) for this goal.
    $target_day_num = !empty($goal->days_of_week) ? (int) $goal->days_of_week : 7;

    // Show up to 12 past weeks + current week + 1 upcoming week (oldest first).
    // w=12 is oldest past, w=0 is current, w=-1 is next week (upcoming).
    for ($w = 12; $w >= -1; $w--) {
      $week_mon_ts  = strtotime('-' . ($w * 7) . ' days', $cur_week_mon_ts);
      $week_sun_ts  = strtotime('+6 days', $week_mon_ts);
      $week_mon_str = date('Y-m-d', $week_mon_ts);
      $week_sun_str = date('Y-m-d', $week_sun_ts);

      // Skip weeks entirely before goal creation.
      if ($week_sun_str < $creation_date_str) {
        continue;
      }

      $is_upcoming  = ($w === -1);
      $is_current   = ($w === 0);
      $is_creation_week = ($week_mon_str === $creation_week_mon_str);
      // Partial = goal was created mid-week in a past week (not counted in stats).
      // Current week is always interactive even if the goal was created mid-week.
      $is_partial = !$is_current && $is_creation_week && ($creation_date_str > $week_mon_str);

      // The target date for this week (e.g. the Sunday for a Sunday goal).
      $target_date_ts = strtotime('+' . ($target_day_num - 1) . ' days', $week_mon_ts);
      $target_date    = date('Y-m-d', $target_date_ts);

      // Check for any completion in this week's days up to today.
      // Track the completion date so we can link back to it for toggling.
      $completed = FALSE;
      $completion_date = NULL;
      if (!$is_upcoming) {
        for ($d = 0; $d < 7; $d++) {
          $day = date('Y-m-d', strtotime("+$d days", $week_mon_ts));
          if ($day > $today) {
            break;
          }
          if (isset($log_rows[$day]) && $log_rows[$day]) {
            $completed = TRUE;
            $completion_date = $day;
            break;
          }
        }
      }

      // Determine the toggle URL for this week cell.
      $toggle_url = NULL;
      if (!$is_partial && !$is_upcoming) {
        if ($completed) {
          // Link to the day it was completed so it can be un-done.
          $toggle_url = url('drop/goals/toggle/' . $goal->gid . '/' . $completion_date);
        }
        elseif ($is_current) {
          // Current week: toggle today (target day may be in the future).
          $toggle_url = url('drop/goals/toggle/' . $goal->gid . '/' . $today);
        }
        elseif ($target_date <= $today) {
          // Past week: toggle the target day.
          $toggle_url = url('drop/goals/toggle/' . $goal->gid . '/' . $target_date);
        }
      }

      $class = 'dropgoals-week-cell';
      if ($is_upcoming) {
        $class .= ' dropgoals-week-upcoming';
      }
      elseif ($is_partial) {
        $class .= ' dropgoals-week-partial';
        if ($completed) {
          $class .= ' dropgoals-week-completed';
        }
      }
      elseif ($is_current) {
        $class .= ' dropgoals-week-current';
        if ($completed) {
          $class .= ' dropgoals-week-completed';
        }
      }
      elseif ($completed) {
        $class .= ' dropgoals-week-completed';
      }
      else {
        $class .= ' dropgoals-week-missed';
      }

      // Label each cell by the target day date (e.g. "Mar 22" for a Sunday goal)
      // so the cell reads as the due date rather than the week's Monday.
      $cell_label = format_date($target_date_ts, 'custom', 'M j');
      $mon_label  = format_date($week_mon_ts, 'custom', 'M j');
      $sun_label  = format_date($week_sun_ts, 'custom', 'M j');
      $title = $mon_label . '–' . $sun_label;
      if ($is_upcoming) {
        $title .= ' — ' . t('upcoming');
      }
      elseif ($is_partial) {
        $title .= ' — ' . t('goal started this week; not counted in stats');
      }
      elseif ($is_current) {
        $title .= ' — ' . t('current week');
      }

      $inner = '<span class="dropgoals-week-label">' . check_plain($cell_label) . '</span>';
      if ($toggle_url) {
        $title .= ' — ' . t('click to toggle');
        $html .= '<a href="' . $toggle_url . '" class="' . $class . '" title="' . check_plain($title) . '">' . $inner . '</a>';
      }
      else {
        $html .= '<div class="' . $class . '" title="' . check_plain($title) . '">' . $inner . '</div>';
      }
    }

    $html .= '</div>'; // .dropgoals-week-grid
  }
  else {
    // Day-based grid: 35 days (5 weeks), oldest top-left, newest bottom-right.
    $html .= '<div class="dropgoals-history-grid">';

    // Day-of-week headers (Mon–Sun).
    $day_labels = array(t('M'), t('T'), t('W'), t('T'), t('F'), t('S'), t('S'));
    foreach ($day_labels as $dl) {
      $html .= '<div class="dropgoals-history-header">' . check_plain($dl) . '</div>';
    }

    // Find the Monday that starts the 5-week window ending on Sunday ≥ today.
    $today_ts       = strtotime($today);
    $today_dow      = (int) date('N', $today_ts); // 1=Mon, 7=Sun
    $days_to_sunday = 7 - $today_dow;
    $grid_end_ts    = strtotime("+$days_to_sunday days", $today_ts);
    $grid_start_ts  = strtotime('-34 days', $grid_end_ts);

    for ($i = 0; $i < 35; $i++) {
      $cell_ts   = strtotime("+$i days", $grid_start_ts);
      $cell_date = date('Y-m-d', $cell_ts);

      $is_today  = ($cell_date === $today);
      $is_future = ($cell_date > $today);
      $is_due    = dropgoals_goal_is_due($goal, $cell_date);

      $class = 'dropgoals-history-day';
      $title = format_date($cell_ts, 'custom', 'M j, Y');

      if ($is_future) {
        $class .= ' dropgoals-day-future';
      }
      elseif (!$is_due) {
        $class .= ' dropgoals-day-na';
        $title .= ' — ' . t('not due');
      }
      elseif (isset($log_rows[$cell_date])) {
        $class .= $log_rows[$cell_date] ? ' dropgoals-day-completed' : ' dropgoals-day-missed';
        $title .= $log_rows[$cell_date] ? ' — ' . t('completed') : ' — ' . t('missed');
      }
      else {
        $class .= ' dropgoals-day-missed';
      }
      if ($is_today) {
        $class .= ' dropgoals-day-today';
      }

      // Future and non-due days are non-interactive.
      if ($is_future || !$is_due) {
        $html .= '<div class="' . $class . '" title="' . check_plain($title) . '"></div>';
      }
      else {
        $toggle_url = url('drop/goals/toggle/' . $goal->gid . '/' . $cell_date);
        $title .= ' — ' . t('click to toggle');
        $html .= '<a href="' . $toggle_url . '" class="' . $class . '" title="' . check_plain($title) . '"></a>';
      }
    }

    $html .= '</div>'; // .dropgoals-history-grid
  }

  return $html;
}
