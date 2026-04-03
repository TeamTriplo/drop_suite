<?php
/**
 * @file
 * Uptime monitoring for project websites.
 *
 * Checks all projects with a site_url via HEAD request and logs results.
 */

/**
 * Runs uptime checks for all active projects with a site_url.
 */
function dropmonitor_run_uptime_checks() {
  if (!db_table_exists('drop_project')) {
    return;
  }

  $projects = db_select('drop_project', 'p')
    ->fields('p', array('pid', 'name', 'site_url'))
    ->condition('p.active', 1)
    ->condition('p.site_url', '', '!=')
    ->execute()
    ->fetchAll();

  if (empty($projects)) {
    return;
  }

  $checked = 0;
  foreach ($projects as $project) {
    _dropmonitor_check_uptime($project);
    $checked++;
  }

  watchdog('dropmonitor', 'Uptime check complete: @count projects checked.', array(
    '@count' => $checked,
  ), WATCHDOG_INFO);
}

/**
 * Checks a single project's uptime and logs the result.
 */
function _dropmonitor_check_uptime($project) {
  $url = $project->site_url;

  // Ensure URL has a scheme.
  if (strpos($url, '://') === FALSE) {
    $url = 'https://' . $url;
  }

  $start = microtime(TRUE);
  @$response = backdrop_http_request($url, array(
    'method' => 'HEAD',
    'timeout' => 10,
    'max_redirects' => 3,
  ));
  $elapsed_ms = (int) round((microtime(TRUE) - $start) * 1000);

  $code = isset($response->code) ? (int) $response->code : 0;
  // Consider 2xx and 3xx as "up".
  $is_up = ($code >= 200 && $code < 400) ? 1 : 0;

  // Log the check.
  db_insert('dropmonitor_uptime')
    ->fields(array(
      'pid' => $project->pid,
      'status' => $is_up,
      'response_code' => $code,
      'response_ms' => $elapsed_ms,
      'checked' => REQUEST_TIME,
    ))
    ->execute();

  // Detect state change for notifications.
  _dropmonitor_uptime_state_change($project, $is_up, $code, $elapsed_ms);
}

/**
 * Detects uptime state changes and sends notifications.
 */
function _dropmonitor_uptime_state_change($project, $is_up, $code, $response_ms) {
  $state_key = 'dropmonitor.uptime_state.' . $project->pid;
  $previous = state_get($state_key, NULL);

  // First check ever — just record the state.
  if ($previous === NULL) {
    state_set($state_key, $is_up);
    return;
  }

  // No change — but check for escalation if still down.
  if ((int) $previous === $is_up) {
    if (!$is_up) {
      _dropmonitor_check_escalation($project);
    }
    return;
  }

  // State changed!
  state_set($state_key, $is_up);

  if ($is_up) {
    // Site came back up.
    $down_since = state_get('dropmonitor.uptime_down_since.' . $project->pid, 0);
    $downtime = $down_since ? format_interval(REQUEST_TIME - $down_since) : t('unknown');
    state_del('dropmonitor.uptime_down_since.' . $project->pid);
    state_del('dropmonitor.uptime_escalated.' . $project->pid);

    $message = ':white_check_mark: *' . $project->name . '* is BACK UP (was down for ' . $downtime . ')';
    watchdog('dropmonitor', '@name is back up after @duration downtime. Response: @code in @ms ms.', array(
      '@name' => $project->name,
      '@duration' => $downtime,
      '@code' => $code,
      '@ms' => $response_ms,
    ), WATCHDOG_INFO);

    // Notify both channels on recovery.
    _dropmonitor_slack_alert($message);
    _dropmonitor_slack_escalation($message);
  }
  else {
    // Site went down — immediate alert.
    state_set('dropmonitor.uptime_down_since.' . $project->pid, REQUEST_TIME);

    $message = ':red_circle: *' . $project->name . '* is DOWN (HTTP ' . $code . ')';
    watchdog('dropmonitor', '@name is DOWN. HTTP @code, response time @ms ms.', array(
      '@name' => $project->name,
      '@code' => $code,
      '@ms' => $response_ms,
    ), WATCHDOG_ERROR);

    // Immediate alert to support channel only.
    _dropmonitor_slack_alert($message);
  }
}

/**
 * Checks if a sustained outage should trigger an escalation.
 *
 * Fires once after 30 minutes of continuous downtime.
 */
