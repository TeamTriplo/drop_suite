<?php
/**
 * @file
 * Page callbacks for Drop Monitor: dashboard, server detail, refresh.
 */

/**
 * Dashboard page — overview of all monitored servers.
 */
function dropmonitor_dashboard_page() {
  $servers = dropmonitor_get_servers(TRUE);

  if (empty($servers)) {
    return '<p>' . t('No servers are being monitored. !add', array(
      '!add' => l(t('Add a server'), 'drop/monitor/servers/add'),
    )) . '</p>';
  }

  $config = config('dropmonitor.settings');

  // Action bar.
  $actions = array();
  if (user_access('administer dropmonitor')) {
    $actions[] = l(t('Refresh all'), 'drop/monitor/refresh', array(
      'query' => array('token' => backdrop_get_token('dropmonitor_refresh')),
    ));
  }
  $output = '';
  if (!empty($actions)) {
    $output .= drop_base_page_actions($actions);
  }

  // Build server cards.
  $output .= '<div class="dropmonitor-dashboard">';

  foreach ($servers as $server) {
    $checks = dropmonitor_get_checks($server->sid);
    $overall = dropmonitor_overall_status($checks);

    $output .= '<div class="dropmonitor-card dropmonitor-card-' . $overall . '">';

    // Header.
    $output .= '<div class="dropmonitor-card-header">';
    $output .= '<h3>' . l(check_plain($server->label), 'drop/monitor/server/' . $server->sid) . '</h3>';
    $output .= dropmonitor_status_badge($overall);
    $output .= '</div>';

    // Metrics grid.
    $output .= '<div class="dropmonitor-metrics">';

    // HTTP check.
    if (isset($checks['http'])) {
      $http = json_decode($checks['http']->data, TRUE);
      $output .= _dropmonitor_metric_item(
        t('Response'),
        isset($http['response_code']) ? $http['response_code'] : '—',
        isset($http['response_ms']) ? $http['response_ms'] . 'ms' : ''
      );
      if (isset($http['ssl_expiry_days']) && $http['ssl_expiry_days'] !== NULL) {
        $cert_class = '';
        if ($http['ssl_expiry_days'] <= $config->get('alert_cert_days')) {
          $cert_class = $http['ssl_expiry_days'] <= 0 ? 'dropmonitor-critical' : 'dropmonitor-warning';
        }
        $output .= _dropmonitor_metric_item(
          t('SSL cert'),
          $http['ssl_expiry_days'] . ' ' . t('days'),
          $http['ssl_expiry_date'],
          $cert_class
        );
      }
    }

    // Agent check.
    $agent_sites_html = '';
    if (isset($checks['agent'])) {
      $agent = json_decode($checks['agent']->data, TRUE);
      if (isset($agent['error'])) {
        $output .= _dropmonitor_metric_item(t('Agent'), t('Error'), $agent['error'], 'dropmonitor-error');
      }
      elseif (isset($agent['_format']) && $agent['_format'] === 'standalone') {
        $output .= _dropmonitor_dashboard_standalone_metrics($agent, $config);
        $agent_sites_html = _dropmonitor_dashboard_standalone_sites($agent, $server->sid);
      }
      else {
        $output .= _dropmonitor_dashboard_legacy_agent($agent, $config);
      }
    }

    // Linode check.
    if (isset($checks['linode'])) {
      $linode = json_decode($checks['linode']->data, TRUE);
      if (isset($linode['status'])) {
        $linode_class = ($linode['status'] !== 'running') ? 'dropmonitor-critical' : '';
        $output .= _dropmonitor_metric_item(
          t('Linode'),
          ucfirst($linode['status']),
          isset($linode['region']) ? $linode['region'] : '',
          $linode_class
        );
      }
      // Show specs.
      if (isset($linode['specs']) && is_array($linode['specs'])) {
        $specs = $linode['specs'];
        $cpu_ram = array();
        if (!empty($specs['vcpus'])) {
          $cpu_ram[] = $specs['vcpus'] . ' vCPUs';
        }
        if (!empty($specs['memory'])) {
          $cpu_ram[] = round($specs['memory'] / 1024, 1) . ' GB RAM';
        }
        $disk_detail = !empty($specs['disk']) ? round($specs['disk'] / 1024, 0) . ' GB disk' : '';
        if (!empty($cpu_ram)) {
          $output .= _dropmonitor_metric_item(
            t('Resources'),
            implode(', ', $cpu_ram),
            $disk_detail
          );
        }
      }
      if (isset($linode['transfer_used_gb'])) {
        $output .= _dropmonitor_metric_item(
          t('Transfer'),
          $linode['transfer_used_gb'] . ' GB',
          isset($linode['transfer_quota_gb']) ? t('of') . ' ' . $linode['transfer_quota_gb'] . ' GB' : ''
        );
      }
    }

    $output .= '</div>'; // .dropmonitor-metrics

    // Per-site summary (standalone agent format only).
    $output .= $agent_sites_html;

    // Footer with last checked time.
    $last_checked = 0;
    foreach ($checks as $check) {
      if ($check->checked > $last_checked) {
        $last_checked = $check->checked;
      }
    }
    $output .= '<div class="dropmonitor-card-footer">';
    if ($last_checked) {
      $output .= '<span class="dropmonitor-last-checked">' . t('Checked @time ago', array('@time' => format_interval(REQUEST_TIME - $last_checked, 1))) . '</span>';
    }
    else {
      $output .= '<span class="dropmonitor-last-checked">' . t('Not yet checked') . '</span>';
    }
    if (user_access('administer dropmonitor')) {
      $output .= ' ' . l(t('Refresh'), 'drop/monitor/server/' . $server->sid . '/refresh', array(
        'query' => array('token' => backdrop_get_token('dropmonitor_refresh')),
        'attributes' => array('class' => array('dropmonitor-refresh-link')),
      ));
    }
    $output .= '</div>';

    $output .= '</div>'; // .dropmonitor-card
  }

  $output .= '</div>'; // .dropmonitor-dashboard

  return $output;
}

/**
 * Server detail page.
 */
