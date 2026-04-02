<?php
/**
 * @file
 * Page callbacks for DropTasks.
 */

/**
 * Page callback: display the task list.
 *
 * @param string $filter
 *   One of 'active', 'today', 'upcoming', 'completed'.
 *
 * @return array
 *   A render array.
 */
function droptasks_task_list($filter = 'active') {
  global $user;

  backdrop_set_title(t('Tasks'));

  $today_start    = strtotime('today midnight');
  $today_end      = strtotime('tomorrow midnight');
  $upcoming_end   = strtotime('+7 days midnight');

  // Optional tag filter via ?tag=name query parameter.
  $tag_filter = isset($_GET['tag']) ? trim($_GET['tag']) : '';

  // Show tasks the user owns OR is assigned to.
  // For individual-completion tasks, exclude ones the current user has already completed.
  $query = db_select('drop_task', 't')
    ->fields('t');
  $query->leftJoin('drop_task_assignee', 'ta', 'ta.tid = t.tid AND ta.uid = :uid', array(':uid' => $user->uid));
  $query->leftJoin('drop_task_completion', 'tc', 'tc.tid = t.tid AND tc.uid = :cuid', array(':cuid' => $user->uid));
  $or = db_or()
    ->condition('t.uid', $user->uid)
    ->isNotNull('ta.uid');
  $query->condition($or)
    ->distinct();

  if ($tag_filter !== '') {
    $query->join('field_data_field_drop_tags', 'ft', "ft.entity_id = t.tid AND ft.entity_type = 'drop_task'");
    $query->join('taxonomy_term_data', 'td', 'td.tid = ft.field_drop_tags_tid');
    $query->condition('td.name', $tag_filter);
  }

  switch ($filter) {
    case 'today':
      // Overdue + due today (pending, not yet completed by this user).
      $query->condition('t.status', 'pending');
      $query->condition('t.due_date', $today_end, '<');
      $query->isNotNull('t.due_date');
      $query->condition(db_or()
        ->condition('t.completion_mode', 0)
        ->isNull('tc.cid')
      );
      break;

    case 'upcoming':
      // Pending within next 7 days, not yet completed by this user.
      $query->condition('t.status', 'pending');
      $query->condition('t.due_date', $today_start, '>=');
      $query->condition('t.due_date', $upcoming_end, '<');
      $query->condition(db_or()
        ->condition('t.completion_mode', 0)
        ->isNull('tc.cid')
      );
      break;

    case 'completed':
      // Shared: status = complete. Individual: current user has a completion record.
      $query->condition(db_or()
        ->condition(db_and()->condition('t.status', 'complete')->condition('t.completion_mode', 0))
        ->isNotNull('tc.cid')
      );
      $query->orderBy('t.changed', 'DESC');
      break;

    case 'active':
    default:
      $filter = 'active';
      // Shared: status = pending. Individual: pending AND not yet completed by this user.
      $query->condition(db_or()
        ->condition(db_and()->condition('t.status', 'pending')->condition('t.completion_mode', 0))
        ->condition(db_and()->condition('t.status', 'pending')->condition('t.completion_mode', 1)->isNull('tc.cid'))
      );
      break;
  }

  if ($filter !== 'completed') {
    // Sort: overdue first, then by due date (nulls last), then by priority desc.
    $query->orderBy('t.due_date', 'ASC');
    $query->orderBy('t.priority', 'DESC');
  }

  $results = $query->execute()->fetchAllAssoc('tid');
  $tasks = array();
  if ($results) {
    $tids = array_keys($results);
    $tasks = entity_load('drop_task', $tids);
    // Re-order to match query order.
    $ordered = array();
    foreach ($tids as $tid) {
      if (isset($tasks[$tid])) {
        $ordered[$tid] = $tasks[$tid];
      }
    }
    $tasks = $ordered;
  }

  $output = array(
    '#theme'      => 'droptasks_task_list',
    '#tasks'      => $tasks,
    '#filter'     => $filter,
    '#tag_filter' => $tag_filter,
    '#attached'   => array(
      'css' => array(backdrop_get_path('module', 'droptasks') . '/droptasks.css'),
    ),
  );

  if (module_exists('dropcrm') && user_access('access crm')) {
    $output['#suffix'] = '<p>' . l(t('← Go to CRM'), 'drop/crm') . '</p>';
  }

  return $output;
}

