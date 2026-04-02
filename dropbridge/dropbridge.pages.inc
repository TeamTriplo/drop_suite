<?php
/**
 * @file
 * Page callbacks for DropBridge.
 */

// ---------------------------------------------------------------------------
// Daily list
// ---------------------------------------------------------------------------

/**
 * Page callback: unified daily to-do list.
 *
 * Pulls pending/incomplete items from whichever of DropGoals, DropTasks,
 * and DropCRM are enabled. Items are sorted with overdue first (oldest first),
 * then due today, then goals (which have no due date).
 *
 * @return array
 *   A render array.
 */
function dropbridge_daily_list() {
  global $user;

  backdrop_set_title(t('Dashboard'));

  // Determine "today" in the site's configured timezone.
  $site_tz_name = config_get('system.date', 'default_timezone') ?: date_default_timezone_get();
  $site_tz      = new DateTimeZone($site_tz_name);
  $now          = new DateTime('now', $site_tz);
  $today_str    = $now->format('Y-m-d');

  // Support viewing a different date via ?date=YYYY-MM-DD.
  $view_date = NULL;
  if (!empty($_GET['date']) && preg_match('/^\d{4}-\d{2}-\d{2}$/', $_GET['date']) && $_GET['date'] !== $today_str) {
    $view_date = $_GET['date'];
  }

  // The "active" date for queries — today normally, view date when set.
  $active_date = $view_date ?: $today_str;
  $is_preview  = !empty($view_date) && $view_date > $today_str;
  $is_past     = !empty($view_date) && $view_date < $today_str;

  $today_start  = (new DateTime($active_date . ' 00:00:00', $site_tz))->getTimestamp();
  $today_end    = (new DateTime($active_date . ' 23:59:59', $site_tz))->getTimestamp();

  $items = array();

  // ---- DropGoals: active goals not yet completed on the active date -------
  if (module_exists('dropgoals')) {
    $gids = db_select('drop_goal', 'g')
      ->fields('g', array('gid'))
      ->condition('g.uid', $user->uid)
      ->condition('g.active', 1)
      ->orderBy('g.label', 'ASC')
      ->execute()
      ->fetchCol();

    module_load_include('inc', 'dropgoals', 'dropgoals.pages');

    if ($gids) {
      // Find which ones are already completed on the active date.
      $done_gids = db_select('drop_goal_log', 'l')
        ->fields('l', array('gid'))
        ->condition('l.gid', $gids, 'IN')
        ->condition('l.log_date', $active_date)
        ->condition('l.completed', 1)
        ->execute()
        ->fetchCol();
      $done_gids = array_flip($done_gids);

      $goals = entity_load('drop_goal', $gids);
      foreach ($goals as $gid => $goal) {
        if (isset($done_gids[$gid])) {
          continue;
        }
        // Skip goals that are not due on the active date.
        if (!dropgoals_goal_is_due($goal, $active_date)) {
          continue;
        }
        // Skip weekly/monthly goals already completed for their period.
        $frequency = isset($goal->frequency) ? $goal->frequency : 'daily';
        if ($frequency === 'weekly' || $frequency === 'monthly') {
          if (_dropgoals_period_completed($goal, $active_date)) {
            continue;
          }
        }
        $items[] = array(
          'type'      => 'goal',
          'label'     => $goal->label,
          'due'       => NULL,
          'overdue'   => FALSE,
          // No Done action in preview — can't complete future goals.
          'done_url'  => ($is_preview) ? NULL : 'drop/goal/' . $gid . '/done',
          'view_url'  => 'drop/goals/manage/' . $gid,
          'sort_key'  => 2,
          'sort_date' => 0,
        );
      }
    }
  }

  // ---- DropTasks: pending tasks due on/before the active date -------------
  if (module_exists('droptasks')) {
    $task_q = db_select('drop_task', 't')
      ->fields('t')
      ->condition('t.status', 'pending')
      ->isNotNull('t.due_date')
      ->orderBy('t.due_date', 'ASC')
      ->distinct();
    $task_q->leftJoin('drop_task_assignee', 'ta', 'ta.tid = t.tid AND ta.uid = :tuid', array(':tuid' => $user->uid));
    $or = db_or()
      ->condition('t.uid', $user->uid)
      ->isNotNull('ta.uid');
    $task_q->condition($or);

    if ($is_preview) {
      // Preview: only tasks due specifically on this date.
      $task_q->condition('t.due_date', $today_start, '>=');
      $task_q->condition('t.due_date', $today_end,   '<=');
    }
    else {
      // Today: overdue + due today.
      $task_q->condition('t.due_date', $today_end, '<=');
    }

    $task_results = $task_q->execute()->fetchAllAssoc('tid');

    if ($task_results) {
      $tasks = entity_load('drop_task', array_keys($task_results));
      foreach ($tasks as $tid => $task) {
        $overdue = !$is_preview && ($task->due_date < $today_start);
        $items[] = array(
          'type'      => 'task',
          'label'     => $task->title,
          'due'       => $task->due_date,
          'overdue'   => $overdue,
          'done_url'  => ($is_preview) ? NULL : 'drop/task/' . $tid . '/done',
          'view_url'  => 'drop/tasks/' . $tid,
          'sort_key'  => $overdue ? 0 : 1,
          'sort_date' => $task->due_date,
        );
      }
    }
  }

  // ---- DropCRM: planned activities due on/before the active date ----------
  if (module_exists('dropcrm') && user_access('access crm')) {
    $act_q = db_select('crm_activity', 'a')
      ->fields('a')
      ->condition('a.status', 'planned')
      ->isNotNull('a.due_date')
      ->orderBy('a.due_date', 'ASC');

    if ($is_preview) {
      $act_q->condition('a.due_date', $today_start, '>=');
      $act_q->condition('a.due_date', $today_end,   '<=');
    }
    else {
      $act_q->condition('a.due_date', $today_end, '<=');
    }

    $act_results = $act_q->execute()->fetchAllAssoc('aid');

    if ($act_results) {
      $activities = entity_load('crm_activity', array_keys($act_results));
      foreach ($activities as $aid => $activity) {
        $overdue = !$is_preview && ($activity->due_date < $today_start);
        $items[] = array(
          'type'      => 'activity',
          'label'     => $activity->subject,
          'due'       => $activity->due_date,
          'overdue'   => $overdue,
          'done_url'  => ($is_preview) ? NULL : 'drop/activity/' . $aid . '/done',
          'view_url'  => 'drop/crm/activity/' . $aid,
          'sort_key'  => $overdue ? 0 : 1,
          'sort_date' => $activity->due_date,
        );
      }
    }
  }

  // ---- DropTickets: tickets assigned to current user, not done -----------
  if (module_exists('droptickets')) {
    module_load_include('inc', 'droptickets', 'droptickets.pages');
    $active_statuses = droptickets_active_statuses();

    $tkt_q = db_select('drop_ticket', 't')
      ->fields('t')
      ->condition('t.assignee_uid', $user->uid)
      ->condition('t.status', $active_statuses, 'IN')
      ->orderBy('t.priority', 'DESC')
      ->orderBy('t.created', 'ASC');

    $tkt_results = $tkt_q->execute()->fetchAllAssoc('tid');

    if ($tkt_results) {
      $tickets = entity_load('drop_ticket', array_keys($tkt_results));
      foreach ($tickets as $tid => $ticket) {
        $overdue = !$is_preview && $ticket->due_date && ($ticket->due_date < $today_start);
        $due_today = $ticket->due_date && ($ticket->due_date >= $today_start) && ($ticket->due_date <= $today_end);
        $items[] = array(
          'type'      => 'ticket',
          'label'     => droptickets_ticket_label($ticket) . ': ' . $ticket->title,
          'due'       => $ticket->due_date,
          'overdue'   => $overdue,
          'status'    => $ticket->status,
          'done_url'  => ($is_preview || !user_access('close droptickets')) ? NULL : 'drop/tickets/' . $tid . '/done',
          'view_url'  => 'drop/tickets/' . $tid,
          'sort_key'  => $overdue ? 0 : ($due_today ? 1 : 2),
          'sort_date' => $ticket->due_date ?: PHP_INT_MAX,
        );
      }
    }
  }

  // ---- Google Calendar: events for the active date (read-only) -----------
  $cal_events = _dropbridge_fetch_calendar_events($active_date);
  foreach ($cal_events as $event) {
    $items[] = $event;
  }

  // Sort: overdue (sort_key=0) first by date, then due-today / events
  // (sort_key=1) by date, then goals (sort_key=2).
  usort($items, '_dropbridge_sort_items');

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropbridge') . '/dropbridge.css'),
    ),
  );

  // Notifications panel — shown only when the user has unread notifications.
  if (module_exists('droptickets') && $user->uid) {
    $unread = db_select('drop_notification', 'n')
      ->fields('n')
      ->condition('n.uid', $user->uid)
      ->condition('n.is_read', 0)
      ->orderBy('n.created', 'DESC')
      ->range(0, 10)
      ->execute()
      ->fetchAll();

    if ($unread) {
      $tids = array_unique(array_column((array) $unread, 'tid'));
      $notif_tickets = entity_load('drop_ticket', $tids);

      $pids = array_filter(array_unique(array_map(function($t) { return $t->pid; }, $notif_tickets)));
      $notif_projects = $pids ? entity_load('drop_project', $pids) : array();

      $actor_uids = array_unique(array_column((array) $unread, 'actor_uid'));
      $actors = $actor_uids ? user_load_multiple($actor_uids) : array();

      $type_labels = array(
        'assigned'  => t('assigned to you'),
        'commented' => t('new comment'),
        'updated'   => t('updated'),
      );

      $items_html = '';
      foreach ($unread as $n) {
        if (!isset($notif_tickets[$n->tid])) { continue; }
        $nt      = $notif_tickets[$n->tid];
        $np      = isset($notif_projects[$nt->pid]) ? $notif_projects[$nt->pid] : NULL;
        $nlabel  = droptickets_ticket_label($nt, $np);
        $actor   = isset($actors[$n->actor_uid]) ? check_plain($actors[$n->actor_uid]->name) : '';
        $type    = isset($type_labels[$n->type]) ? $type_labels[$n->type] : check_plain($n->type);
        $date    = format_date($n->created, 'custom', 'M j');

        $items_html .= '<li class="dropbridge-notif-item">';
        $items_html .= l('<span class="dropbridge-notif-label">' . check_plain($nlabel) . '</span> ' . check_plain($nt->title), 'drop/tickets/' . $n->tid, array('html' => TRUE));
        $items_html .= ' <span class="dropbridge-notif-meta">' . $type . ($actor ? ' · ' . $actor : '') . ' · ' . $date . '</span>';
        $items_html .= '</li>';
      }

      $count = count($unread);
      $heading = format_plural($count, '1 unread notification', '@count unread notifications');
      $all_link = l(t('View all'), 'drop/notifications');

      $output['notifications'] = array(
        '#markup' => '<details class="dropbridge-notifications">'
          . '<summary class="dropbridge-notifications-header"><strong>' . $heading . '</strong>' . $all_link . '</summary>'
          . '<ul class="dropbridge-notifications-list">' . $items_html . '</ul>'
          . '</details>',
        '#weight' => -10,
      );
    }
  }


  $output['daily_list'] = array(
    '#theme'        => 'dropbridge_daily_list',
    '#items'        => $items,
    '#preview_date' => $view_date,
    '#is_past'      => $is_past,
    '#weight'       => 0,
  );

  return $output;
}

