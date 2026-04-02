<?php
/**
 * @file
 * Page callbacks for Drop Strategic.
 */

// ---------------------------------------------------------------------------
// Dashboard
// ---------------------------------------------------------------------------

/**
 * Page callback: team goals dashboard.
 */
function dropstrategic_dashboard_page() {
  backdrop_set_title(t('Goals'));

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropstrategic') . '/dropstrategic.css'),
      'js'  => array(backdrop_get_path('module', 'dropstrategic') . '/dropstrategic.js'),
    ),
  );

  // Status filter.
  $status_filter = (isset($_GET['status']) && array_key_exists($_GET['status'], dropstrategic_status_options()))
    ? $_GET['status'] : 'active';

  $status_opts_html = '';
  foreach (array('active' => t('Active'), 'all' => t('All')) + dropstrategic_status_options() as $key => $label) {
    $status_opts_html .= '<option value="' . check_plain($key) . '"' . ($status_filter === $key ? ' selected' : '') . '>'
      . check_plain($label) . '</option>';
  }
  $filter_html = '<div class="droptickets-filter-row">'
    . '<form class="droptickets-filter-form" method="get" action="' . url('drop/strategic') . '">'
    . '<select name="status" class="droptickets-filter-select" onchange="this.form.submit()">' . $status_opts_html . '</select>'
    . '</form>'
    . '</div>';

  $output['filters'] = array('#markup' => $filter_html, '#weight' => -10);

  // Load goals.
  $query = db_select('drop_strategic_goal', 'g')
    ->fields('g')
    ->orderBy('g.type') // team first (t before p alphabetically)
    ->orderBy('g.progress', 'DESC')
    ->orderBy('g.created', 'ASC');

  if ($status_filter !== 'all') {
    $query->condition('g.status', $status_filter);
  }

  $goals = $query->execute()->fetchAll();

  if (empty($goals)) {
    $output['empty'] = array(
      '#markup' => '<p class="dropstrategic-empty">' . t('No goals yet. <a href="@url">Add the first one.</a>', array(
        '@url' => url('drop/strategic/add'),
      )) . '</p>',
    );
    return $output;
  }

  // Load owners.
  $uids     = array_unique(array_filter(array_column((array) $goals, 'owner_uid')));
  $accounts = $uids ? user_load_multiple($uids) : array();

  // Group: team goals, then personal goals.
  $grouped = array('team' => array(), 'personal' => array());
  foreach ($goals as $goal) {
    $grouped[$goal->type][] = $goal;
  }

  $html = '';
  foreach (array('team' => t('Team goals'), 'personal' => t('Personal goals')) as $type => $heading) {
    if (empty($grouped[$type])) {
      continue;
    }
    $html .= '<h2 class="dropstrategic-group-heading">' . check_plain($heading) . '</h2>';
    foreach ($grouped[$type] as $goal) {
      $html .= dropstrategic_render_goal_card($goal, $accounts);
    }
  }

  $output['goals'] = array('#markup' => $html, '#weight' => 0);
  return $output;
}

// ---------------------------------------------------------------------------
// My Goals
// ---------------------------------------------------------------------------

/**
 * Page callback: goals owned by the current user.
 */
function dropstrategic_mine_page() {
  global $user;
  backdrop_set_title(t('My goals'));

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropstrategic') . '/dropstrategic.css'),
      'js'  => array(backdrop_get_path('module', 'dropstrategic') . '/dropstrategic.js'),
    ),
  );

  $goals = db_select('drop_strategic_goal', 'g')
    ->fields('g')
    ->condition('g.owner_uid', $user->uid)
    ->condition('g.status', array('active', 'paused'), 'IN')
    ->orderBy('g.type')
    ->orderBy('g.progress', 'DESC')
    ->execute()->fetchAll();

  if (empty($goals)) {
    $output['empty'] = array(
      '#markup' => '<p class="dropstrategic-empty">' . t('You have no active goals. <a href="@url">Add one.</a>', array(
        '@url' => url('drop/strategic/add'),
      )) . '</p>',
    );
    return $output;
  }

  $accounts = array($user->uid => $user);
  $html = '';
  foreach ($goals as $goal) {
    $html .= dropstrategic_render_goal_card($goal, $accounts, TRUE);
  }
  $output['goals'] = array('#markup' => $html);
  return $output;
}