/**
 * Theme function: render the task list table.
 *
 * @param array $variables
 *   - tasks: array of DroptTask entities.
 *   - filter: the active filter string.
 *
 * @return string
 *   Rendered HTML.
 */
function theme_droptasks_task_list($variables) {
  $tasks      = $variables['tasks'];
  $filter     = $variables['filter'];
  $tag_filter = $variables['tag_filter'];

  $priority_labels = drop_base_priority_labels();

  // Active tag filter banner.
  $prefix = '';
  if ($tag_filter !== '') {
    $clear_url = url('drop/tasks/' . $filter);
    $prefix = '<p class="droptasks-tag-filter-banner">'
      . t('Showing tasks tagged: <strong>@tag</strong>', array('@tag' => $tag_filter))
      . ' &nbsp;·&nbsp; '
      . '<a href="' . $clear_url . '">' . t('clear filter') . '</a>'
      . '</p>';
  }

  $rows = array();
  foreach ($tasks as $task) {
    $due = '';
    $row_class = '';
    if (!empty($task->due_date)) {
      $due = format_date($task->due_date, 'custom', 'M j, Y');
      if ($task->isOverdue()) {
        $due = '<span class="drop-overdue">' . $due . ' (' . t('overdue') . ')</span>';
        $row_class = 'drop-row-overdue';
      }
      elseif ($task->isDueToday()) {
        $due = '<span class="drop-due-today">' . $due . '</span>';
      }
    }

    $priority_label = isset($priority_labels[$task->priority]) ? $priority_labels[$task->priority] : '';
    $priority_class = 'drop-priority-' . drop_base_priority_class($task->priority);

    // Title + recurrence indicator + tag chips stacked in the first cell.
    $tags_html       = _droptasks_render_tags($task);
    $recurrence_html = _droptasks_recurrence_label($task);
    $title_cell = l(check_plain($task->title), 'drop/tasks/' . $task->tid);
    $sub = array();
    if ($recurrence_html) {
      $sub[] = '<span class="droptasks-recurrence">' . check_plain($recurrence_html) . '</span>';
    }
    if ($tags_html) {
      $sub[] = $tags_html;
    }
    if ($sub) {
      $title_cell .= '<br>' . implode(' ', $sub);
    }

    $complete_label = ($task->status === 'complete') ? t('Reopen') : t('Complete');
    $actions = l(t('Edit'), 'drop/tasks/' . $task->tid . '/edit')
      . ' &nbsp;·&nbsp; '
      . l($complete_label, 'drop/tasks/' . $task->tid . '/complete')
      . ' &nbsp;·&nbsp; '
      . l(t('Delete'), 'drop/tasks/' . $task->tid . '/delete');

    $row = array(
      'data' => array(
        array('data' => $title_cell),
        array('data' => $priority_label, 'class' => array($priority_class)),
        $due,
        $actions,
      ),
    );
    if ($row_class) {
      $row['class'] = array($row_class);
    }
    $rows[] = $row;
  }

  $empty_messages = array(
    'active'    => t('No active tasks. <a href="!url">Add one</a>.', array('!url' => url('drop/tasks/add'))),
    'today'     => t('Nothing due today.'),
    'upcoming'  => t('Nothing due in the next 7 days.'),
    'completed' => t('No completed tasks.'),
  );
  $empty = isset($empty_messages[$filter]) ? $empty_messages[$filter] : t('No tasks found.');

  $header = array(t('Task'), t('Priority'), t('Due Date'), t('Actions'));

  return $prefix . theme('table', array(
    'header'     => $header,
    'rows'       => $rows,
    'empty'      => $empty,
    'attributes' => array('class' => array('droptasks-list')),
  ));
}

