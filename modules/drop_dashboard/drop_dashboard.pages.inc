<?php
/**
 * @file
 * Page callbacks for Drop Dashboard.
 */

// ---------------------------------------------------------------------------
// Daily list
// ---------------------------------------------------------------------------

/**
 * Page callback: unified daily to-do list.
 *
 * Pulls pending/incomplete items from whichever of DropGoals, DropTasks,
 * DropTickets, and Drop CRM are enabled. Items are sorted with overdue first
 * (oldest first), then due today, then goals (which have no due date).
 *
 * @return array
 *   A render array.
 */
function drop_dashboard_daily_list() {
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

  $active_date = $view_date ?: $today_str;
  $is_preview  = !empty($view_date) && $view_date > $today_str;
  $is_past     = !empty($view_date) && $view_date < $today_str;

  $today_start = (new DateTime($active_date . ' 00:00:00', $site_tz))->getTimestamp();
  $today_end   = (new DateTime($active_date . ' 23:59:59', $site_tz))->getTimestamp();

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
        if (!dropgoals_goal_is_due($goal, $active_date)) {
          continue;
        }
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
      $task_q->condition('t.due_date', $today_start, '>=');
      $task_q->condition('t.due_date', $today_end,   '<=');
    }
    else {
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

  // ---- Drop CRM: planned activities due on/before the active date ----------
  if (module_exists('drop_crm') && user_access('access crm')) {
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

  // ---- Calendar events from DropBridge state (if installed) --------------
  // DropBridge owns the cron that fetches ICS feeds and stores them in
  // state keys 'dropbridge.ical_<md5>'. The dashboard reads those keys
  // directly so calendar events show up on /drop alongside other items.
  if (module_exists('dropbridge')) {
    $cal_events = _drop_dashboard_fetch_calendar_events($active_date);
    foreach ($cal_events as $event) {
      $items[] = $event;
    }
  }

  // Sort: overdue (sort_key=0) first by date, then due-today / events
  // (sort_key=1) by date, then goals (sort_key=2).
  usort($items, '_drop_dashboard_sort_items');

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'drop_dashboard') . '/drop_dashboard.css'),
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
    '#theme'        => 'drop_dashboard_daily_list',
    '#items'        => $items,
    '#preview_date' => $view_date,
    '#is_past'      => $is_past,
    '#weight'       => 0,
  );

  // ---- DropUpdates: Recent Team Updates section --------------------------
  if (module_exists('dropupdates') && user_access('view drop updates')) {
    $recent_html = _drop_dashboard_render_recent_updates($user);
    if ($recent_html) {
      $output['recent_updates'] = array(
        '#markup' => $recent_html,
        '#weight' => 5,
        '#attached' => array(
          'css' => array(backdrop_get_path('module', 'dropupdates') . '/css/dropupdates.css'),
        ),
      );
    }
  }

  return $output;
}

/**
 * Renders the "Recent Team Updates" panel for the /drop dashboard.
 *
 * @param object $viewer
 *   The current user account.
 *
 * @return string|false
 *   HTML markup, or FALSE if there's nothing to show.
 */
