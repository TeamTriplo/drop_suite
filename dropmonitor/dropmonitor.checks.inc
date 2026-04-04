<?php
/**
 * @file
 * Data collection functions for Drop Monitor.
 *
 * Three check types per server:
 * - http: Response code, response time, SSL certificate expiry.
 * - agent: Disk, updates, PHP, Backdrop version, cron, DB size.
 * - linode: CPU, RAM, transfer, uptime (via Linode API).
 */

/**
 * Runs all checks for all active servers.
 */
function dropmonitor_run_all_checks() {
  $servers = dropmonitor_get_servers(TRUE);
  foreach ($servers as $server) {
    dropmonitor_check_server($server);
  }
}

/**
 * Runs all checks for a single server.
 */
function dropmonitor_check_server($server) {
  _dropmonitor_check_http($server);
  _dropmonitor_check_agent($server);
  _dropmonitor_check_linode($server);
}

// -------------------------------------------------------------------------
// HTTP check.
// -------------------------------------------------------------------------

/**
 * Checks HTTP response and SSL certificate for a server.
 */
function _dropmonitor_check_http($server) {
  if (empty($server->site_url)) {
    return;
  }

  $config = config('dropmonitor.settings');
  $alert_response_ms = $config->get('alert_response_ms');
  $alert_cert_days = $config->get('alert_cert_days');

  $data = array();
  $status = 'ok';

  // HTTP response check. Suppress notices from core when connections fail.
  $start = microtime(TRUE);
  @$response = backdrop_http_request($server->site_url, array('timeout' => 15));
  $elapsed_ms = round((microtime(TRUE) - $start) * 1000);

  $data['response_code'] = isset($response->code) ? (int) $response->code : 0;
  $data['response_ms'] = $elapsed_ms;
  $data['response_error'] = isset($response->error) ? $response->error : '';

  if ($data['response_code'] === 0 || $data['response_code'] >= 500) {
    $status = 'critical';
  }
  elseif ($data['response_code'] >= 400) {
    $status = 'warning';
  }
  elseif ($elapsed_ms > $alert_response_ms) {
    $status = 'warning';
  }

  // SSL certificate check.
  $data['ssl_expiry_days'] = NULL;
  $data['ssl_expiry_date'] = '';

  if (strpos($server->site_url, 'https://') === 0) {
    $ssl_info = _dropmonitor_check_ssl($server->site_url);
    if ($ssl_info) {
      $data['ssl_expiry_days'] = $ssl_info['days'];
      $data['ssl_expiry_date'] = $ssl_info['date'];
      if ($ssl_info['days'] !== NULL && $ssl_info['days'] <= $alert_cert_days) {
        $status = ($ssl_info['days'] <= 0) ? 'critical' : 'warning';
      }
    }
  }

  dropmonitor_save_check($server->sid, 'http', $status, $data);
}

/**
 * Checks SSL certificate expiry for a URL.
 *
 * @return array|FALSE
 *   Array with 'days' and 'date' keys, or FALSE on failure.
 */
function _dropmonitor_check_ssl($url) {
  $parsed = parse_url($url);
  $host = $parsed['host'];
  $port = isset($parsed['port']) ? $parsed['port'] : 443;

  $context = stream_context_create(array(
    'ssl' => array(
      'capture_peer_cert' => TRUE,
      'verify_peer' => FALSE,
      'verify_peer_name' => FALSE,
    ),
  ));

  $stream = @stream_socket_client(
    'ssl://' . $host . ':' . $port,
    $errno, $errstr, 10,
    STREAM_CLIENT_CONNECT,
    $context
  );

  if (!$stream) {
    return FALSE;
  }

  $params = stream_context_get_params($stream);
  fclose($stream);

  if (empty($params['options']['ssl']['peer_certificate'])) {
    return FALSE;
  }

  $cert_info = openssl_x509_parse($params['options']['ssl']['peer_certificate']);
  if (empty($cert_info['validTo_time_t'])) {
    return FALSE;
  }

  $expiry_timestamp = $cert_info['validTo_time_t'];
  $days = (int) round(($expiry_timestamp - time()) / 86400);

  return array(
    'days' => $days,
    'date' => date('Y-m-d', $expiry_timestamp),
  );
}

// -------------------------------------------------------------------------
// Agent check.
// -------------------------------------------------------------------------

/**
 * Polls the status agent script on a remote server.
 */