/**
 * usort() comparator for daily list items.
 */
function _dropbridge_sort_items($a, $b) {
  if ($a['sort_key'] !== $b['sort_key']) {
    return $a['sort_key'] - $b['sort_key'];
  }
  // Within the same group, sort by date ascending (0 for goals, so stable).
  return $a['sort_date'] - $b['sort_date'];
}

/**
 * Theme function: render the unified daily list as a grouped grid.
 *
 * Calendar events appear full-width at the top. Goals, Tasks, and CRM
 * Activities appear in a responsive grid below, each in their own card.
 * Empty sections are omitted.
 *
 * @param array $variables
 *   - items: array of item arrays (type, label, due, overdue, done_url, view_url).
 *   - preview_date: optional preview date string.
 *
 * @return string
 *   Rendered HTML.
 */
function theme_dropbridge_daily_list($variables) {
  $items        = $variables['items'];
  $preview_date = $variables['preview_date'];
  $is_past      = !empty($variables['is_past']);
  $is_preview   = !empty($preview_date) && !$is_past;

  $today_str    = date('Y-m-d');
  $yesterday_str = date('Y-m-d', strtotime('-1 day'));
  $tomorrow_str  = date('Y-m-d', strtotime('+1 day'));

  $display_date = $preview_date ?: $today_str;
  $date_heading = format_date(strtotime($display_date), 'custom', 'l, F j, Y');

  // Date navigation banner.
  if ($is_preview) {
    $html  = '<div class="dropbridge-preview-banner">';
    $html .= t('Preview — items are read-only until the day arrives.');
    $html .= ' ' . l(t('← Back to today'), 'drop');
    $html .= '</div>';
    $html .= '<h2 class="dropbridge-date dropbridge-date--preview">' . check_plain($date_heading) . '</h2>';
  }
  elseif ($is_past) {
    $html  = '<h2 class="dropbridge-date dropbridge-date--past">' . check_plain($date_heading) . '</h2>';
    $html .= '<p class="dropbridge-date-nav">';
    // Previous day link (go one further back).
    $prev_str = date('Y-m-d', strtotime($preview_date . ' -1 day'));
    $html .= l('← ' . date('M j', strtotime($prev_str)), 'drop', array('query' => array('date' => $prev_str)));
    $html .= ' &nbsp; ';
    $html .= l(t('Today →'), 'drop');
    $html .= '</p>';
  }
  else {
    $html  = '<h2 class="dropbridge-date">' . check_plain($date_heading) . '</h2>';
    $html .= '<p class="dropbridge-date-nav">'
      . l('← ' . date('M j', strtotime($yesterday_str)), 'drop', array('query' => array('date' => $yesterday_str)))
      . ' &nbsp; '
      . l(date('M j', strtotime($tomorrow_str)) . ' →', 'drop', array('query' => array('date' => $tomorrow_str)))
      . '</p>';
  }

  if (empty($items)) {
    if ($is_past) {
      $html .= '<p class="dropbridge-empty">' . t('No pending items found for this day.') . '</p>';
    }
    elseif ($is_preview) {
      $html .= '<p class="dropbridge-empty">' . t('Nothing scheduled for this day yet.') . '</p>';
    }
    else {
      $html .= '<p class="dropbridge-empty">' . t("You're all caught up. Nothing left to do today.") . '</p>';
    }
  }
  else {
    // Group items by type, preserving their existing sort order within each group.
    $groups = array(
      'event'    => array('label' => t('Calendar'),   'add_url' => NULL),
      'goal'     => array('label' => t('Goals'),      'add_url' => 'drop/goals/manage/add'),
      'task'     => array('label' => t('Tasks'),      'add_url' => 'drop/tasks/add'),
      'activity' => array('label' => t('Activities'), 'add_url' => 'drop/crm/activities/add'),
      'ticket'   => array('label' => t('Tickets'),    'add_url' => 'drop/tickets/add'),
    );
    foreach ($groups as $type => $info) {
      $groups[$type]['items'] = array();
    }
    foreach ($items as $item) {
      $type = $item['type'];
      if (isset($groups[$type])) {
        $groups[$type]['items'][] = $item;
      }
    }

    // Calendar events: full-width section at the top.
    if (!empty($groups['event']['items'])) {
      $html .= _dropbridge_render_section('event', $groups['event']['label'], $groups['event']['items'], NULL);
    }

    // Actionable sections: responsive grid.
    $action_types = array('goal', 'task', 'activity', 'ticket');
    $action_html  = '';
    foreach ($action_types as $type) {
      if (!empty($groups[$type]['items'])) {
        $action_html .= _dropbridge_render_section(
          $type,
          $groups[$type]['label'],
          $groups[$type]['items'],
          $groups[$type]['add_url']
        );
      }
    }
    if ($action_html) {
      $html .= '<div class="dropbridge-grid">' . $action_html . '</div>';
    }

    // Count only actionable (non-event) items for the footer summary.
    $actionable = array_filter($items, function($i) { return $i['type'] !== 'event'; });
    $count = count($actionable);
    if ($count > 0) {
      $html .= '<p class="dropbridge-count">'
        . format_plural($count, '1 actionable item.', '@count actionable items.')
        . '</p>';
    }
  }

  // Module quick-links.
  $module_links = array();
  if (module_exists('dropgoals')) {
    $module_links[] = l(t('Goals'), 'drop/goals');
  }
  if (module_exists('droptasks')) {
    $module_links[] = l(t('Tasks'), 'drop/tasks');
  }
  if (module_exists('dropcrm')) {
    $module_links[] = l(t('CRM'), 'drop/crm');
  }
  if ($module_links) {
    $html .= '<p class="dropbridge-module-links">' . implode(' &nbsp;·&nbsp; ', $module_links) . '</p>';
  }
  global $user;
  $footer_links = array();
  $footer_links[] = l(t('My Account'), 'user/' . $user->uid . '/edit');
  if (user_access('administer site configuration')) {
    $footer_links[] = l(t('Settings'), 'drop/settings');
  }
  $html .= '<p class="dropbridge-settings-link">' . implode(' &nbsp;·&nbsp; ', $footer_links) . '</p>';

  return $html;
}