/**
 * Page callback: view a single task.
 *
 * @param DroptTask $task
 *   The task entity.
 *
 * @return array
 *   A render array.
 */
function droptasks_task_view(DroptTask $task) {
  backdrop_set_title(check_plain($task->title));

  $priority_labels = drop_base_priority_labels();

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'droptasks') . '/droptasks.css'),
    ),
  );

  // Actions bar.
  $actions = array();
  if (droptasks_task_access('update', $task)) {
    $complete_label = ($task->status === 'complete') ? t('Reopen') : t('Mark complete');
    $actions[] = l(t('Edit task'), 'drop/tasks/' . $task->tid . '/edit');
    $actions[] = l($complete_label, 'drop/tasks/' . $task->tid . '/complete');
  }
  if (droptasks_task_access('delete', $task)) {
    $actions[] = l(t('Delete'), 'drop/tasks/' . $task->tid . '/delete');
  }

  // "Log time" link — links to DropTime daily page with note pre-filled.
  if (module_exists('droptime') && user_access('access droptime')) {
    $actions[] = l(t('+ Log time'), 'drop/time', array(
      'query' => array(
        'notes'  => $task->title,
        'return' => 'drop/tasks/' . $task->tid,
      ),
    ));
  }
  $output['actions'] = array(
    '#markup' => drop_base_page_actions($actions),
    '#weight' => -100,
  );

  // Detail rows.
  $rows = array();

  $status_label = ($task->status === 'complete') ? t('Complete') : t('Pending');
  $rows[] = array(t('Status'), $status_label);

  if (isset($priority_labels[$task->priority])) {
    $rows[] = array(t('Priority'), $priority_labels[$task->priority]);
  }

  if (!empty($task->due_date)) {
    $due = format_date($task->due_date, 'custom', 'M j, Y');
    if ($task->isOverdue()) {
      $due .= ' <span class="drop-overdue">(' . t('overdue') . ')</span>';
    }
    $rows[] = array(t('Due'), $due);
  }

  $recurrence_label = _droptasks_recurrence_label($task);
  if ($recurrence_label) {
    $rows[] = array(t('Recurrence'), check_plain($recurrence_label));
  }

  // Assignees.
  $assignee_uids = db_select('drop_task_assignee', 'a')
    ->fields('a', array('uid'))
    ->condition('a.tid', $task->tid)
    ->execute()
    ->fetchCol();
  if ($assignee_uids) {
    $assignee_accounts = user_load_multiple($assignee_uids);
    $names = array();
    foreach ($assignee_accounts as $acc) {
      $names[] = check_plain($acc->name);
    }
    $rows[] = array(t('Assignees'), implode(', ', $names));
  }

  $rows[] = array(t('Created'), format_date($task->created, 'custom', 'M j, Y'));

  // Tags.
  $tags_html = _droptasks_render_tags($task);
  if ($tags_html) {
    $rows[] = array(t('Tags'), $tags_html);
  }

  // Linked contact.
  if (!empty($task->contact_id) && module_exists('dropcrm')) {
    $contacts = entity_load('crm_contact', array($task->contact_id));
    $contact  = $contacts ? reset($contacts) : NULL;
    if ($contact) {
      $rows[] = array(t('Contact'), l(entity_label('crm_contact', $contact), 'drop/crm/contact/' . $task->contact_id));
    }
  }

  // Source activity.
  if (!empty($task->source_activity_id) && module_exists('dropcrm')) {
    $activities = entity_load('crm_activity', array($task->source_activity_id));
    $activity   = $activities ? reset($activities) : NULL;
    if ($activity) {
      $rows[] = array(t('From activity'), l(entity_label('crm_activity', $activity), 'crm/activities/' . $task->source_activity_id));
    }
  }

  $output['details_heading'] = array(
    '#markup' => drop_base_section_header(t('Details')),
    '#weight' => 0,
  );
  $output['details'] = array(
    '#theme'      => 'table',
    '#rows'       => $rows,
    '#attributes' => array('class' => array('droptasks-detail-table')),
    '#weight'     => 1,
  );

  // Completion history for recurring tasks.
  if (!empty($task->recurrence)) {
    $completions = db_select('drop_task_completion', 'c')
      ->fields('c', array('uid', 'completed'))
      ->condition('c.tid', $task->tid)
      ->orderBy('c.completed', 'DESC')
      ->range(0, 20)
      ->execute()
      ->fetchAll();

    $output['completion_heading'] = array(
      '#markup' => drop_base_section_header(t('Completion History')),
      '#weight' => 12,
    );

    if ($completions) {
      $uids     = array_unique(array_column((array) $completions, 'uid'));
      $accounts = user_load_multiple($uids);
      $comp_rows = array();
      foreach ($completions as $c) {
        $name = isset($accounts[$c->uid]) ? check_plain($accounts[$c->uid]->name) : t('Unknown');
        $comp_rows[] = array(
          format_date($c->completed, 'custom', 'M j, Y g:ia'),
          $name,
        );
      }
      $output['completion_history'] = array(
        '#theme'      => 'table',
        '#header'     => array(t('Completed'), t('By')),
        '#rows'       => $comp_rows,
        '#attributes' => array('class' => array('droptasks-detail-table')),
        '#weight'     => 13,
      );
    }
    else {
      $output['completion_history'] = array(
        '#markup' => '<p>' . t('No completions recorded yet.') . '</p>',
        '#weight' => 13,
      );
    }
  }

  if (!empty($task->notes)) {
    $output['notes_heading'] = array(
      '#markup' => drop_base_section_header(t('Notes')),
      '#weight' => 10,
    );
    $output['notes'] = array(
      '#markup' => check_markup($task->notes),
      '#weight' => 11,
    );
  }

  return $output;
}