function _drop_dashboard_render_recent_updates($viewer) {
  $ids = db_select('drop_update', 'u')
    ->fields('u', array('uid_post'))
    ->condition('u.uid', $viewer->uid, '<>')
    ->orderBy('u.created', 'DESC')
    ->range(0, 5)
    ->execute()
    ->fetchCol();

  if (empty($ids)) {
    return FALSE;
  }

  $updates = entity_load('drop_update', $ids);

  $seen = array();
  if ($viewer->uid) {
    $seen = db_select('drop_update_view', 'v')
      ->fields('v', array('uid_post'))
      ->condition('v.uid_post', $ids, 'IN')
      ->condition('v.uid', $viewer->uid)
      ->execute()
      ->fetchCol();
    $seen = array_flip($seen);
  }

  $items_html = '';
  foreach ($ids as $id) {
    if (empty($updates[$id])) {
      continue;
    }
    $u = $updates[$id];
    $author = user_load($u->uid);
    $name   = $author ? format_username($author) : t('Someone');
    $time   = format_date($u->created, 'custom', 'M j, g:i a');

    $plain = trim(strip_tags($u->body));
    $plain = preg_replace('/\s+/', ' ', $plain);
    if (backdrop_strlen($plain) > 140) {
      $plain = backdrop_substr($plain, 0, 140) . '…';
    }

    $classes = array('dropbridge-recent-updates__item');
    if (!isset($seen[$id])) {
      $classes[] = 'is-unread';
    }

    $items_html .= l(
      '<span class="dropbridge-recent-updates__author">' . check_plain($name) . '</span>'
      . '<span class="dropbridge-recent-updates__time">' . check_plain($time) . '</span>'
      . '<div class="dropbridge-recent-updates__preview">' . check_plain($plain) . '</div>',
      'drop/updates',
      array(
        'html' => TRUE,
        'fragment' => 'u-' . (int) $id,
        'attributes' => array('class' => $classes),
      )
    );
  }

  $header = '<div class="dropbridge-section-header"><h3>' . t('Recent Team Updates') . '</h3>'
    . l(t('View all →'), 'drop/updates', array('attributes' => array('class' => array('dropbridge-section-more'))))
    . '</div>';

  return '<div class="dropbridge-recent-updates">' . $header . $items_html . '</div>';
}

/**
 * usort() comparator for daily list items.
 */
function _drop_dashboard_sort_items($a, $b) {
  if ($a['sort_key'] !== $b['sort_key']) {
    return $a['sort_key'] - $b['sort_key'];
  }
  return $a['sort_date'] - $b['sort_date'];
}

/**
 * Theme function: render the unified daily list as a grouped grid.
 *
 * Calendar events appear full-width at the top. Goals, Tasks, CRM Activities,
 * and Tickets appear in a responsive grid below, each in their own card.
 * Empty sections are omitted.
 *
 * @param array $variables
 *   - items: array of item arrays (type, label, due, overdue, done_url, view_url).
 *   - preview_date: optional preview date string.
 *
 * @return string
 *   Rendered HTML.
 */