/**
 * Renders a single grouped section card for the daily list grid.
 *
 * @param string $type
 *   Item type: 'event', 'goal', 'task', or 'activity'.
 * @param string $label
 *   Human-readable section heading.
 * @param array $items
 *   The items to render within this section.
 * @param string|null $add_url
 *   Internal path for an "+ Add" link, or NULL for read-only sections.
 *
 * @return string
 *   HTML for one section card.
 */
function _dropbridge_render_section($type, $label, array $items, $add_url) {
  $count        = count($items);
  $overdue_count = count(array_filter($items, function($i) { return $i['overdue']; }));

  $html  = '<div class="dropbridge-section dropbridge-section--' . $type . '">';

  // Section header.
  $html .= '<div class="dropbridge-section-header">';
  $html .= '<span class="dropbridge-section-title">' . check_plain($label) . '</span>';
  $html .= '<span class="dropbridge-section-count">' . $count . '</span>';
  if ($add_url) {
    $html .= l(t('+ Add'), $add_url, array('attributes' => array('class' => array('dropbridge-section-add'))));
  }
  $html .= '</div>';

  // Overdue alert bar (shown when at least one item is overdue).
  if ($overdue_count > 0) {
    $html .= '<div class="dropbridge-overdue-bar">'
      . format_plural($overdue_count, '1 overdue', '@count overdue')
      . '</div>';
  }

  // Items list.
  $html .= '<ul class="dropbridge-section-list">';
  foreach ($items as $item) {
    $overdue = $item['overdue'];
    $item_class = 'dropbridge-section-item';
    if ($overdue) {
      $item_class .= ' dropbridge-section-item--overdue';
    }

    $html .= '<li class="' . $item_class . '">';
    $html .= '<span class="dropbridge-section-item-body">';

    // Label — linked to the item's detail page if a URL is set.
    if (!empty($item['view_url'])) {
      $html .= l(check_plain($item['label']), $item['view_url'], array('html' => TRUE));
    }
    else {
      $html .= check_plain($item['label']);
    }

    // Time label (calendar events with a specific time).
    if (!empty($item['time_label'])) {
      $html .= '<span class="dropbridge-item-meta dropbridge-item-meta--time">'
        . check_plain($item['time_label']) . '</span>';
    }
    // Due date for tasks / activities.
    elseif (!empty($item['due'])) {
      $due_label = format_date($item['due'], 'custom', 'M j');
      if ($overdue) {
        $html .= '<span class="dropbridge-item-meta dropbridge-item-meta--overdue">'
          . check_plain($due_label) . ' — ' . t('overdue') . '</span>';
      }
      else {
        $html .= '<span class="dropbridge-item-meta">' . check_plain($due_label) . '</span>';
      }
    }

    // Status badge for tickets.
    if ($type === 'ticket' && !empty($item['status'])) {
      $status_options = function_exists('droptickets_status_options') ? droptickets_status_options() : array();
      $status_label = isset($status_options[$item['status']]) ? $status_options[$item['status']] : $item['status'];
      $html .= '<span class="dropbridge-item-meta dropbridge-item-status dropbridge-ticket-status--' . check_plain($item['status']) . '">'
        . check_plain($status_label) . '</span>';
    }

    // Calendar source chip for events (when multiple calendars are configured).
    if ($type === 'event' && !empty($item['calendar_label'])) {
      $palette = array(
        '#1565c0', '#6a1b9a', '#ad1457', '#00838f',
        '#e65100', '#558b2f', '#4527a0', '#00695c',
        '#c62828', '#37474f',
      );
      $color = $palette[abs(crc32($item['calendar_label'])) % count($palette)];
      $html .= '<span class="dropbridge-cal-chip" style="background:' . $color . ';">'
        . check_plain($item['calendar_label']) . '</span>';
    }

    $html .= '</span>'; // .dropbridge-section-item-body

    // Done link (omitted for read-only items like calendar events).
    if (!empty($item['done_url'])) {
      $html .= l(t('Mark done'), $item['done_url'], array(
        'attributes' => array('class' => array('dropbridge-done-link')),
      ));
    }

    $html .= '</li>';
  }
  $html .= '</ul>';
  $html .= '</div>'; // .dropbridge-section

  return $html;
}