// ---------------------------------------------------------------------------
// Goal view
// ---------------------------------------------------------------------------

/**
 * Page callback: view a single goal.
 */
function dropstrategic_goal_view(DropGoalStrategic $goal) {
  global $user;

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropstrategic') . '/dropstrategic.css'),
      'js'  => array(backdrop_get_path('module', 'dropstrategic') . '/dropstrategic.js'),
    ),
  );

  $accounts   = user_load_multiple(array($goal->owner_uid));
  $owner      = isset($accounts[$goal->owner_uid]) ? $accounts[$goal->owner_uid] : NULL;
  $key_results = dropstrategic_key_results_load($goal->gid);

  $type_opts    = dropstrategic_type_options();
  $status_opts  = dropstrategic_status_options();
  $horizon_opts = dropstrategic_horizon_options();

  $type_label    = isset($type_opts[$goal->type])    ? $type_opts[$goal->type]    : $goal->type;
  $status_label  = isset($status_opts[$goal->status]) ? $status_opts[$goal->status] : $goal->status;
  $horizon_label = isset($horizon_opts[$goal->horizon]) ? $horizon_opts[$goal->horizon] : $goal->horizon;

  backdrop_set_title(check_plain($goal->title));

  // --- Header ---
  $header = '<div class="dropstrategic-view-header">';
  $header .= '<div class="dropstrategic-view-meta">';
  $header .= '<span class="dropstrategic-badge dropstrategic-badge--' . check_plain($goal->type) . '">' . check_plain($type_label) . '</span>';
  $header .= '<span class="dropstrategic-badge dropstrategic-badge--status-' . check_plain($goal->status) . '">' . check_plain($status_label) . '</span>';
  $header .= '<span class="dropstrategic-view-horizon">' . icon('calendar', array('attributes' => array('width' => 14, 'height' => 14))) . ' ' . check_plain($horizon_label) . '</span>';
  if ($goal->due_date) {
    $header .= '<span class="dropstrategic-view-due">' . icon('clock', array('attributes' => array('width' => 14, 'height' => 14))) . ' ' . t('Due') . ' ' . format_date($goal->due_date, 'custom', 'M j, Y') . '</span>';
  }
  if ($owner) {
    $header .= '<span class="dropstrategic-view-owner">' . icon('user', array('attributes' => array('width' => 14, 'height' => 14))) . ' ' . check_plain($owner->name) . '</span>';
  }
  $header .= '</div>'; // .meta
  $header .= '</div>'; // .header
  $output['header'] = array('#markup' => $header, '#weight' => -10);

  // --- Progress bar ---
  $token    = backdrop_get_token('kr-toggle');
  $progress = dropstrategic_calculate_progress($key_results);
  $output['progress'] = array(
    '#markup' => dropstrategic_render_progress_bar($progress),
    '#weight' => -5,
  );

  // --- Body ---
  if (!empty($goal->body)) {
    $output['body'] = array(
      '#markup' => '<div class="dropstrategic-view-body">' . check_markup($goal->body, $goal->body_format) . '</div>',
      '#weight' => 0,
    );
  }

  // --- Key results ---
  if (!empty($key_results)) {
    $kr_html = '<div class="dropstrategic-key-results">';
    $kr_html .= '<h3 class="dropstrategic-kr-heading">' . t('Key results') . '</h3>';
    $kr_html .= '<ul class="dropstrategic-kr-list">';
    foreach ($key_results as $kr) {
      $can_toggle = dropstrategic_goal_access('edit', $goal);
      $done_class = $kr->done ? ' dropstrategic-kr-item--done' : '';
      $kr_html .= '<li class="dropstrategic-kr-item' . $done_class . '" data-krid="' . (int) $kr->krid . '">';

      if ($can_toggle) {
        $toggle_url = url('drop/strategic/kr/' . $kr->krid . '/toggle', array('query' => array('token' => $token)));
        $checked    = $kr->done ? ' checked' : '';
        $kr_html .= '<input type="checkbox" class="dropstrategic-kr-check"' . $checked . ' data-url="' . check_plain($toggle_url) . '">';
      }
      else {
        $icon_name = $kr->done ? 'check-circle' : 'circle';
        $kr_html .= icon($icon_name, array('attributes' => array('width' => 16, 'height' => 16, 'class' => array('dropstrategic-kr-icon'))));
      }

      $kr_html .= '<span class="dropstrategic-kr-title">' . check_plain($kr->title) . '</span>';

      if (!empty($kr->target)) {
        $current = !empty($kr->current_value) ? check_plain($kr->current_value) : '0';
        $kr_html .= '<span class="dropstrategic-kr-metric">' . $current . ' / ' . check_plain($kr->target) . '</span>';
      }

      $kr_html .= '</li>';
    }
    $kr_html .= '</ul></div>';
    $output['key_results'] = array('#markup' => $kr_html, '#weight' => 5);
  }

  // --- Linked tickets ---
  if (module_exists('droptickets')) {
    $tickets = db_select('drop_ticket', 't')
      ->fields('t', array('tid', 'title', 'status', 'pid', 'ticket_number'))
      ->condition('t.goal_id', $goal->gid)
      ->isNull('t.import_id')
      ->orderBy('t.status')
      ->orderBy('t.priority', 'DESC')
      ->execute()->fetchAll();

    if (!empty($tickets)) {
      $pids     = array_unique(array_filter(array_column((array) $tickets, 'pid')));
      $projects = $pids ? entity_load('drop_project', $pids) : array();
      $status_options = droptickets_status_options();

      $done_statuses = array('done', 'closed');
      $open  = array_filter($tickets, function($t) use ($done_statuses) { return !in_array($t->status, $done_statuses); });
      $done  = array_filter($tickets, function($t) use ($done_statuses) { return in_array($t->status, $done_statuses); });

      $t_html  = '<div class="dropstrategic-linked-tickets">';
      $t_html .= '<h3 class="dropstrategic-kr-heading">' . t('Linked tickets') . ' <span class="dropstrategic-ticket-count">' . count($open) . '/' . count($tickets) . ' ' . t('open') . '</span></h3>';
      $t_html .= '<ul class="dropstrategic-ticket-list">';
      foreach (array_merge(array_values($open), array_values($done)) as $ticket) {
        $project = isset($projects[$ticket->pid]) ? $projects[$ticket->pid] : NULL;
        $label   = droptickets_ticket_label($ticket, $project);
        $status_label = isset($status_options[$ticket->status]) ? $status_options[$ticket->status] : $ticket->status;
        $is_done = in_array($ticket->status, $done_statuses);
        $t_html .= '<li class="dropstrategic-ticket-item' . ($is_done ? ' dropstrategic-ticket-item--done' : '') . '">';
        $t_html .= '<span class="droptickets-ticket-id">' . check_plain($label) . '</span> ';
        $t_html .= l($ticket->title, 'drop/tickets/' . $ticket->tid);
        $t_html .= ' <span class="dropstrategic-ticket-status droptickets-status droptickets-status--' . check_plain($ticket->status) . '">' . check_plain($status_label) . '</span>';
        $t_html .= '</li>';
      }
      $t_html .= '</ul>';
      $t_html .= '</div>';
      $output['linked_tickets'] = array('#markup' => $t_html, '#weight' => 8);
    }
  }

  // --- Private notes (owner only) ---
  $is_owner = (int) $goal->owner_uid === (int) $user->uid;
  if (($is_owner || user_access('administer dropstrategic')) && !empty($goal->private_notes)) {
    $output['private_notes'] = array(
      '#markup' => '<div class="dropstrategic-private-notes">'
        . '<h3 class="dropstrategic-private-notes-heading">'
        . icon('lock', array('attributes' => array('width' => 14, 'height' => 14)))
        . ' ' . t('Private notes') . '</h3>'
        . check_markup($goal->private_notes, $goal->private_notes_format)
        . '</div>',
      '#weight' => 10,
    );
  }

  return $output;
}