function dropmonitor_server_detail_page($server) {
  $checks = dropmonitor_get_checks($server->sid);

  $actions = array();
  if (user_access('administer dropmonitor')) {
    $actions[] = l(t('Refresh'), 'drop/monitor/server/' . $server->sid . '/refresh', array(
      'query' => array('token' => backdrop_get_token('dropmonitor_refresh')),
    ));
    $actions[] = l(t('Edit server'), 'drop/monitor/servers/' . $server->sid . '/edit');
  }
  $actions[] = l(t('Back to dashboard'), 'drop/monitor');

  $output = drop_base_page_actions($actions);

  $output .= '<div class="dropmonitor-detail">';

  // Server info.
  $output .= '<h3>' . t('Server information') . '</h3>';
  $info_rows = array();
  if (!empty($server->site_url)) {
    $info_rows[] = array(t('Site URL'), l($server->site_url, $server->site_url, array('external' => TRUE)));
  }
  if (!empty($server->agent_url)) {
    $info_rows[] = array(t('Agent URL'), check_plain($server->agent_url));
  }
  if (!empty($server->linode_id)) {
    $info_rows[] = array(t('Linode ID'), $server->linode_id);
  }
  $output .= theme('table', array('rows' => $info_rows));

  // Check results.
  $check_types = array(
    'http' => t('HTTP Check'),
    'agent' => t('Status Agent'),
    'linode' => t('Linode API'),
  );

  foreach ($check_types as $type => $label) {
    if (!isset($checks[$type])) {
      continue;
    }
    $check = $checks[$type];
    $data = json_decode($check->data, TRUE);

    $output .= '<h3>' . $label . ' ' . dropmonitor_status_badge($check->status) . '</h3>';
    $output .= '<p class="dropmonitor-checked-at">' . t('Last checked: @date', array('@date' => format_date($check->checked, 'medium'))) . '</p>';

    if (!empty($data)) {
      // Standalone agent format gets structured rendering.
      if ($type === 'agent' && isset($data['_format']) && $data['_format'] === 'standalone') {
        $output .= _dropmonitor_detail_standalone($data, $server->sid);
      }
      else {
        $output .= _dropmonitor_detail_generic($data, $type);
      }
    }
  }

  $output .= '</div>';

  return $output;
}

/**
 * Renders detail page data for the standalone monitoring app format.
 */
function _dropmonitor_detail_standalone($data, $sid = NULL) {
  $output = '';
  $srv = $data['server'];
  $project_links = $sid ? dropmonitor_get_project_links($sid) : array();

  // Server metrics table.
  $rows = array();
  if (!empty($srv['hostname'])) {
    $rows[] = array(array('data' => '<strong>' . t('Hostname') . '</strong>'), check_plain($srv['hostname']));
  }
  if (!empty($srv['os'])) {
    $rows[] = array(array('data' => '<strong>' . t('OS') . '</strong>'), check_plain($srv['os']));
  }
  if (isset($srv['uptime_days'])) {
    $rows[] = array(array('data' => '<strong>' . t('Uptime') . '</strong>'), round($srv['uptime_days'], 1) . ' ' . t('days'));
  }
  if (isset($srv['disk_free_percent'])) {
    $rows[] = array(array('data' => '<strong>' . t('Disk free') . '</strong>'), $srv['disk_free_percent'] . '%');
  }
  if (isset($srv['memory'])) {
    $mem = $srv['memory'];
    $mem_text = $mem['used_percent'] . '% ' . t('used');
    if (isset($mem['used_mb']) && isset($mem['total_mb'])) {
      $mem_text .= ' (' . $mem['used_mb'] . ' / ' . $mem['total_mb'] . ' MB)';
    }
    $rows[] = array(array('data' => '<strong>' . t('Memory') . '</strong>'), $mem_text);
  }
  if (isset($srv['cpu_used_percent'])) {
    $rows[] = array(array('data' => '<strong>' . t('CPU') . '</strong>'), $srv['cpu_used_percent'] . '%');
  }
  if (isset($srv['network'])) {
    $net = $srv['network'];
    $rx = isset($net['rx_bytes_per_sec']) ? number_format($net['rx_bytes_per_sec']) : '0';
    $tx = isset($net['tx_bytes_per_sec']) ? number_format($net['tx_bytes_per_sec']) : '0';
    $rows[] = array(array('data' => '<strong>' . t('Network') . '</strong>'), t('RX: @rx B/s, TX: @tx B/s', array('@rx' => $rx, '@tx' => $tx)));
  }
  if (isset($srv['apt_updates_available'])) {
    $rows[] = array(array('data' => '<strong>' . t('Apt updates') . '</strong>'), $srv['apt_updates_available'] . ' ' . t('pending'));
  }

  if (!empty($rows)) {
    $output .= '<h4>' . t('Server metrics') . '</h4>';
    $output .= theme('table', array('rows' => $rows));
  }

  // SSL certificates.
  if (!empty($srv['ssl_certificates'])) {
    $output .= '<h4>' . t('SSL certificates') . '</h4>';
    $cert_header = array(t('Domain'), t('Days until expiry'));
    $cert_rows = array();
    foreach ($srv['ssl_certificates'] as $cert) {
      $domain = isset($cert['domain']) ? check_plain($cert['domain']) : t('Unknown');
      $days = isset($cert['days_until_expiry']) ? $cert['days_until_expiry'] : '—';
      $cert_rows[] = array($domain, $days);
    }
    $output .= theme('table', array('header' => $cert_header, 'rows' => $cert_rows));
  }

  // Per-site details.
  if (!empty($data['sites'])) {
    $output .= '<h4>' . t('Sites (@count)', array('@count' => count($data['sites']))) . '</h4>';
    $site_header = array(t('Site'), t('CMS'), t('HTTP'), t('Response'), t('DB size'), t('Watchdog (24h)'), t('Core version'), t('Notes'));
    $site_rows = array();
    foreach ($data['sites'] as $label => $site) {
      $site_name = check_plain($label);
      if (isset($project_links[$label])) {
        $site_name = l(check_plain($label), 'drop/projects/' . $project_links[$label]->pid);
      }
      elseif (!empty($site['http']['url'])) {
        $site_name = l(check_plain($label), $site['http']['url'], array('external' => TRUE));
      }

      $http_status = '';
      if (isset($site['http']['status'])) {
        $class = $site['http']['status'] === 'up' ? 'dropmonitor-up' : ($site['http']['status'] === 'down' ? 'dropmonitor-down' : '');
        $http_status = '<span class="' . $class . '">' . strtoupper($site['http']['status']) . '</span>';
        if (isset($site['http']['http_code'])) {
          $http_status .= ' (' . $site['http']['http_code'] . ')';
        }
      }

      $response = isset($site['http']['response_ms']) ? $site['http']['response_ms'] . 'ms' : '—';
      $db_size = isset($site['db_size_mb']) ? $site['db_size_mb'] . ' MB' : '—';
      $watchdog = isset($site['watchdog_errors_24h']) ? $site['watchdog_errors_24h'] : '—';

      // Core version and update status.
      $core_version = '—';
      if (!empty($site['core_update'])) {
        $cu = $site['core_update'];
        $current = isset($cu['current']) ? check_plain($cu['current']) : '';
        if (!empty($cu['update_available']) && !empty($cu['latest'])) {
          $latest = check_plain($cu['latest']);
          if (!empty($cu['security'])) {
            $core_version = $current . ' <span class="dropmonitor-site-security">&rarr; ' . $latest . ' (' . t('security') . ')</span>';
          }
          else {
            $core_version = $current . ' <span class="dropmonitor-site-update">&rarr; ' . $latest . '</span>';
          }
        }
        else {
          $core_version = $current;
        }
      }

      $notes = !empty($site['error']) ? '<span class="dropmonitor-site-errors">' . check_plain($site['error']) . '</span>' : '';

      $row_class = array();
      if (isset($site['http']['status']) && $site['http']['status'] === 'down') {
        $row_class = array('dropmonitor-row-down');
      }
      $site_rows[] = array(
        'data' => array($site_name, isset($site['cms']) ? $site['cms'] : '—', $http_status, $response, $db_size, $watchdog, $core_version, $notes),
        'class' => $row_class,
      );
    }
    $output .= theme('table', array('header' => $site_header, 'rows' => $site_rows));
  }

  return $output;
}