// ---------------------------------------------------------------------------
// Google Calendar / ICS integration
// ---------------------------------------------------------------------------

/**
 * Fetches and returns today's calendar events as dropbridge item arrays.
 *
 * Results are cached for the configured interval. Returns an empty array if
 * no ICS URL is configured or if the fetch fails.
 *
 * @param string $today_str
 *   Today's date in 'Y-m-d' format.
 *
 * @return array
 *   Array of item arrays ready for the unified daily list.
 */
function _dropbridge_fetch_calendar_events($today_str) {
  global $user;

  // Collect URL lines: global feeds + current user's personal feeds.
  $url_lines = array();

  $global = config_get('dropbridge.settings', 'calendar_ics_url');
  if (!empty(trim($global))) {
    $url_lines = array_merge($url_lines, array_filter(array_map('trim', explode("\n", $global))));
  }

  $personal_map = state_get('dropbridge.personal_calendars', array());
  if (!empty($personal_map[$user->uid])) {
    $url_lines = array_merge($url_lines, array_filter(array_map('trim', explode("\n", $personal_map[$user->uid]))));
  }

  if (empty($url_lines)) {
    return array();
  }

  $all_events = array();
  $seen       = array();

  foreach ($url_lines as $url_line) {
    $parts          = array_map('trim', explode('|', $url_line, 2));
    $url            = $parts[0];
    $calendar_label = (isset($parts[1]) && $parts[1] !== '') ? $parts[1] : NULL;

    if (empty($url) || isset($seen[$url])) {
      continue;
    }
    $seen[$url] = TRUE;

    // Read raw ICS from state — populated by hook_cron().
    $stored = state_get('dropbridge.ical_' . md5($url));
    if (empty($stored['ics'])) {
      continue;
    }

    $label      = $calendar_label ?: (isset($stored['label']) ? $stored['label'] : NULL);
    $events     = _dropbridge_parse_ics($stored['ics'], $today_str, $label);
    $all_events = array_merge($all_events, $events);
  }

  return $all_events;
}