// ---------------------------------------------------------------------------
// Render helpers
// ---------------------------------------------------------------------------

/**
 * Renders a goal summary card for the dashboard.
 */
function dropstrategic_render_goal_card($goal, array $accounts, $show_private = FALSE) {
  global $user;

  $type_opts   = dropstrategic_type_options();
  $status_opts = dropstrategic_status_options();
  $type_label  = isset($type_opts[$goal->type]) ? $type_opts[$goal->type] : $goal->type;
  $status_label = isset($status_opts[$goal->status]) ? $status_opts[$goal->status] : $goal->status;
  $owner       = isset($accounts[$goal->owner_uid]) ? $accounts[$goal->owner_uid] : NULL;
  $key_results = dropstrategic_key_results_load($goal->gid);
  $kr_done     = count(array_filter($key_results, function($kr) { return $kr->done; }));
  $kr_total    = count($key_results);
  $progress    = dropstrategic_calculate_progress($key_results);

  $html  = '<div class="dropstrategic-card dropstrategic-card--' . check_plain($goal->status) . '">';

  // Card header row.
  $html .= '<div class="dropstrategic-card-header">';
  $html .= '<h3 class="dropstrategic-card-title">' . l($goal->title, 'drop/strategic/' . $goal->gid) . '</h3>';
  $html .= '<div class="dropstrategic-card-badges">';
  $html .= '<span class="dropstrategic-badge dropstrategic-badge--status-' . check_plain($goal->status) . '">' . check_plain($status_label) . '</span>';
  if ($owner) {
    $html .= '<span class="dropstrategic-card-owner">' . icon('user', array('attributes' => array('width' => 12, 'height' => 12))) . ' ' . check_plain($owner->name) . '</span>';
  }
  if ($goal->due_date) {
    $overdue = $goal->due_date < REQUEST_TIME && $goal->status === 'active';
    $html .= '<span class="dropstrategic-card-due' . ($overdue ? ' dropstrategic-card-due--overdue' : '') . '">'
      . icon('calendar', array('attributes' => array('width' => 12, 'height' => 12)))
      . ' ' . format_date($goal->due_date, 'custom', 'M j, Y') . '</span>';
  }
  $html .= '</div>'; // .badges
  $html .= '</div>'; // .header

  // Progress bar.
  $html .= dropstrategic_render_progress_bar($progress, $kr_done, $kr_total);

  // Key results preview (first 3).
  if (!empty($key_results)) {
    $html .= '<ul class="dropstrategic-card-krs">';
    foreach (array_slice($key_results, 0, 2) as $kr) {
      $done_class = $kr->done ? ' dropstrategic-card-kr--done' : '';
      $icon_name  = $kr->done ? 'check-circle' : 'circle';
      $html .= '<li class="dropstrategic-card-kr' . $done_class . '">'
        . icon($icon_name, array('attributes' => array('width' => 13, 'height' => 13)))
        . ' ' . check_plain($kr->title);
      if (!empty($kr->target)) {
        $current = !empty($kr->current_value) ? $kr->current_value : '0';
        $html .= ' <span class="dropstrategic-card-kr-metric">' . check_plain($current) . '/' . check_plain($kr->target) . '</span>';
      }
      $html .= '</li>';
    }
    if (count($key_results) > 3) {
      $html .= '<li class="dropstrategic-card-kr-more">'
        . t('@n more…', array('@n' => count($key_results) - 3)) . '</li>';
    }
    $html .= '</ul>';
  }

  // Edit link.
  if (dropstrategic_goal_access('edit', $goal)) {
    $html .= '<div class="dropstrategic-card-actions">'
      . l(t('Edit'), 'drop/strategic/' . $goal->gid . '/edit', array('attributes' => array('class' => array('dropstrategic-card-edit'))))
      . '</div>';
  }

  $html .= '</div>'; // .dropstrategic-card
  return $html;
}

/**
 * Renders a progress bar with optional key result count.
 */
function dropstrategic_render_progress_bar($progress, $kr_done = NULL, $kr_total = NULL) {
  $label = $progress . '%';
  if ($kr_total !== NULL && $kr_total > 0) {
    $label .= ' &nbsp;·&nbsp; ' . $kr_done . '/' . $kr_total . ' ' . t('key results');
  }
  return '<div class="dropstrategic-progress">'
    . '<div class="dropstrategic-progress-bar"><div class="dropstrategic-progress-fill" style="width:' . $progress . '%"></div></div>'
    . '<span class="dropstrategic-progress-label">' . $label . '</span>'
    . '</div>';
}
