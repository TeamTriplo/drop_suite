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
    if (isset($checks['agent'])) {
      $agent = json_decode($checks['agent']->data, TRUE);
      if (isset($agent['error'])) {
        $output .= _dropmonitor_metric_item(t('Agent'), t('Error'), $agent['error'], 'dropmonitor-error');
      }
      else {
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
          if ($agent['pending_security'] > 0) {
            $update_class = 'dropmonitor-warning';
          }
          $update_detail = '';
          if ($agent['pending_security'] > 0) {
            $update_detail = $agent['pending_security'] . ' ' . t('security');
          }
          $output .= _dropmonitor_metric_item(
            t('Updates'),
            $agent['pending_updates'] . ' ' . t('pending'),
            $update_detail,
            $update_class
          );
        }

        // PHP + Backdrop.
        $versions = array();
        if (!empty($agent['php_version'])) {
          $versions[] = 'PHP ' . $agent['php_version'];
        }
        if (!empty($agent['backdrop_version'])) {
          $versions[] = 'Backdrop ' . $agent['backdrop_version'];
        }
        if (!empty($versions)) {
          $output .= _dropmonitor_metric_item(t('Stack'), implode(', ', $versions), '');
        }

        // Cron.
        if (isset($agent['cron_age_hours'])) {
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
        $specs_detail = array();
        if (!empty($specs['vcpus'])) {
          $specs_detail[] = $specs['vcpus'] . ' vCPU' . ($specs['vcpus'] != 1 ? 's' : '');
        }
        if (!empty($specs['memory'])) {
          $specs_detail[] = round($specs['memory'] / 1024, 1) . ' GB RAM';
        }
        if (!empty($specs['disk'])) {
          $specs_detail[] = round($specs['disk'] / 1024, 0) . ' GB disk';
        }
        if (!empty($specs_detail)) {
          $output .= _dropmonitor_metric_item(
            t('Resources'),
            implode(', ', $specs_detail),
            ''
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
  $info_rows = array(
    array(t('Site URL'), l($server->site_url, $server->site_url, array('external' => TRUE))),
  );
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
      $rows = array();
      foreach ($data as $key => $value) {
        if ($type === 'linode' && $key === 'specs' && is_array($value)) {
          // Format Linode specs nicely.
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
          // Other arrays show as formatted JSON.
          $display_value = '<pre>' . check_plain(json_encode($value, JSON_PRETTY_PRINT)) . '</pre>';
          $rows[] = array(
            array('data' => '<strong>' . check_plain($key) . '</strong>'),
            $display_value,
          );
        }
        else {
          // Simple values.
          $rows[] = array(
            array('data' => '<strong>' . check_plain($key) . '</strong>'),
            check_plain($value),
          );
        }
      }
      $output .= theme('table', array('rows' => $rows));
    }
  }

  $output .= '</div>';

  return $output;
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