/**
 * Parses an ICS string and returns dropbridge items for events occurring today.
 *
 * Handles:
 *  - Non-recurring events (DTSTART/DTEND without RRULE)
 *  - All-day events (VALUE=DATE)
 *  - Timezone-aware events (TZID= or trailing Z)
 *  - Recurring events with FREQ=DAILY or FREQ=WEEKLY (BYDAY)
 *
 * Monthly, yearly, and complex recurrence (EXDATE, complex COUNT) are not
 * supported and will be skipped.
 *
 * @param string $ics
 *   Raw ICS content.
 * @param string $today_str
 *   Today's date in 'Y-m-d' format.
 * @param string|null $calendar_label
 *   Optional label for events from this calendar (e.g. "Wolves").
 *
 * @return array
 *   Array of dropbridge item arrays.
 */
function _dropbridge_parse_ics($ics, $today_str, $calendar_label = NULL) {
  // Unfold long lines (RFC 5545: lines may be folded with CRLF + whitespace).
  $ics = preg_replace("/\r\n[ \t]/", '', $ics);
  $ics = preg_replace("/\n[ \t]/", '', $ics);

  $items    = array();
  $seen_uid = array();
  $today_dt = new DateTime($today_str . ' 00:00:00');
  $tomorrow_dt = clone $today_dt;
  $tomorrow_dt->modify('+1 day');

  // Split into VEVENT blocks.
  if (!preg_match_all('/BEGIN:VEVENT\r?\n(.+?)END:VEVENT/s', $ics, $matches)) {
    return array();
  }

  foreach ($matches[1] as $block) {
    $item = _dropbridge_parse_vevent($block, $today_dt, $tomorrow_dt, $today_str);
    if (!$item) {
      continue;
    }
    // Deduplicate by UID (same event can appear in multiple calendars).
    if (!empty($item['uid']) && isset($seen_uid[$item['uid']])) {
      continue;
    }
    if (!empty($item['uid'])) {
      $seen_uid[$item['uid']] = TRUE;
    }
    // Attach the per-calendar label if provided.
    if ($calendar_label !== NULL) {
      $item['calendar_label'] = $calendar_label;
    }
    $items[] = $item;
  }

  // Sort by start time within this group.
  usort($items, function($a, $b) { return $a['sort_date'] - $b['sort_date']; });

  return $items;
}

/**
 * Parses a single VEVENT block and returns a dropbridge item or NULL.
 *
 * @param string $block
 *   The VEVENT property lines (between BEGIN:VEVENT and END:VEVENT).
 * @param DateTime $today_dt
 *   Midnight at the start of today (server timezone).
 * @param DateTime $tomorrow_dt
 *   Midnight at the start of tomorrow (server timezone).
 * @param string $today_str
 *   Today's date in 'Y-m-d' format.
 *
 * @return array|null
 *   A dropbridge item array, or NULL if the event doesn't fall today.
 */