/**
 * Page callback: toggle task complete/pending.
 *
 * For recurring tasks, completing advances the due date instead of marking done.
 *
 * @param DroptTask $task
 *   The task entity.
 */
function droptasks_task_toggle_complete(DroptTask $task) {
  global $user;

  if (!empty($task->completion_mode)) {
    // Individual mode: track per-user in drop_task_completion.
    $existing_cid = db_select('drop_task_completion', 'c')
      ->fields('c', array('cid'))
      ->condition('tid', $task->tid)
      ->condition('uid', $user->uid)
      ->execute()
      ->fetchField();

    if ($existing_cid) {
      db_delete('drop_task_completion')->condition('cid', $existing_cid)->execute();
      backdrop_set_message(t('Task %title reopened for you.', array('%title' => $task->title)));
    }
    else {
      db_insert('drop_task_completion')->fields(array(
        'tid'       => $task->tid,
        'uid'       => $user->uid,
        'completed' => REQUEST_TIME,
      ))->execute();
      backdrop_set_message(t('Task %title marked complete for you.', array('%title' => $task->title)));
    }
  }
  elseif (!empty($task->recurrence) && $task->status === 'pending') {
    // Recurring shared task: log completion and advance to next occurrence.
    db_insert('drop_task_completion')->fields(array(
      'tid'       => $task->tid,
      'uid'       => $user->uid,
      'completed' => REQUEST_TIME,
    ))->execute();

    $next_ts = _droptasks_next_occurrence($task);
    if ($next_ts) {
      $task->due_date = $next_ts;
      $task->save();
      $next_label = format_date($next_ts, 'custom', 'M j, Y');
      backdrop_set_message(t('Task %title done — next due @date.', array(
        '%title' => $task->title,
        '@date'  => $next_label,
      )));
    }
    else {
      backdrop_set_message(t('Could not calculate next recurrence for %title.', array('%title' => $task->title)), 'warning');
    }
  }
  else {
    // Shared non-recurring: flip task status for everyone.
    $task->status = ($task->status === 'complete') ? 'pending' : 'complete';
    $task->save();
    $label = ($task->status === 'complete') ? t('marked complete') : t('reopened');
    backdrop_set_message(t('Task %title @label.', array('%title' => $task->title, '@label' => $label)));
  }

  backdrop_goto('drop/tasks');
}