function _dropmonitor_check_agent($server) {
  if (empty($server->agent_url) || empty($server->agent_token)) {
    return;
  }

  $config = config('dropmonitor.settings');
  $alert_disk_percent = $config->get('alert_disk_percent');

  $url = $server->agent_url . '?token=' . urlencode($server->agent_token);

  $options = array('timeout' => 15);

  // If an origin IP is set, rewrite the URL to hit the IP directly and pass
  // the original hostname as a Host header. This bypasses CDNs like Cloudflare.
  if (!empty($server->origin_ip)) {
    $parsed = parse_url($url);
    $host = $parsed['host'];
    $scheme = isset($parsed['scheme']) ? $parsed['scheme'] : 'https';
    $path = isset($parsed['path']) ? $parsed['path'] : '/';
    $query = isset($parsed['query']) ? '?' . $parsed['query'] : '';
    $url = $scheme . '://' . $server->origin_ip . $path . $query;
    $options['headers'] = array('Host' => $host);
    // Tell SSL to verify against the original hostname, not the IP.
    $options['context'] = stream_context_create(array(
      'ssl' => array(
        'peer_name' => $host,
        'verify_peer' => TRUE,
        'verify_peer_name' => TRUE,
      ),
    ));
  }

  @$response = backdrop_http_request($url, $options);

  if (!isset($response->data) || $response->code != 200) {
    $error = isset($response->error) ? $response->error : 'No response';
    watchdog('dropmonitor', 'Agent check failed for %label: @error (HTTP @code)', array(
      '%label' => $server->label,
      '@error' => $error,
      '@code' => isset($response->code) ? $response->code : 0,
    ), WATCHDOG_WARNING);
    dropmonitor_save_check($server->sid, 'agent', 'error', array(
      'error' => $error,
      'response_code' => isset($response->code) ? $response->code : 0,
    ));
    return;
  }

  $data = json_decode($response->data, TRUE);
  if (!is_array($data)) {
    watchdog('dropmonitor', 'Agent check for %label returned invalid JSON.', array(
      '%label' => $server->label,
    ), WATCHDOG_WARNING);
    dropmonitor_save_check($server->sid, 'agent', 'error', array(
      'error' => 'Invalid JSON response',
    ));
    return;
  }

  // Determine status based on thresholds.
  $status = 'ok';

  // Disk space check.
  if (isset($data['disk_used_percent'])) {
    $free_percent = 100 - $data['disk_used_percent'];
    if ($free_percent <= $alert_disk_percent) {
      $status = 'critical';
    }
    elseif ($free_percent <= $alert_disk_percent * 2) {
      $status = 'warning';
    }
  }

  // Pending security updates.
  if (!empty($data['pending_security'])) {
    $status = _dropmonitor_worse_status($status, 'warning');
  }

  // Cron age.
  if (isset($data['cron_age_hours']) && $data['cron_age_hours'] > 24) {
    $status = _dropmonitor_worse_status($status, 'warning');
  }

  dropmonitor_save_check($server->sid, 'agent', $status, $data);
}

// -------------------------------------------------------------------------
// Linode API check.
// -------------------------------------------------------------------------

/**
 * Fetches stats from the Linode API for a server.
 */
function _dropmonitor_check_linode($server) {
  if (empty($server->linode_id)) {
    return;
  }

  $config = config('dropmonitor.settings');
  $api_token = $config->get('linode_api_token');
  if (empty($api_token)) {
    return;
  }

  // Get instance info.
  $instance = _dropmonitor_linode_api('linode/instances/' . $server->linode_id, $api_token);
  if (!$instance) {
    watchdog('dropmonitor', 'Linode API check failed for %label (Linode ID: @id).', array(
      '%label' => $server->label,
      '@id' => $server->linode_id,
    ), WATCHDOG_WARNING);
    dropmonitor_save_check($server->sid, 'linode', 'error', array(
      'error' => 'Failed to fetch Linode instance data',
    ));
    return;
  }

  $data = array(
    'status' => isset($instance['status']) ? $instance['status'] : 'unknown',
    'label' => isset($instance['label']) ? $instance['label'] : '',
    'region' => isset($instance['region']) ? $instance['region'] : '',
    'type' => isset($instance['type']) ? $instance['type'] : '',
    'specs' => isset($instance['specs']) ? $instance['specs'] : array(),
    'ipv4' => isset($instance['ipv4']) ? $instance['ipv4'] : array(),
    'created' => isset($instance['created']) ? $instance['created'] : '',
    'updated' => isset($instance['updated']) ? $instance['updated'] : '',
  );

  // Get transfer usage for this Linode.
  $transfer = _dropmonitor_linode_api('linode/instances/' . $server->linode_id . '/transfer', $api_token);
  if ($transfer) {
    $data['transfer_used_gb'] = isset($transfer['used']) ? round($transfer['used'] / 1073741824, 2) : 0;
    // Quota is already in GB from the API.
    $data['transfer_quota_gb'] = isset($transfer['quota']) ? (int) $transfer['quota'] : 0;
  }

  // Determine status.
  $status = 'ok';
  if (isset($data['status']) && $data['status'] !== 'running') {
    $status = 'critical';
  }

  dropmonitor_save_check($server->sid, 'linode', $status, $data);
}

/**
 * Makes a Linode API v4 request.
 *
 * @return array|FALSE
 *   Decoded JSON response, or FALSE on failure.
 */
function _dropmonitor_linode_api($endpoint, $api_token) {
  $url = 'https://api.linode.com/v4/' . $endpoint;
  @$response = backdrop_http_request($url, array(
    'headers' => array(
      'Authorization' => 'Bearer ' . $api_token,
      'Content-Type' => 'application/json',
    ),
    'timeout' => 15,
  ));

  if (!isset($response->data) || $response->code != 200) {
    return FALSE;
  }

  return json_decode($response->data, TRUE);
}

// -------------------------------------------------------------------------
// Helpers.
// -------------------------------------------------------------------------

/**
 * Returns the worse of two statuses.
 */
function _dropmonitor_worse_status($current, $new) {
  $severity = array('ok' => 0, 'warning' => 1, 'critical' => 2, 'error' => 3);
  $current_val = isset($severity[$current]) ? $severity[$current] : 0;
  $new_val = isset($severity[$new]) ? $severity[$new] : 0;
  return ($new_val > $current_val) ? $new : $current;
}