/**
 * Renders detail page data as a generic key-value table (legacy/Linode/HTTP).
 */
function _dropmonitor_detail_generic($data, $type) {
  $rows = array();
  foreach ($data as $key => $value) {
    // Skip internal format markers.
    if ($key === '_format') {
      continue;
    }
    if ($type === 'linode' && $key === 'specs' && is_array($value)) {
      $specs_output = '<ul style="margin: 5px 0; padding-left: 20px;">';
      $specs_output .= !empty($value['disk']) ? '<li>Disk: ' . round($value['disk'] / 1024, 0) . ' GB</li>' : '';
      $specs_output .= !empty($value['memory']) ? '<li>Memory: ' . $value['memory'] . ' MB (' . round($value['memory'] / 1024, 1) . ' GB)</li>' : '';
      $specs_output .= !empty($value['vcpus']) ? '<li>vCPUs: ' . $value['vcpus'] . '</li>' : '';
      $specs_output .= !empty($value['gpus']) && $value['gpus'] > 0 ? '<li>GPUs: ' . $value['gpus'] . '</li>' : '';
      $specs_output .= !empty($value['transfer']) ? '<li>Transfer: ' . $value['transfer'] . ' GB/month</li>' : '';
      $specs_output .= '</ul>';
      $rows[] = array(
        array('data' => '<strong>Specs</strong>'),
        $specs_output,
      );
    }
    elseif (is_array($value)) {
      $display_value = '<pre>' . check_plain(json_encode($value, JSON_PRETTY_PRINT)) . '</pre>';
      $rows[] = array(
        array('data' => '<strong>' . check_plain($key) . '</strong>'),
        $display_value,
      );
    }
    else {
      $rows[] = array(
        array('data' => '<strong>' . check_plain($key) . '</strong>'),
        check_plain($value),
      );
    }
  }

  if (!empty($rows)) {
    return theme('table', array('rows' => $rows));
  }
  return '';
}

/**
 * Refresh all servers and redirect back to dashboard.
 */
function dropmonitor_refresh_all() {
  if (!backdrop_valid_token($_GET['token'], 'dropmonitor_refresh')) {
    backdrop_access_denied();
    return;
  }

  module_load_include('inc', 'dropmonitor', 'dropmonitor.checks');
  dropmonitor_run_all_checks();
  backdrop_set_message(t('All server checks refreshed.'));
  backdrop_goto('drop/monitor');
}

/**
 * Refresh a single server and redirect back.
 */
function dropmonitor_refresh_server($server) {
  if (!backdrop_valid_token($_GET['token'], 'dropmonitor_refresh')) {
    backdrop_access_denied();
    return;
  }

  module_load_include('inc', 'dropmonitor', 'dropmonitor.checks');
  dropmonitor_check_server($server);
  backdrop_set_message(t('Checks refreshed for %label.', array('%label' => $server->label)));

  // Redirect back to where we came from.
  $destination = isset($_GET['destination']) ? $_GET['destination'] : 'drop/monitor';
  backdrop_goto($destination);
}

// -------------------------------------------------------------------------
// Theme helpers.
// -------------------------------------------------------------------------

/**
 * Renders a single metric item in the dashboard card.
 */
function _dropmonitor_metric_item($label, $value, $detail = '', $class = '') {
  $output = '<div class="dropmonitor-metric' . ($class ? ' ' . $class : '') . '">';
  $output .= '<span class="dropmonitor-metric-label">' . $label . '</span>';
  $output .= '<span class="dropmonitor-metric-value">' . $value . '</span>';
  if ($detail) {
    $output .= '<span class="dropmonitor-metric-detail">' . $detail . '</span>';
  }
  $output .= '</div>';
  return $output;
}

// -------------------------------------------------------------------------
// Agent format renderers for dashboard cards.
// -------------------------------------------------------------------------