function theme_drop_dashboard_daily_list($variables) {
  $items        = $variables['items'];
  $preview_date = $variables['preview_date'];
  $is_past      = !empty($variables['is_past']);
  $is_preview   = !empty($preview_date) && !$is_past;

  $today_str     = date('Y-m-d');
  $yesterday_str = date('Y-m-d', strtotime('-1 day'));
  $tomorrow_str  = date('Y-m-d', strtotime('+1 day'));

  $display_date = $preview_date ?: $today_str;
  $date_heading = format_date(strtotime($display_date), 'custom', 'l, F j, Y');

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

    if (!empty($groups['event']['items'])) {
      $html .= _drop_dashboard_render_section('event', $groups['event']['label'], $groups['event']['items'], NULL);
    }

    $action_types = array('goal', 'task', 'activity', 'ticket');
    $action_html  = '';
    foreach ($action_types as $type) {
      if (!empty($groups[$type]['items'])) {
        $action_html .= _drop_dashboard_render_section(
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
  if (module_exists('drop_crm')) {
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
 *   Item type: 'event', 'goal', 'task', 'activity', or 'ticket'.
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
function _drop_dashboard_render_section($type, $label, array $items, $add_url) {
  $count         = count($items);
  $overdue_count = count(array_filter($items, function($i) { return $i['overdue']; }));

  $html  = '<div class="dropbridge-section dropbridge-section--' . $type . '">';

  $html .= '<div class="dropbridge-section-header">';
  $html .= '<span class="dropbridge-section-title">' . check_plain($label) . '</span>';
  $html .= '<span class="dropbridge-section-count">' . $count . '</span>';
  if ($add_url) {
    $html .= l(t('+ Add'), $add_url, array('attributes' => array('class' => array('dropbridge-section-add'))));
  }
  $html .= '</div>';

  if ($overdue_count > 0) {
    $html .= '<div class="dropbridge-overdue-bar">'
      . format_plural($overdue_count, '1 overdue', '@count overdue')
      . '</div>';
  }

  $html .= '<ul class="dropbridge-section-list">';
  foreach ($items as $item) {
    $overdue    = $item['overdue'];
    $item_class = 'dropbridge-section-item';
    if ($overdue) {
      $item_class .= ' dropbridge-section-item--overdue';
    }

    $html .= '<li class="' . $item_class . '">';
    $html .= '<span class="dropbridge-section-item-body">';

    if (!empty($item['view_url'])) {
      $html .= l(check_plain($item['label']), $item['view_url'], array('html' => TRUE));
    }
    else {
      $html .= check_plain($item['label']);
    }

    if (!empty($item['time_label'])) {
      $html .= '<span class="dropbridge-item-meta dropbridge-item-meta--time">'
        . check_plain($item['time_label']) . '</span>';
    }
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

    if ($type === 'ticket' && !empty($item['status'])) {
      $status_options = function_exists('droptickets_status_options') ? droptickets_status_options() : array();
      $status_label = isset($status_options[$item['status']]) ? $status_options[$item['status']] : $item['status'];
      $html .= '<span class="dropbridge-item-meta dropbridge-item-status drop-ticket-status--' . check_plain($item['status']) . '">'
        . check_plain($status_label) . '</span>';
    }

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

    $html .= '</span>';

    if (!empty($item['done_url'])) {
      $html .= l(t('Mark done'), $item['done_url'], array(
        'attributes' => array('class' => array('dropbridge-done-link')),
      ));
    }

    $html .= '</li>';
  }
  $html .= '</ul>';
  $html .= '</div>';

  return $html;
}

// ---------------------------------------------------------------------------
// Calendar / ICS integration (today's events for the daily list)
//
// DropBridge owns cron-fetching of ICS feeds and stores raw ICS data in state
// keys 'dropbridge.ical_<md5>'. The dashboard reads those keys directly and
// parses out just today's events. The calendar weekly view (drop/calendar)
// stays in dropbridge with its own range parsers.
// ---------------------------------------------------------------------------

/**
 * Fetches and returns today's calendar events as dashboard item arrays.
 *
 * Reads cached ICS data from state keys populated by dropbridge's cron.
 * Returns an empty array if no ICS feeds are configured or the cache is cold.
 *
 * @param string $today_str
 *   Today's date in 'Y-m-d' format.
 *
 * @return array
 *   Array of item arrays ready for the unified daily list.
 */
function _drop_dashboard_fetch_calendar_events($today_str) {
  global $user;

  // Collect URL lines: global feeds + current user's personal feeds.
  // Both are managed by dropbridge but stored as state/config it owns.
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

    $stored = state_get('dropbridge.ical_' . md5($url));
    if (empty($stored['ics'])) {
      continue;
    }

    $label      = $calendar_label ?: (isset($stored['label']) ? $stored['label'] : NULL);
    $events     = _drop_dashboard_parse_ics($stored['ics'], $today_str, $label);
    $all_events = array_merge($all_events, $events);
  }

  return $all_events;
}

/**
 * Parses an ICS string and returns dashboard items for events occurring today.
 *
 * Handles:
 *  - Non-recurring events (DTSTART/DTEND without RRULE)
 *  - All-day events (VALUE=DATE)
 *  - Timezone-aware events (TZID= or trailing Z)
 *  - Recurring events with FREQ=DAILY, WEEKLY (BYDAY), and MONTHLY (BYDAY=nDOW)
 *
 * Yearly and complex recurrence (EXDATE, complex COUNT) are not supported
 * and will be skipped.
 *
 * @param string $ics
 *   Raw ICS content.
 * @param string $today_str
 *   Today's date in 'Y-m-d' format.
 * @param string|null $calendar_label
 *   Optional label for events from this calendar (e.g. "Wolves").
 *
 * @return array
 *   Array of dashboard item arrays.
 */
function _drop_dashboard_parse_ics($ics, $today_str, $calendar_label = NULL) {
  // Unfold long lines (RFC 5545: lines may be folded with CRLF + whitespace).
  $ics = preg_replace("/\r\n[ \t]/", '', $ics);
  $ics = preg_replace("/\n[ \t]/", '', $ics);

  $items    = array();
  $seen_uid = array();
  $today_dt = new DateTime($today_str . ' 00:00:00');
  $tomorrow_dt = clone $today_dt;
  $tomorrow_dt->modify('+1 day');

  if (!preg_match_all('/BEGIN:VEVENT\r?\n(.+?)END:VEVENT/s', $ics, $matches)) {
    return array();
  }

  foreach ($matches[1] as $block) {
    $item = _drop_dashboard_parse_vevent($block, $today_dt, $tomorrow_dt, $today_str);
    if (!$item) {
      continue;
    }
    if (!empty($item['uid']) && isset($seen_uid[$item['uid']])) {
      continue;
    }
    if (!empty($item['uid'])) {
      $seen_uid[$item['uid']] = TRUE;
    }
    if ($calendar_label !== NULL) {
      $item['calendar_label'] = $calendar_label;
    }
    $items[] = $item;
  }

  usort($items, function($a, $b) { return $a['sort_date'] - $b['sort_date']; });

  return $items;
}

/**
 * Parses a single VEVENT block and returns a dashboard item or NULL.
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
 *   A dashboard item array, or NULL if the event doesn't fall today.
 */
function _drop_dashboard_parse_vevent($block, DateTime $today_dt, DateTime $tomorrow_dt, $today_str) {
  $props = array();
  foreach (preg_split('/\r?\n/', trim($block)) as $line) {
    if (strpos($line, ':') === FALSE) {
      continue;
    }
    list($key, $value) = explode(':', $line, 2);
    $props[strtoupper($key)] = trim($value);
  }

  if (isset($props['STATUS']) && strtoupper($props['STATUS']) === 'CANCELLED') {
    return NULL;
  }

  $summary = isset($props['SUMMARY']) ? $props['SUMMARY'] : t('(no title)');
  $summary = str_replace(array('\\,', '\\;', '\\n', '\\N'), array(',', ';', "\n", "\n"), $summary);
  $summary = trim($summary);

  $uid = isset($props['UID']) ? trim($props['UID']) : NULL;

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

  if (preg_match('/^DTSTART;VALUE=DATE:(\d{8})/i', $dtstart_raw, $m)) {
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

  $occurs_today = FALSE;
  if (isset($props['RRULE'])) {
    $occurs_today = _drop_dashboard_rrule_occurs_today($props['RRULE'], $start_ts, $today_dt, $today_str);
  }
  else {
    $today_start_ts = $today_dt->getTimestamp();
    $today_end_ts   = $tomorrow_dt->getTimestamp();

    if ($is_allday) {
      $end_ts = $end_ts ?: ($start_ts + 86400);
      $occurs_today = $start_ts < $today_end_ts && $end_ts > $today_start_ts;
    }
    else {
      $event_end = $end_ts ?: ($start_ts + 3600);
      $occurs_today = $start_ts < $today_end_ts && $event_end > $today_start_ts;
    }
  }

  if (!$occurs_today) {
    return NULL;
  }

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
    'done_url'   => NULL,
    'view_url'   => NULL,
    'time_label' => $time_label,
    'sort_key'   => 1,
    'sort_date'  => $is_allday ? 0 : $start_ts,
  );
}

/**
 * Evaluates a basic RRULE to determine if an event occurs today.
 *
 * Supports FREQ=DAILY, WEEKLY (BYDAY), and MONTHLY (BYDAY=nDOW). Skips yearly.
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
function _drop_dashboard_rrule_occurs_today($rrule, $start_ts, DateTime $today_dt, $today_str) {
  $parts = array();
  foreach (explode(';', $rrule) as $part) {
    if (strpos($part, '=') !== FALSE) {
      list($k, $v) = explode('=', $part, 2);
      $parts[strtoupper($k)] = $v;
    }
  }

  $freq = isset($parts['FREQ']) ? strtoupper($parts['FREQ']) : '';

  if ($start_ts > $today_dt->getTimestamp() + 86399) {
    return FALSE;
  }

  if (!empty($parts['UNTIL'])) {
    $until_str = $parts['UNTIL'];
    if (preg_match('/^(\d{8})$/', $until_str, $m)) {
      $until_dt = DateTime::createFromFormat('Ymd', $m[1]);
    }
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

  if (!empty($parts['COUNT'])) {
    $count = (int) $parts['COUNT'];
    $interval = isset($parts['INTERVAL']) ? (int) $parts['INTERVAL'] : 1;

    if ($freq === 'DAILY') {
      $days_since = (int) floor(($today_dt->getTimestamp() - strtotime(date('Y-m-d', $start_ts))) / 86400);
      $occurrences_before = (int) floor($days_since / $interval);
      if ($occurrences_before >= $count) {
        return FALSE;
      }
    }
    elseif ($freq === 'WEEKLY') {
      $byday_count = 1;
      if (!empty($parts['BYDAY'])) {
        $byday_count = count(explode(',', $parts['BYDAY']));
      }
      $start_monday = strtotime('monday this week', $start_ts);
      $today_monday = strtotime('monday this week', $today_dt->getTimestamp());
      $weeks_since  = (int) round(($today_monday - $start_monday) / 604800);
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
    $days_since = (int) (($today_dt->getTimestamp() - strtotime(date('Y-m-d', $start_ts))) / 86400);
    return ($days_since % $interval) === 0;
  }

  if ($freq === 'WEEKLY') {
    $interval = isset($parts['INTERVAL']) ? (int) $parts['INTERVAL'] : 1;
    $day_map = array('MO' => 1, 'TU' => 2, 'WE' => 3, 'TH' => 4, 'FR' => 5, 'SA' => 6, 'SU' => 7);
    $today_dow = (int) $today_dt->format('N');

    $byday = isset($parts['BYDAY']) ? strtoupper($parts['BYDAY']) : date('N', $start_ts);
    $allowed_days = array();
    foreach (explode(',', $byday) as $day_code) {
      $day_code = preg_replace('/[^A-Z]/', '', $day_code);
      if (isset($day_map[$day_code])) {
        $allowed_days[] = $day_map[$day_code];
      }
    }
    if (empty($allowed_days)) {
      $allowed_days[] = (int) (new DateTime())->setTimestamp($start_ts)->format('N');
    }

    if (!in_array($today_dow, $allowed_days)) {
      return FALSE;
    }

    if ($interval > 1) {
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

    $months_since = ($year - $start_year) * 12 + ($month - $start_month);
    if ($months_since < 0 || ($months_since % $interval) !== 0) {
      return FALSE;
    }

    if (!empty($parts['BYDAY'])) {
      $byday = strtoupper($parts['BYDAY']);
      if (!preg_match('/^(-?\d+)(MO|TU|WE|TH|FR|SA|SU)$/', $byday, $m)) {
        return FALSE;
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
        $first_of_month = new DateTime("$year-$month-01");
        $first_dow      = (int) $first_of_month->format('N');
        $days_to_first  = ($target_dow - $first_dow + 7) % 7;
        $nth_day        = 1 + $days_to_first + ($nth - 1) * 7;
        return (int) $today_dt->format('j') === $nth_day;
      }
      else {
        $days_in_month = (int) $today_dt->format('t');
        $last_of_month = new DateTime("$year-$month-$days_in_month");
        $last_dow      = (int) $last_of_month->format('N');
        $days_from_last = ($last_dow - $target_dow + 7) % 7;
        $nth_day       = $days_in_month - $days_from_last + ($nth + 1) * 7;
        return (int) $today_dt->format('j') === $nth_day;
      }
    }
    else {
      return (int) $today_dt->format('j') === (int) date('j', $start_ts);
    }
  }

  return FALSE;
}