function _dropmonitor_check_escalation($project) {
  $escalated = state_get('dropmonitor.uptime_escalated.' . $project->pid, FALSE);
  if ($escalated) {
    return; // Already escalated for this outage.
  }

  $down_since = state_get('dropmonitor.uptime_down_since.' . $project->pid, 0);
  if (!$down_since) {
    return;
  }

  $down_minutes = (REQUEST_TIME - $down_since) / 60;
  if ($down_minutes >= 30) {
    state_set('dropmonitor.uptime_escalated.' . $project->pid, TRUE);

    $duration = format_interval(REQUEST_TIME - $down_since);
    $message = ':rotating_light: *ESCALATION: ' . $project->name . '* has been DOWN for ' . $duration . '. Immediate attention required.';

    watchdog('dropmonitor', 'ESCALATION: @name has been down for @duration.', array(
      '@name' => $project->name,
      '@duration' => $duration,
    ), WATCHDOG_CRITICAL);

    _dropmonitor_slack_escalation($message);
  }
}

/**
 * Posts a message to the Slack alert channel (immediate notifications).
 */
function _dropmonitor_slack_alert($message) {
  $config = config('dropmonitor.settings');
  $channel = $config->get('slack_alert_channel');
  if (!empty($channel)) {
    _dropmonitor_slack_post($channel, $message);
  }
}

/**
 * Posts a message to the Slack escalation channel (30-minute sustained outage).
 */
function _dropmonitor_slack_escalation($message) {
  $config = config('dropmonitor.settings');
  $channel = $config->get('slack_escalation_channel');
  if (!empty($channel)) {
    _dropmonitor_slack_post($channel, $message);
  }
}

/**
 * Posts a message to a Slack channel using the DropBridge bot token.
 */
function _dropmonitor_slack_post($channel, $text) {
  $bot_token = config_get('dropbridge.settings', 'slack_bot_token');
  if (empty($bot_token)) {
    watchdog('dropmonitor', 'Cannot send Slack alert: no bot token configured in DropBridge settings.', array(), WATCHDOG_WARNING);
    return;
  }

  $payload = json_encode(array(
    'channel' => $channel,
    'text' => $text,
  ));

  $response = backdrop_http_request('https://slack.com/api/chat.postMessage', array(
    'method' => 'POST',
    'headers' => array(
      'Authorization' => 'Bearer ' . $bot_token,
      'Content-Type' => 'application/json',
    ),
    'data' => $payload,
    'timeout' => 10,
  ));

  if (isset($response->data)) {
    $result = json_decode($response->data, TRUE);
    if (empty($result['ok'])) {
      watchdog('dropmonitor', 'Slack alert failed: @error', array(
        '@error' => isset($result['error']) ? $result['error'] : 'unknown',
      ), WATCHDOG_WARNING);
    }
  }
}

/**
 * Purges old "up" records from the uptime log.
 *
 * Keeps all downtime records indefinitely. Removes "up" records older than
 * the specified number of days.
 */
function dropmonitor_uptime_purge($days = 14) {
  $cutoff = REQUEST_TIME - ($days * 86400);
  $deleted = db_delete('dropmonitor_uptime')
    ->condition('status', 1)
    ->condition('checked', $cutoff, '<')
    ->execute();

  if ($deleted > 0) {
    watchdog('dropmonitor', 'Purged @count uptime log records older than @days days.', array(
      '@count' => $deleted,
      '@days' => $days,
    ), WATCHDOG_INFO);
  }
}

/**
 * Calculates uptime percentage for a project over the last N days.
 *
 * @param int $pid
 *   Project ID.
 * @param int $days
 *   Number of days to look back.
 *
 * @return float|NULL
 *   Uptime percentage (0-100), or NULL if no data.
 */
function dropmonitor_uptime_percentage($pid, $days = 14) {
  $since = REQUEST_TIME - ($days * 86400);

  $total = (int) db_select('dropmonitor_uptime', 'u')
    ->condition('pid', $pid)
    ->condition('checked', $since, '>=')
    ->countQuery()
    ->execute()
    ->fetchField();

  if ($total === 0) {
    return NULL;
  }

  $up = (int) db_select('dropmonitor_uptime', 'u')
    ->condition('pid', $pid)
    ->condition('status', 1)
    ->condition('checked', $since, '>=')
    ->countQuery()
    ->execute()
    ->fetchField();

  return round(($up / $total) * 100, 2);
}

/**
 * Returns the latest uptime check for each project.
 *
 * @return array
 *   Array of objects keyed by pid, each with status, response_code,
 *   response_ms, and checked.
 */
function dropmonitor_uptime_latest() {
  // Get the max checked timestamp per project.
  $subquery = db_select('dropmonitor_uptime', 'u2')
    ->fields('u2', array('pid'))
    ->groupBy('u2.pid');
  $subquery->addExpression('MAX(u2.checked)', 'max_checked');

  $query = db_select('dropmonitor_uptime', 'u')
    ->fields('u', array('pid', 'status', 'response_code', 'response_ms', 'checked'));
  $query->join($subquery, 'latest', 'u.pid = latest.pid AND u.checked = latest.max_checked');

  return $query->execute()->fetchAllAssoc('pid');
}