/**
 * Renders server-level metrics for the standalone monitoring app format.
 */
function _dropmonitor_dashboard_standalone_metrics($agent, $config) {
  $output = '';
  $srv = $agent['server'];

  // Disk.
  if (isset($srv['disk_free_percent'])) {
    $used = 100 - $srv['disk_free_percent'];
    $disk_class = '';
    if ($srv['disk_free_percent'] <= $config->get('alert_disk_percent')) {
      $disk_class = 'dropmonitor-critical';
    }
    elseif ($srv['disk_free_percent'] <= $config->get('alert_disk_percent') * 2) {
      $disk_class = 'dropmonitor-warning';
    }
    $output .= _dropmonitor_metric_item(
      t('Disk'),
      $used . '% ' . t('used'),
      $srv['disk_free_percent'] . '% ' . t('free'),
      $disk_class
    );
  }

  // Memory.
  if (isset($srv['memory']['used_percent'])) {
    $mem = $srv['memory'];
    $mem_class = '';
    if ($mem['used_percent'] >= 90) {
      $mem_class = 'dropmonitor-warning';
    }
    $output .= _dropmonitor_metric_item(
      t('Memory'),
      $mem['used_percent'] . '% ' . t('used'),
      isset($mem['used_mb']) ? $mem['used_mb'] . ' / ' . $mem['total_mb'] . ' MB' : '',
      $mem_class
    );
  }

  // CPU.
  if (isset($srv['cpu_used_percent'])) {
    $cpu_class = '';
    if ($srv['cpu_used_percent'] >= 90) {
      $cpu_class = 'dropmonitor-warning';
    }
    $output .= _dropmonitor_metric_item(
      t('CPU'),
      $srv['cpu_used_percent'] . '%',
      '',
      $cpu_class
    );
  }

  // OS + Uptime.
  $os_details = array();
  if (!empty($srv['os'])) {
    $os_details[] = check_plain($srv['os']);
  }
  if (isset($srv['uptime_days'])) {
    $os_details[] = t('Up @days days', array('@days' => round($srv['uptime_days'], 1)));
  }
  if (!empty($os_details)) {
    $output .= _dropmonitor_metric_item(
      t('System'),
      array_shift($os_details),
      implode('<br>', $os_details)
    );
  }

  // Apt updates.
  if (isset($srv['apt_updates_available'])) {
    $output .= _dropmonitor_metric_item(
      t('Updates'),
      $srv['apt_updates_available'] . ' ' . t('pending'),
      ''
    );
  }

  // SSL certificates.
  if (!empty($srv['ssl_certificates'])) {
    $alert_cert_days = $config->get('alert_cert_days');
    $worst_days = NULL;
    $cert_count = count($srv['ssl_certificates']);
    foreach ($srv['ssl_certificates'] as $cert) {
      if (isset($cert['days_until_expiry'])) {
        if ($worst_days === NULL || $cert['days_until_expiry'] < $worst_days) {
          $worst_days = $cert['days_until_expiry'];
        }
      }
    }
    if ($worst_days !== NULL) {
      $cert_class = '';
      if ($worst_days <= 0) {
        $cert_class = 'dropmonitor-critical';
      }
      elseif ($worst_days <= $alert_cert_days) {
        $cert_class = 'dropmonitor-warning';
      }
      $output .= _dropmonitor_metric_item(
        t('SSL certs'),
        $worst_days . ' ' . t('days (nearest)'),
        format_plural($cert_count, '1 certificate', '@count certificates'),
        $cert_class
      );
    }
  }

  return $output;
}

/**
 * Renders the per-site summary table for the standalone monitoring app format.
 */
function _dropmonitor_dashboard_standalone_sites($agent, $sid = NULL) {
  if (empty($agent['sites'])) {
    return '';
  }

  $site_count = count($agent['sites']);
  $project_links = $sid ? dropmonitor_get_project_links($sid) : array();

  // Count sites with errors and update issues for the summary line.
  $error_count = 0;
  $security_count = 0;
  $update_count = 0;
  foreach ($agent['sites'] as $site) {
    if ((isset($site['http']['status']) && $site['http']['status'] === 'down')
      || !empty($site['watchdog_errors_24h'])
      || !empty($site['error'])) {
      $error_count++;
    }
    if (!empty($site['core_update']['security'])) {
      $security_count++;
    }
    elseif (!empty($site['core_update']['update_available'])) {
      $update_count++;
    }
  }

  // Build summary text.
  $summary_parts = array();
  $summary_parts[] = format_plural($site_count, '1 site', '@count sites');
  if ($error_count > 0) {
    $summary_parts[] = '<span class="dropmonitor-sites-errors-count">' . format_plural($error_count, '1 error', '@count errors') . '</span>';
  }
  if ($security_count > 0) {
    $summary_parts[] = '<span class="dropmonitor-sites-security-count">' . format_plural($security_count, '1 security update', '@count security updates') . '</span>';
  }
  if ($update_count > 0) {
    $summary_parts[] = '<span class="dropmonitor-sites-update-count">' . format_plural($update_count, '1 update', '@count updates') . '</span>';
  }
  $summary_text = implode(' &mdash; ', $summary_parts);

  $output = '<div class="dropmonitor-sites">';
  $output .= '<details class="dropmonitor-sites-details">';
  $output .= '<summary class="dropmonitor-sites-summary">' . $summary_text . '</summary>';
  $output .= '<table class="dropmonitor-sites-table"><tbody>';
  foreach ($agent['sites'] as $label => $site) {
    $row_class = '';

    // HTTP status indicator.
    if (isset($site['http']['status'])) {
      if ($site['http']['status'] === 'up') {
        $http_icon = '<span class="dropmonitor-site-up" title="' . t('Up') . '">&bull;</span>';
      }
      elseif ($site['http']['status'] === 'down') {
        $http_icon = '<span class="dropmonitor-site-down" title="' . t('Down') . '">&bull;</span>';
        $row_class = ' dropmonitor-site-row-down';
      }
      else {
        $http_icon = '<span class="dropmonitor-site-unknown" title="' . t('Unknown') . '">&bull;</span>';
      }
    }
    else {
      $http_icon = '';
    }

    // Site name with link — prefer project page, fall back to external URL.
    $site_name = check_plain($label);
    if (isset($project_links[$label])) {
      $site_name = l(check_plain($label), 'drop/projects/' . $project_links[$label]->pid);
    }
    elseif (!empty($site['http']['url'])) {
      $site_name = l(check_plain($label), $site['http']['url'], array('external' => TRUE));
    }

    // Details column.
    $details = array();
    if (isset($site['http']['http_code'])) {
      $details[] = 'HTTP ' . $site['http']['http_code'];
    }
    if (isset($site['http']['response_ms'])) {
      $details[] = $site['http']['response_ms'] . 'ms';
    }
    if (isset($site['db_size_mb'])) {
      $details[] = $site['db_size_mb'] . ' MB';
    }
    if (!empty($site['watchdog_errors_24h'])) {
      $details[] = '<span class="dropmonitor-site-errors">' . $site['watchdog_errors_24h'] . ' ' . t('errors') . '</span>';
    }
    if (!empty($site['error'])) {
      $details[] = '<span class="dropmonitor-site-errors" title="' . check_plain($site['error']) . '">' . t('DB error') . '</span>';
    }
    // Core update info.
    if (!empty($site['core_update']['update_available'])) {
      $update_text = check_plain($site['core_update']['current']) . ' &rarr; ' . check_plain($site['core_update']['latest']);
      if (!empty($site['core_update']['security'])) {
        $details[] = '<span class="dropmonitor-site-security" title="' . $update_text . '">' . t('Security update') . '</span>';
      }
      else {
        $details[] = '<span class="dropmonitor-site-update" title="' . $update_text . '">' . t('Update available') . '</span>';
      }
    }

    $output .= '<tr class="dropmonitor-site-row' . $row_class . '">';
    $output .= '<td class="dropmonitor-site-status">' . $http_icon . '</td>';
    $output .= '<td class="dropmonitor-site-name">' . $site_name . '</td>';
    $output .= '<td class="dropmonitor-site-details">' . implode(' &middot; ', $details) . '</td>';
    $output .= '</tr>';
  }
  $output .= '</tbody></table>';
  $output .= '</details>';
  $output .= '</div>';

  return $output;
}