function _dropbridge_parse_vevent($block, DateTime $today_dt, DateTime $tomorrow_dt, $today_str) {
  // Parse all property lines into key => value, preserving parameter info.
  $props = array();
  foreach (preg_split('/\r?\n/', trim($block)) as $line) {
    if (strpos($line, ':') === FALSE) {
      continue;
    }
    list($key, $value) = explode(':', $line, 2);
    $props[strtoupper($key)] = trim($value);
  }

  // Skip cancelled events.
  if (isset($props['STATUS']) && strtoupper($props['STATUS']) === 'CANCELLED') {
    return NULL;
  }

  $summary = isset($props['SUMMARY']) ? $props['SUMMARY'] : t('(no title)');
  // Decode common ICS text escapes.
  $summary = str_replace(array('\\,', '\\;', '\\n', '\\N'), array(',', ';', "\n", "\n"), $summary);
  $summary = trim($summary);

  // Grab UID for cross-calendar deduplication.
  $uid = isset($props['UID']) ? trim($props['UID']) : NULL;

  // Find DTSTART and DTEND (may have parameters like ;TZID= or ;VALUE=DATE).
  $dtstart_raw = NULL;
  $dtend_raw   = NULL;
  foreach (preg_split('/\r?\n/', trim($block)) as $line) {
    if (preg_match('/^DTSTART(;[^:]*)?:(.+)$/i', $line, $m)) {
      $dtstart_raw = $line;
    }
    if (preg_match('/^DTEND(;[^:]*)?:(.+)$/i', $line, $m)) {
      $dtend_raw = $line;
    }
  }

  if (!$dtstart_raw) {
    return NULL;
  }

  $is_allday  = FALSE;
  $start_ts   = NULL;
  $end_ts     = NULL;
  $time_label = NULL;

  // Parse DTSTART.
  if (preg_match('/^DTSTART;VALUE=DATE:(\d{8})/i', $dtstart_raw, $m)) {
    // All-day event.
    $is_allday = TRUE;
    $start_dt  = DateTime::createFromFormat('Ymd', $m[1]);
    $start_ts  = $start_dt ? $start_dt->getTimestamp() : NULL;
  }
  elseif (preg_match('/^DTSTART;TZID=([^:]+):(\d{8}T\d{6})/i', $dtstart_raw, $m)) {
    try {
      $tz = new DateTimeZone($m[1]);
      $start_dt = DateTime::createFromFormat('Ymd\THis', $m[2], $tz);
      $start_ts = $start_dt ? $start_dt->getTimestamp() : NULL;
    }
    catch (Exception $e) {
      $start_ts = NULL;
    }
  }
  elseif (preg_match('/^DTSTART:(\d{8}T\d{6})(Z)?/i', $dtstart_raw, $m)) {
    $tz = !empty($m[2]) ? new DateTimeZone('UTC') : new DateTimeZone(date_default_timezone_get());
    $start_dt = DateTime::createFromFormat('Ymd\THis', $m[1], $tz);
    $start_ts = $start_dt ? $start_dt->getTimestamp() : NULL;
  }

  if ($start_ts === NULL) {
    return NULL;
  }

  // Parse DTEND if present.
  if ($dtend_raw) {
    if (preg_match('/^DTEND;VALUE=DATE:(\d{8})/i', $dtend_raw, $m)) {
      $end_dt = DateTime::createFromFormat('Ymd', $m[1]);
      $end_ts = $end_dt ? $end_dt->getTimestamp() : NULL;
    }
    elseif (preg_match('/^DTEND;TZID=([^:]+):(\d{8}T\d{6})/i', $dtend_raw, $m)) {
      try {
        $tz = new DateTimeZone($m[1]);
        $end_dt = DateTime::createFromFormat('Ymd\THis', $m[2], $tz);
        $end_ts = $end_dt ? $end_dt->getTimestamp() : NULL;
      }
      catch (Exception $e) {}
    }
    elseif (preg_match('/^DTEND:(\d{8}T\d{6})(Z)?/i', $dtend_raw, $m)) {
      $tz = !empty($m[2]) ? new DateTimeZone('UTC') : new DateTimeZone(date_default_timezone_get());
      $end_dt = DateTime::createFromFormat('Ymd\THis', $m[1], $tz);
      $end_ts = $end_dt ? $end_dt->getTimestamp() : NULL;
    }
  }

  // Check RRULE if present.
  $occurs_today = FALSE;
  if (isset($props['RRULE'])) {
    $occurs_today = _dropbridge_rrule_occurs_today($props['RRULE'], $start_ts, $today_dt, $today_str);
  }
  else {
    // Non-recurring: check if today falls within [DTSTART, DTEND).
    $today_start_ts = $today_dt->getTimestamp();
    $today_end_ts   = $tomorrow_dt->getTimestamp();

    if ($is_allday) {
      // For all-day events, DTEND is exclusive.
      $end_ts = $end_ts ?: ($start_ts + 86400);
      $occurs_today = $start_ts < $today_end_ts && $end_ts > $today_start_ts;
    }
    else {
      // For timed events: event starts today, or spans today.
      $event_end = $end_ts ?: ($start_ts + 3600);
      $occurs_today = $start_ts < $today_end_ts && $event_end > $today_start_ts;
    }
  }

  if (!$occurs_today) {
    return NULL;
  }

  // Build time label for timed events.
  if (!$is_allday && $start_ts) {
    $time_label = format_date($start_ts, 'custom', 'g:i A');
    if ($end_ts) {
      $time_label .= ' – ' . format_date($end_ts, 'custom', 'g:i A');
    }
  }

  return array(
    'type'       => 'event',
    'uid'        => $uid,
    'label'      => $summary,
    'due'        => $is_allday ? NULL : $start_ts,
    'overdue'    => FALSE,
    'done_url'   => NULL,  // Read-only.
    'view_url'   => NULL,
    'time_label' => $time_label,
    'sort_key'   => 1,     // Sort alongside "due today" items.
    'sort_date'  => $is_allday ? 0 : $start_ts,
  );
}