/**
 * Calculate the next occurrence timestamp for a recurring task.
 *
 * Always advances forward from tomorrow, so completing a task today will
 * never reschedule it for today again.
 *
 * @param DroptTask $task
 *
 * @return int|null
 *   Unix timestamp of next due date, or NULL on failure.
 */
function _droptasks_next_occurrence(DroptTask $task) {
  // Base: tomorrow midnight (we always move forward).
  $base = new DateTime('tomorrow midnight');

  switch ($task->recurrence) {
    case 'daily':
      return $base->getTimestamp();

    case 'weekly':
      $days = array_map('intval', array_filter(explode(',', $task->recurrence_days)));
      if (empty($days)) {
        return NULL;
      }
      sort($days);
      // Find the next matching day of week starting from tomorrow.
      for ($i = 0; $i < 8; $i++) {
        $check = clone $base;
        if ($i > 0) {
          $check->modify("+{$i} days");
        }
        $dow = (int) $check->format('w'); // 0=Sun…6=Sat
        if (in_array($dow, $days)) {
          return $check->getTimestamp();
        }
      }
      return NULL;

    case 'monthly':
      if ($task->recurrence_month_type === 'nth_weekday') {
        return _droptasks_next_nth_weekday($base, $task->recurrence_month_week, $task->recurrence_month_weekday);
      }
      else {
        // day_of_month: use the day number from the task's current due_date.
        $day = (int) format_date($task->due_date, 'custom', 'j');
        return _droptasks_next_day_of_month($base, $day);
      }
  }

  return NULL;
}

/**
 * Returns the next timestamp for a given day-of-month, starting from $base.
 *
 * @param DateTime $base  Starting point (typically tomorrow midnight).
 * @param int      $day   Day of month (1–31).
 *
 * @return int  Unix timestamp.
 */
function _droptasks_next_day_of_month(DateTime $base, $day) {
  $year  = (int) $base->format('Y');
  $month = (int) $base->format('n');
  $today = (int) $base->format('j');

  // If the target day is still ahead this month, use this month; else next.
  if ($today <= $day) {
    $target_year  = $year;
    $target_month = $month;
  }
  else {
    $next = clone $base;
    $next->modify('first day of next month');
    $target_year  = (int) $next->format('Y');
    $target_month = (int) $next->format('n');
  }

  // Clamp to the last day of the target month (e.g. Jan 31 → Feb 28).
  $max_day = (int) (new DateTime("{$target_year}-{$target_month}-01"))->format('t');
  $day     = min($day, $max_day);

  $dt = new DateTime("{$target_year}-{$target_month}-{$day} 00:00:00");
  return $dt->getTimestamp();
}

/**
 * Returns the next timestamp for the Nth weekday of a month, from $base.
 *
 * @param DateTime $base     Starting point (typically tomorrow midnight).
 * @param int      $week     1–4 or -1 for last.
 * @param int      $weekday  0=Sun…6=Sat.
 *
 * @return int|null  Unix timestamp, or NULL if the Nth weekday doesn't exist.
 */