/**
 * Renders dashboard metrics for the legacy per-site status.php format.
 */
function _dropmonitor_dashboard_legacy_agent($agent, $config) {
  $output = '';

  // Disk.
  if (isset($agent['disk_used_percent'])) {
    $free = round(100 - $agent['disk_used_percent'], 1);
    $disk_class = '';
    if ($free <= $config->get('alert_disk_percent')) {
      $disk_class = 'dropmonitor-critical';
    }
    elseif ($free <= $config->get('alert_disk_percent') * 2) {
      $disk_class = 'dropmonitor-warning';
    }
    $output .= _dropmonitor_metric_item(
      t('Disk'),
      $agent['disk_used_percent'] . '% ' . t('used'),
      $agent['disk_free_gb'] . 'GB ' . t('free'),
      $disk_class
    );
  }

  // Updates.
  if (isset($agent['pending_updates'])) {
    $update_class = '';
    if (!empty($agent['pending_security'])) {
      $update_class = 'dropmonitor-warning';
    }
    $update_detail = '';
    if (!empty($agent['pending_security'])) {
      $update_detail = $agent['pending_security'] . ' ' . t('security');
    }
    $output .= _dropmonitor_metric_item(
      t('Updates'),
      $agent['pending_updates'] . ' ' . t('pending'),
      $update_detail,
      $update_class
    );
  }

  // PHP + Backdrop + OS version.
  if (!empty($agent['php_version']) || !empty($agent['backdrop_version'])) {
    $php_short = !empty($agent['php_version']) ? preg_replace('/[-+].*$/', '', $agent['php_version']) : '';
    $value = $php_short ? 'PHP ' . $php_short : 'Backdrop ' . $agent['backdrop_version'];
    $details = array();
    if ($php_short && !empty($agent['backdrop_version'])) {
      $details[] = 'Backdrop ' . $agent['backdrop_version'];
    }
    if (!empty($agent['os_version'])) {
      $details[] = check_plain($agent['os_version']);
    }
    $output .= _dropmonitor_metric_item(t('Stack'), $value, implode('<br>', $details));
  }

  // Cron.
  if (isset($agent['cron_age_hours']) && $agent['cron_age_hours'] !== NULL) {
    $cron_class = '';
    if ($agent['cron_age_hours'] > 24) {
      $cron_class = 'dropmonitor-warning';
    }
    $output .= _dropmonitor_metric_item(
      t('Cron'),
      $agent['cron_age_hours'] . 'h ' . t('ago'),
      '',
      $cron_class
    );
  }

  // DB size.
  if (isset($agent['db_size_mb'])) {
    $output .= _dropmonitor_metric_item(t('Database'), $agent['db_size_mb'] . ' MB', '');
  }

  return $output;
}

// -------------------------------------------------------------------------
// Uptime monitoring pages.
// -------------------------------------------------------------------------

/**
 * Uptime dashboard — shows all projects with URLs and their current status.
 */