/**
 * Evaluates a basic RRULE to determine if an event occurs today.
 *
 * Supports FREQ=DAILY and FREQ=WEEKLY with BYDAY. Skips monthly/yearly.
 *
 * @param string $rrule
 *   The RRULE value string (e.g. "FREQ=WEEKLY;BYDAY=MO,WE,FR").
 * @param int $start_ts
 *   Unix timestamp of the event's original DTSTART.
 * @param DateTime $today_dt
 *   Midnight of today.
 * @param string $today_str
 *   Today's date 'Y-m-d'.
 *
 * @return bool
 */
function _dropbridge_rrule_occurs_today($rrule, $start_ts, DateTime $today_dt, $today_str) {
  // Parse RRULE into key => value pairs.
  $parts = array();
  foreach (explode(';', $rrule) as $part) {
    if (strpos($part, '=') !== FALSE) {
      list($k, $v) = explode('=', $part, 2);
      $parts[strtoupper($k)] = $v;
    }
  }

  $freq = isset($parts['FREQ']) ? strtoupper($parts['FREQ']) : '';

  // Must have started on or before today.
  if ($start_ts > $today_dt->getTimestamp() + 86399) {
    return FALSE;
  }

  // Check UNTIL.
  if (!empty($parts['UNTIL'])) {
    $until_str = $parts['UNTIL'];
    // DATE-only UNTIL.
    if (preg_match('/^(\d{8})$/', $until_str, $m)) {
      $until_dt = DateTime::createFromFormat('Ymd', $m[1]);
    }
    // DATETIME UNTIL.
    elseif (preg_match('/^(\d{8}T\d{6})(Z?)$/', $until_str, $m)) {
      $tz = $m[2] === 'Z' ? new DateTimeZone('UTC') : new DateTimeZone(date_default_timezone_get());
      $until_dt = DateTime::createFromFormat('Ymd\THis', $m[1], $tz);
    }
    else {
      $until_dt = NULL;
    }
    if ($until_dt && $until_dt->getTimestamp() < $today_dt->getTimestamp()) {
      return FALSE;
    }
  }

  // Check COUNT: if the recurrence has a fixed number of occurrences, work out
  // whether today is still within that count by approximating the number of
  // occurrences that would have happened between DTSTART and today.
  if (!empty($parts['COUNT'])) {
    $count = (int) $parts['COUNT'];
    $interval = isset($parts['INTERVAL']) ? (int) $parts['INTERVAL'] : 1;

    if ($freq === 'DAILY') {
      $days_since = (int) floor(($today_dt->getTimestamp() - strtotime(date('Y-m-d', $start_ts))) / 86400);
      // Number of occurrences that have happened up to (but not including) today.
      $occurrences_before = (int) floor($days_since / $interval);
      if ($occurrences_before >= $count) {
        return FALSE;
      }
    }
    elseif ($freq === 'WEEKLY') {
      // Count weekly occurrences: each week has at most count(BYDAY) occurrences.
      $byday_count = 1;
      if (!empty($parts['BYDAY'])) {
        $byday_count = count(explode(',', $parts['BYDAY']));
      }
      $start_monday = strtotime('monday this week', $start_ts);
      $today_monday = strtotime('monday this week', $today_dt->getTimestamp());
      $weeks_since  = (int) round(($today_monday - $start_monday) / 604800);
      // Approximate: full weeks * days_per_week.
      $approx_occurrences = (int) floor($weeks_since / $interval) * $byday_count;
      if ($approx_occurrences >= $count) {
        return FALSE;
      }
    }
  }

  if ($freq === 'DAILY') {
    $interval = isset($parts['INTERVAL']) ? (int) $parts['INTERVAL'] : 1;
    if ($interval <= 1) {
      return TRUE;
    }
    // Check if today lands on the right interval from DTSTART.
    $days_since = (int) (($today_dt->getTimestamp() - strtotime(date('Y-m-d', $start_ts))) / 86400);
    return ($days_since % $interval) === 0;
  }

  if ($freq === 'WEEKLY') {
    $interval = isset($parts['INTERVAL']) ? (int) $parts['INTERVAL'] : 1;
    // Map ICS day abbreviations to PHP date('N') values (1=Mon, 7=Sun).
    $day_map = array('MO' => 1, 'TU' => 2, 'WE' => 3, 'TH' => 4, 'FR' => 5, 'SA' => 6, 'SU' => 7);
    $today_dow = (int) $today_dt->format('N');

    $byday = isset($parts['BYDAY']) ? strtoupper($parts['BYDAY']) : date('N', $start_ts);
    $allowed_days = array();
    foreach (explode(',', $byday) as $day_code) {
      $day_code = preg_replace('/[^A-Z]/', '', $day_code); // strip ordinals
      if (isset($day_map[$day_code])) {
        $allowed_days[] = $day_map[$day_code];
      }
    }
    if (empty($allowed_days)) {
      // Fall back to the day of week from DTSTART.
      $allowed_days[] = (int) (new DateTime())->setTimestamp($start_ts)->format('N');
    }

    if (!in_array($today_dow, $allowed_days)) {
      return FALSE;
    }

    if ($interval > 1) {
      // Check week interval from DTSTART week.
      $start_monday = strtotime('monday this week', $start_ts);
      $today_monday = strtotime('monday this week', $today_dt->getTimestamp());
      $weeks = (int) round(($today_monday - $start_monday) / 604800);
      return ($weeks % $interval) === 0;
    }

    return TRUE;
  }

  if ($freq === 'MONTHLY') {
    $interval   = isset($parts['INTERVAL']) ? (int) $parts['INTERVAL'] : 1;
    $year       = (int) $today_dt->format('Y');
    $month      = (int) $today_dt->format('n');
    $start_year = (int) date('Y', $start_ts);
    $start_month = (int) date('n', $start_ts);

    // Check month interval: months since DTSTART must be divisible by INTERVAL.
    $months_since = ($year - $start_year) * 12 + ($month - $start_month);
    if ($months_since < 0 || ($months_since % $interval) !== 0) {
      return FALSE;
    }

    if (!empty($parts['BYDAY'])) {
      // Nth weekday pattern: e.g. "3TU" = 3rd Tuesday, "-1FR" = last Friday.
      $byday = strtoupper($parts['BYDAY']);
      if (!preg_match('/^(-?\d+)(MO|TU|WE|TH|FR|SA|SU)$/', $byday, $m)) {
        return FALSE; // Complex pattern not supported.
      }
      $nth        = (int) $m[1];
      $day_code   = $m[2];
      $day_map    = array('MO' => 1, 'TU' => 2, 'WE' => 3, 'TH' => 4, 'FR' => 5, 'SA' => 6, 'SU' => 7);
      $target_dow = $day_map[$day_code];
      $today_dow  = (int) $today_dt->format('N');

      if ($today_dow !== $target_dow) {
        return FALSE;
      }

      if ($nth > 0) {
        // Find the Nth occurrence of $target_dow in this month.
        $first_of_month = new DateTime("$year-$month-01");
        $first_dow      = (int) $first_of_month->format('N');
        $days_to_first  = ($target_dow - $first_dow + 7) % 7;
        $nth_day        = 1 + $days_to_first + ($nth - 1) * 7;
        return (int) $today_dt->format('j') === $nth_day;
      }
      else {
        // Negative ordinal: -1 = last, -2 = second-to-last, etc.
        $days_in_month = (int) $today_dt->format('t');
        $last_of_month = new DateTime("$year-$month-$days_in_month");
        $last_dow      = (int) $last_of_month->format('N');
        $days_from_last = ($last_dow - $target_dow + 7) % 7;
        $nth_day       = $days_in_month - $days_from_last + ($nth + 1) * 7;
        return (int) $today_dt->format('j') === $nth_day;
      }
    }
    else {
      // Same day-of-month as DTSTART (e.g. every 15th).
      return (int) $today_dt->format('j') === (int) date('j', $start_ts);
    }
  }

  // Yearly/other: not supported.
  return FALSE;
}