function _droptasks_next_nth_weekday(DateTime $base, $week, $weekday) {
  // Try this month first; if already past, try next month.
  $base_year  = (int) $base->format('Y');
  $base_month = (int) $base->format('n');

  foreach (array(0, 1) as $offset) {
    $dt = clone $base;
    if ($offset === 0) {
      $dt->modify('first day of this month');
    }
    else {
      $dt->modify('first day of next month');
    }
    $year  = (int) $dt->format('Y');
    $month = (int) $dt->format('n');

    if ($week === -1) {
      // Last weekday: start from end of month, walk backward.
      $dt->modify('last day of this month');
      while ((int) $dt->format('w') !== $weekday) {
        $dt->modify('-1 day');
      }
    }
    else {
      // Find the first occurrence of $weekday in this month.
      while ((int) $dt->format('w') !== $weekday) {
        $dt->modify('+1 day');
      }
      // Advance to the Nth occurrence.
      if ($week > 1) {
        $dt->modify('+' . (($week - 1) * 7) . ' days');
      }
      // If we've overshot into the next month, this Nth weekday doesn't exist.
      if ((int) $dt->format('n') !== $month) {
        continue;
      }
    }

    // Only return if this date is >= our base (tomorrow).
    if ($dt->getTimestamp() >= $base->getTimestamp()) {
      return $dt->getTimestamp();
    }
  }

  return NULL;
}

// Priority labels and CSS class helpers are provided by drop_base:
// drop_base_priority_labels() and drop_base_priority_class().

// ---------------------------------------------------------------------------
// Tag helpers
// ---------------------------------------------------------------------------

/**
 * Returns a human-readable recurrence label for a task, or empty string.
 *
 * @param DroptTask $task
 *
 * @return string
 */
function _droptasks_recurrence_label(DroptTask $task) {
  if (empty($task->recurrence)) {
    return '';
  }

  $day_names = array(
    0 => t('Sun'), 1 => t('Mon'), 2 => t('Tue'), 3 => t('Wed'),
    4 => t('Thu'), 5 => t('Fri'), 6 => t('Sat'),
  );
  $week_labels = array(1 => t('1st'), 2 => t('2nd'), 3 => t('3rd'), 4 => t('4th'), -1 => t('Last'));

  switch ($task->recurrence) {
    case 'daily':
      return t('↻ Daily');

    case 'weekly':
      if (!empty($task->recurrence_days)) {
        $days = array_map('intval', explode(',', $task->recurrence_days));
        $names = array();
        foreach ($days as $d) {
          if (isset($day_names[$d])) {
            $names[] = $day_names[$d];
          }
        }
        return t('↻ Weekly') . ' (' . implode(', ', $names) . ')';
      }
      return t('↻ Weekly');

    case 'monthly':
      if ($task->recurrence_month_type === 'nth_weekday') {
        $week    = isset($week_labels[$task->recurrence_month_week]) ? $week_labels[$task->recurrence_month_week] : '';
        $weekday = isset($day_names[$task->recurrence_month_weekday]) ? $day_names[$task->recurrence_month_weekday] : '';
        return t('↻ Monthly') . ' (' . $week . ' ' . $weekday . ')';
      }
      else {
        if (!empty($task->due_date)) {
          $day = format_date($task->due_date, 'custom', 'jS');
          return t('↻ Monthly') . ' (' . t('the @day', array('@day' => $day)) . ')';
        }
        return t('↻ Monthly');
      }
  }

  return '';
}

/**
 * Returns rendered tag chips HTML for a task, or an empty string.
 *
 * Each chip links to the task list filtered by that tag.
 *
 * @param DroptTask $task
 * @param string $base_path
 *   The list path to link chips to (defaults to 'drop/tasks').
 *
 * @return string
 */
function _droptasks_render_tags(DroptTask $task, $base_path = 'drop/tasks') {
  $items = field_get_items('drop_task', $task, 'field_drop_tags');
  if (!$items) {
    return '';
  }
  $tids  = array_column($items, 'tid');
  $terms = taxonomy_term_load_multiple($tids);

  $chips = '';
  foreach ($items as $item) {
    if (!empty($terms[$item['tid']])) {
      $name   = $terms[$item['tid']]->name;
      $chips .= l(check_plain($name), $base_path, array(
        'query'      => array('tag' => $name),
        'attributes' => array('class' => array('droptasks-tag-chip')),
      ));
    }
  }
  return $chips ? '<span class="droptasks-tags">' . $chips . '</span>' : '';
}