function dropmonitor_uptime_dashboard() {
  backdrop_set_title(t('Uptime Monitor'));

  module_load_include('inc', 'dropmonitor', 'dropmonitor.uptime');

  // Get all active projects with site URLs.
  $projects = db_select('drop_project', 'p')
    ->fields('p')
    ->condition('p.active', 1)
    ->condition('p.site_url', '', '!=')
    ->orderBy('p.name')
    ->execute()
    ->fetchAll();

  if (empty($projects)) {
    return '<p>' . t('No projects have a Website URL configured. Add a URL to a project to enable uptime monitoring.') . '</p>';
  }

  $latest = dropmonitor_uptime_latest();

  // Check for projects with incomplete monitoring settings.
  $incomplete = array();
  foreach ($projects as $project) {
    if (empty($project->platform)) {
      $incomplete[] = l(check_plain($project->name), 'drop/projects/' . $project->pid . '/edit');
    }
  }

  $header = array(
    t('Project'),
    t('URL'),
    t('Status'),
    t('Response'),
    t('Uptime (14d)'),
    t('Last checked'),
  );

  $rows = array();
  foreach ($projects as $project) {
    $pid = $project->pid;
    $pct = dropmonitor_uptime_percentage($pid, 14);

    if (isset($latest[$pid])) {
      $check = $latest[$pid];
      $is_up = (int) $check->status;
      $reason = _dropmonitor_http_reason((int) $check->response_code);
      $status_html = $is_up
        ? '<span class="dropmonitor-up">' . t('UP') . '</span>'
        : '<span class="dropmonitor-down">' . t('DOWN') . '</span>';
      $response = $check->response_code . ' <span class="dropmonitor-metric-detail">(' . $check->response_ms . 'ms)</span>';
      if (!$is_up) {
        $response .= '<br><span class="dropmonitor-metric-detail">' . $reason . '</span>';
      }
      $checked = format_interval(REQUEST_TIME - $check->checked, 1) . ' ' . t('ago');
    }
    else {
      $status_html = '<span class="dropmonitor-status-unknown">' . t('No data') . '</span>';
      $response = '—';
      $checked = t('Never');
    }

    $pct_html = '—';
    if ($pct !== NULL) {
      $pct_class = $pct >= 99.5 ? 'dropmonitor-up' : ($pct >= 95 ? 'dropmonitor-warning-text' : 'dropmonitor-down');
      $pct_html = '<span class="' . $pct_class . '">' . $pct . '%</span>';
    }

    $actions = '';
    if (user_access('administer dropmonitor')) {
      $actions = l(t('Check now'), 'drop/monitor/uptime/' . $pid . '/check', array(
        'query' => array('token' => backdrop_get_token('dropmonitor_check')),
        'attributes' => array('class' => array('dropmonitor-check-now')),
      ));
      $actions .= ' ' . l(icon('gear', array('attributes' => array('width' => 13, 'height' => 13))), 'drop/projects/' . $pid . '/edit', array(
        'html' => TRUE,
        'attributes' => array('title' => t('Edit project settings')),
        'query' => array('destination' => 'drop/monitor/uptime'),
      ));
    }

    // Platform/mode indicator.
    $platform_labels = drop_base_platform_options();
    $platform_text = '';
    if (!empty($project->platform) && isset($platform_labels[$project->platform])) {
      $platform_text = '<span class="dropmonitor-metric-detail">' . check_plain($platform_labels[$project->platform]);
      if (!empty($project->gentle_monitor)) {
        $platform_text .= ' · ' . t('Gentle');
      }
      $platform_text .= '</span>';
    }

    $rows[] = array(
      l(check_plain($project->name), 'drop/monitor/uptime/' . $pid) . ($platform_text ? '<br>' . $platform_text : ''),
      '<span class="dropmonitor-metric-detail">' . l(check_plain($project->site_url), $project->site_url, array('external' => TRUE)) . '</span>',
      $status_html,
      $response,
      $pct_html,
      '<span class="dropmonitor-metric-detail">' . $checked . '</span>',
      $actions,
    );
  }

  $header[] = '';

  $output = '';

  if (!empty($incomplete) && user_access('administer dropmonitor')) {
    $output .= '<div class="messages warning">'
      . '<strong>' . t('Incomplete monitoring settings') . '</strong><br>'
      . t('The following projects have no platform set. Monitoring will use a basic homepage check until configured: !list', array(
          '!list' => implode(', ', $incomplete),
        ))
      . '</div>';
  }

  $output .= theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('class' => array('dropmonitor-uptime-table')),
  ));

  $output .= '<p class="dropmonitor-report-count">' . format_plural(count($projects), '1 project monitored', '@count projects monitored') . '</p>';

  return $output;
}

/**
 * Uptime detail page for a single project.
 */
function dropmonitor_uptime_detail_page($pid) {
  $pid = (int) $pid;

  module_load_include('inc', 'dropmonitor', 'dropmonitor.uptime');

  $project = db_select('drop_project', 'p')
    ->fields('p', array('pid', 'name', 'site_url'))
    ->condition('pid', $pid)
    ->execute()
    ->fetchObject();

  if (!$project) {
    return MENU_NOT_FOUND;
  }

  backdrop_set_title(t('Uptime: @name', array('@name' => $project->name)));

  $pct = dropmonitor_uptime_percentage($pid, 14);

  $output = '<div class="dropmonitor-detail">';

  // Summary.
  $output .= '<p>';
  $output .= t('URL: ') . l(check_plain($project->site_url), $project->site_url, array('external' => TRUE));
  if ($pct !== NULL) {
    $output .= ' &middot; ' . t('14-day uptime: <strong>@pct%</strong>', array('@pct' => $pct));
  }
  $output .= '</p>';

  // Recent checks log.
  $checks = db_select('dropmonitor_uptime', 'u')
    ->fields('u')
    ->condition('pid', $pid)
    ->orderBy('checked', 'DESC')
    ->range(0, 200)
    ->execute()
    ->fetchAll();

  if (empty($checks)) {
    $output .= '<p>' . t('No uptime data recorded yet.') . '</p>';
  }
  else {
    $header = array(t('Time'), t('Status'), t('HTTP'), t('Reason'), t('Response time'));
    $rows = array();
    foreach ($checks as $check) {
      $status_html = $check->status
        ? '<span class="dropmonitor-up">' . t('UP') . '</span>'
        : '<span class="dropmonitor-down">' . t('DOWN') . '</span>';
      $reason = $check->status ? '' : _dropmonitor_http_reason((int) $check->response_code);

      $row_class = $check->status ? array() : array('dropmonitor-row-down');
      $rows[] = array(
        'data' => array(
          format_date($check->checked, 'short'),
          $status_html,
          $check->response_code,
          $reason,
          $check->response_ms . 'ms',
        ),
        'class' => $row_class,
      );
    }
    $output .= theme('table', array(
      'header' => $header,
      'rows' => $rows,
      'attributes' => array('class' => array('dropmonitor-uptime-log')),
    ));
  }

  $output .= '</div>';

  // Back link.
  $output .= '<p>' . l(t('Back to uptime dashboard'), 'drop/monitor/uptime') . '</p>';

  return $output;
}