// ---------------------------------------------------------------------------
// Done callbacks
// ---------------------------------------------------------------------------

/**
 * Page callback: mark a goal as done for today, then return to the daily list.
 *
 * @param DropGoal $goal
 *   The goal entity.
 */
function dropbridge_goal_done(DropGoal $goal) {
  global $user;
  $today = date('Y-m-d');

  $existing = db_select('drop_goal_log', 'l')
    ->fields('l', array('lid', 'completed'))
    ->condition('l.gid', $goal->gid)
    ->condition('l.log_date', $today)
    ->execute()
    ->fetchObject();

  if ($existing) {
    db_update('drop_goal_log')
      ->fields(array('completed' => 1))
      ->condition('lid', $existing->lid)
      ->execute();
  }
  else {
    db_insert('drop_goal_log')
      ->fields(array(
        'gid'       => $goal->gid,
        'log_date'  => $today,
        'completed' => 1,
        'uid'       => $user->uid,
        'created'   => REQUEST_TIME,
      ))
      ->execute();
  }

  backdrop_set_message(t('%label marked done.', array('%label' => $goal->label)));
  backdrop_goto('drop');
}

/**
 * Page callback: mark a task as complete, then return to the daily list.
 *
 * @param DroptTask $task
 *   The task entity.
 */
function dropbridge_task_done(DroptTask $task) {
  $task->status = 'complete';
  $task->save();
  backdrop_set_message(t('Task %title marked complete.', array('%title' => $task->title)));
  backdrop_goto('drop');
}

/**
 * Page callback: mark a CRM activity as completed, then return to the daily list.
 *
 * @param CrmActivity $activity
 *   The activity entity.
 */
function dropbridge_activity_done(CrmActivity $activity) {
  $activity->status = 'completed';
  $activity->save();
  $link = l($activity->subject, 'drop/crm/activity/' . $activity->aid);
  backdrop_set_message(t('!link marked as done.', array('!link' => $link)));
  backdrop_goto('drop');
}