/**
 * "Check now" callback — runs an immediate uptime check for one project.
 *
 * Clears alerts and banner state if the site is back up.
 */
function dropmonitor_uptime_check_now($pid) {
  $pid = (int) $pid;

  if (!isset($_GET['token']) || !backdrop_valid_token($_GET['token'], 'dropmonitor_check')) {
    backdrop_access_denied();
    return;
  }

  $project = db_select('drop_project', 'p')
    ->fields('p', array('pid', 'name', 'site_url'))
    ->condition('pid', $pid)
    ->execute()
    ->fetchObject();

  if (!$project || empty($project->site_url)) {
    backdrop_set_message(t('Project not found or has no URL.'), 'error');
    backdrop_goto('drop/monitor/uptime');
    return;
  }

  module_load_include('inc', 'dropmonitor', 'dropmonitor.uptime');
  _dropmonitor_check_uptime($project);

  // Report result.
  $latest = db_select('dropmonitor_uptime', 'u')
    ->fields('u')
    ->condition('pid', $pid)
    ->orderBy('checked', 'DESC')
    ->range(0, 1)
    ->execute()
    ->fetchObject();

  if ($latest && $latest->status) {
    backdrop_set_message(t('%name is UP (HTTP @code, @ms ms).', array(
      '%name' => $project->name,
      '@code' => $latest->response_code,
      '@ms' => $latest->response_ms,
    )));
  }
  else {
    $code = $latest ? $latest->response_code : 0;
    backdrop_set_message(t('%name is still DOWN (HTTP @code).', array(
      '%name' => $project->name,
      '@code' => $code,
    )), 'warning');
  }

  backdrop_goto('drop/monitor/uptime');
}

// -------------------------------------------------------------------------
// Match agent sites to projects.
// -------------------------------------------------------------------------

/**
 * Page callback: shows agent sites alongside projects for linking.
 *
 * Compares site URLs from agent data against project site_url values to
 * suggest matches. Shows already-linked pairs, auto-matched suggestions,
 * and unmatched agent sites (as a checklist for creating new projects).
 */
function dropmonitor_match_sites_page() {
  backdrop_set_title(t('Match sites to projects'));

  $output = '';

  // Gather all agent site data across all servers.
  $servers = dropmonitor_get_servers(TRUE);
  $agent_sites = array();
  foreach ($servers as $server) {
    $check = db_select('dropmonitor_check', 'c')
      ->fields('c', array('data'))
      ->condition('sid', $server->sid)
      ->condition('check_type', 'agent')
      ->execute()
      ->fetchField();

    if (!$check) {
      continue;
    }
    $data = json_decode($check, TRUE);
    if (empty($data['sites'])) {
      continue;
    }
    foreach ($data['sites'] as $key => $site) {
      $agent_sites[] = array(
        'sid' => $server->sid,
        'server_label' => $server->label,
        'site_key' => $key,
        'url' => !empty($site['http']['url']) ? $site['http']['url'] : '',
        'cms' => !empty($site['cms']) ? $site['cms'] : '',
        'site' => $site,
      );
    }
  }

  if (empty($agent_sites)) {
    return '<p>' . t('No agent data available. Run a server check first.') . '</p>';
  }

  // Load all projects.
  $projects = db_select('drop_project', 'p')
    ->fields('p')
    ->orderBy('name')
    ->execute()
    ->fetchAllAssoc('pid');

  // Build lookup: which projects are already linked.
  $linked = array();
  foreach ($projects as $project) {
    if (!empty($project->server_sid) && !empty($project->agent_site_key)) {
      $linked[$project->server_sid . '::' . $project->agent_site_key] = $project;
    }
  }

  // Build URL-based lookup for auto-matching suggestions.
  $project_by_url = array();
  foreach ($projects as $project) {
    if (!empty($project->site_url)) {
      $normalized = _dropmonitor_normalize_url($project->site_url);
      $project_by_url[$normalized] = $project;
    }
  }

  // Categorize agent sites.
  $rows_linked = array();
  $rows_suggested = array();
  $rows_unmatched = array();

  foreach ($agent_sites as $asite) {
    $compound_key = $asite['sid'] . '::' . $asite['site_key'];

    if (isset($linked[$compound_key])) {
      // Already linked.
      $project = $linked[$compound_key];
      $rows_linked[] = array(
        l(check_plain($project->name), 'drop/projects/' . $project->pid),
        check_plain($asite['site_key']),
        check_plain($asite['server_label']),
        !empty($asite['url']) ? l(check_plain($asite['url']), $asite['url'], array('external' => TRUE)) : '—',
        check_plain($asite['cms']),
        '<span class="dropmonitor-up">' . t('Linked') . '</span>',
      );
    }
    else {
      // Try URL match.
      $match = NULL;
      if (!empty($asite['url'])) {
        $normalized = _dropmonitor_normalize_url($asite['url']);
        if (isset($project_by_url[$normalized])) {
          $candidate = $project_by_url[$normalized];
          // Only suggest if not already linked to a different site.
          if (empty($candidate->server_sid)) {
            $match = $candidate;
          }
        }
      }

      if ($match) {
        $link_url = url('drop/monitor/match-sites/link', array(
          'query' => array(
            'pid' => $match->pid,
            'sid' => $asite['sid'],
            'key' => $asite['site_key'],
            'token' => backdrop_get_token('dropmonitor_match'),
          ),
        ));
        $rows_suggested[] = array(
          l(check_plain($match->name), 'drop/projects/' . $match->pid),
          check_plain($asite['site_key']),
          check_plain($asite['server_label']),
          !empty($asite['url']) ? l(check_plain($asite['url']), $asite['url'], array('external' => TRUE)) : '—',
          check_plain($asite['cms']),
          '<a href="' . $link_url . '" class="button button-small">' . t('Link') . '</a>',
        );
      }
      else {
        $rows_unmatched[] = array(
          '—',
          check_plain($asite['site_key']),
          check_plain($asite['server_label']),
          !empty($asite['url']) ? l(check_plain($asite['url']), $asite['url'], array('external' => TRUE)) : '—',
          check_plain($asite['cms']),
          l(t('Create project'), 'drop/projects/add', array(
            'query' => array('destination' => 'drop/monitor/match-sites'),
          )),
        );
      }
    }
  }

  $header = array(t('Project'), t('Agent site'), t('Server'), t('URL'), t('CMS'), t('Action'));

  // Already linked.
  if (!empty($rows_linked)) {
    $output .= '<h3>' . t('Linked (@count)', array('@count' => count($rows_linked))) . '</h3>';
    $output .= theme('table', array('header' => $header, 'rows' => $rows_linked));
  }

  // Suggested matches.
  if (!empty($rows_suggested)) {
    $output .= '<h3>' . t('Suggested matches (@count)', array('@count' => count($rows_suggested))) . '</h3>';
    $output .= '<p>' . t('These agent sites have URLs matching an existing project. Click "Link" to connect them.') . '</p>';
    $output .= theme('table', array('header' => $header, 'rows' => $rows_suggested));

    // Link all button.
    $link_all_url = url('drop/monitor/match-sites/link', array(
      'query' => array(
        'all' => 1,
        'token' => backdrop_get_token('dropmonitor_match'),
      ),
    ));
    $output .= '<p><a href="' . $link_all_url . '" class="button">' . t('Link all suggested matches') . '</a></p>';
  }

  // Unmatched.
  if (!empty($rows_unmatched)) {
    $output .= '<h3>' . t('Unmatched agent sites (@count)', array('@count' => count($rows_unmatched))) . '</h3>';
    $output .= '<p>' . t('These monitored sites do not match any project. Create a project for each one you want to track.') . '</p>';
    $output .= theme('table', array('header' => $header, 'rows' => $rows_unmatched));
  }

  // Summary.
  $total = count($rows_linked) + count($rows_suggested) + count($rows_unmatched);
  $output .= '<p class="dropmonitor-report-count">'
    . t('@total agent sites: @linked linked, @suggested suggested, @unmatched unmatched', array(
      '@total' => $total,
      '@linked' => count($rows_linked),
      '@suggested' => count($rows_suggested),
      '@unmatched' => count($rows_unmatched),
    ))
    . '</p>';

  return $output;
}

/**
 * Callback: links an agent site to a project (or links all suggested matches).
 */
function dropmonitor_match_sites_link() {
  if (!isset($_GET['token']) || !backdrop_valid_token($_GET['token'], 'dropmonitor_match')) {
    backdrop_access_denied();
    return;
  }

  // Link all suggested matches.
  if (!empty($_GET['all'])) {
    $count = _dropmonitor_link_all_suggested();
    backdrop_set_message(t('Linked @count sites to projects.', array('@count' => $count)));
    backdrop_goto('drop/monitor/match-sites');
    return;
  }

  $pid = isset($_GET['pid']) ? (int) $_GET['pid'] : 0;
  $sid = isset($_GET['sid']) ? (int) $_GET['sid'] : 0;
  $key = isset($_GET['key']) ? $_GET['key'] : '';

  if (!$pid || !$sid || $key === '') {
    backdrop_set_message(t('Missing parameters.'), 'error');
    backdrop_goto('drop/monitor/match-sites');
    return;
  }

  db_update('drop_project')
    ->fields(array(
      'server_sid' => $sid,
      'agent_site_key' => $key,
    ))
    ->condition('pid', $pid)
    ->execute();

  $project = db_select('drop_project', 'p')
    ->fields('p', array('name'))
    ->condition('pid', $pid)
    ->execute()
    ->fetchField();

  backdrop_set_message(t('Linked %project to agent site %key.', array(
    '%project' => $project,
    '%key' => $key,
  )));
  backdrop_goto('drop/monitor/match-sites');
}

/**
 * Links all URL-matched agent sites to their corresponding projects.
 *
 * @return int
 *   Number of links created.
 */
function _dropmonitor_link_all_suggested() {
  $servers = dropmonitor_get_servers(TRUE);
  $count = 0;

  // Build project URL lookup.
  $projects = db_select('drop_project', 'p')
    ->fields('p')
    ->execute()
    ->fetchAllAssoc('pid');

  $project_by_url = array();
  foreach ($projects as $project) {
    if (!empty($project->site_url) && empty($project->server_sid)) {
      $normalized = _dropmonitor_normalize_url($project->site_url);
      $project_by_url[$normalized] = $project;
    }
  }

  foreach ($servers as $server) {
    $check = db_select('dropmonitor_check', 'c')
      ->fields('c', array('data'))
      ->condition('sid', $server->sid)
      ->condition('check_type', 'agent')
      ->execute()
      ->fetchField();

    if (!$check) {
      continue;
    }
    $data = json_decode($check, TRUE);
    if (empty($data['sites'])) {
      continue;
    }

    foreach ($data['sites'] as $key => $site) {
      if (empty($site['http']['url'])) {
        continue;
      }
      $normalized = _dropmonitor_normalize_url($site['http']['url']);
      if (isset($project_by_url[$normalized])) {
        $project = $project_by_url[$normalized];
        db_update('drop_project')
          ->fields(array(
            'server_sid' => $server->sid,
            'agent_site_key' => $key,
          ))
          ->condition('pid', $project->pid)
          ->execute();
        // Remove from lookup so it's not matched again.
        unset($project_by_url[$normalized]);
        $count++;
      }
    }
  }

  return $count;
}

/**
 * Normalizes a URL for comparison: lowercase, strip protocol, www, trailing slash.
 */
function _dropmonitor_normalize_url($url) {
  $url = strtolower(trim($url));
  $url = preg_replace('#^https?://#', '', $url);
  $url = preg_replace('#^www\.#', '', $url);
  $url = rtrim($url, '/');
  return $url;
}
