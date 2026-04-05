<?php
/**
 * @file
 * Admin forms for Drop Monitor: settings, server management.
 */

/**
 * Settings form.
 */
function dropmonitor_settings_form($form, &$form_state) {
  $config = config('dropmonitor.settings');

  $form['linode_api_token'] = array(
    '#type' => 'textfield',
    '#title' => t('Linode API token'),
    '#description' => t('Read-only API token from cloud.linode.com. Leave empty to skip Linode API checks.'),
    '#default_value' => $config->get('linode_api_token'),
    '#maxlength' => 255,
  );

  $uptime_token = $config->get('uptime_token');
  $form['uptime_token'] = array(
    '#type' => 'textfield',
    '#title' => t('Uptime check token'),
    '#description' => t('Shared secret for the uptime check endpoint. Add this to your system crontab: <code>*/5 * * * * curl -s "@url?token=YOUR_TOKEN"</code>', array(
      '@url' => url('drop/monitor/uptime-check', array('absolute' => TRUE)),
    )),
    '#default_value' => $uptime_token,
    '#maxlength' => 128,
  );

  $form['slack_alert_channel'] = array(
    '#type' => 'textfield',
    '#title' => t('Slack alert channel ID'),
    '#description' => t('Channel for immediate outage notifications (e.g. #support-notifications). Find the ID in channel details.'),
    '#default_value' => $config->get('slack_alert_channel'),
    '#maxlength' => 32,
    '#placeholder' => 'C0123ABC456',
  );

  $form['slack_escalation_channel'] = array(
    '#type' => 'textfield',
    '#title' => t('Slack escalation channel ID'),
    '#description' => t('Channel for sustained outage alerts after 30 minutes (e.g. #escalated-notifications). Set audio notifications on this channel.'),
    '#default_value' => $config->get('slack_escalation_channel'),
    '#maxlength' => 32,
    '#placeholder' => 'C0123ABC456',
  );

  $form['check_interval'] = array(
    '#type' => 'select',
    '#title' => t('Check interval'),
    '#description' => t('How often to run automated checks via cron.'),
    '#options' => array(
      900 => t('15 minutes'),
      1800 => t('30 minutes'),
      3600 => t('1 hour'),
      7200 => t('2 hours'),
      21600 => t('6 hours'),
      43200 => t('12 hours'),
    ),
    '#default_value' => $config->get('check_interval'),
  );

  $form['thresholds'] = array(
    '#type' => 'fieldset',
    '#title' => t('Alert thresholds'),
    '#collapsible' => TRUE,
  );

  $form['thresholds']['alert_disk_percent'] = array(
    '#type' => 'number',
    '#title' => t('Disk space warning (% free remaining)'),
    '#description' => t('Show warning when free disk space falls below this percentage.'),
    '#default_value' => $config->get('alert_disk_percent'),
    '#min' => 1,
    '#max' => 50,
    '#field_suffix' => '%',
  );

  $form['thresholds']['alert_cert_days'] = array(
    '#type' => 'number',
    '#title' => t('SSL certificate warning (days before expiry)'),
    '#default_value' => $config->get('alert_cert_days'),
    '#min' => 1,
    '#max' => 90,
    '#field_suffix' => t('days'),
  );

  $form['thresholds']['alert_response_ms'] = array(
    '#type' => 'number',
    '#title' => t('Response time warning (milliseconds)'),
    '#description' => t('Show warning when site response exceeds this threshold.'),
    '#default_value' => $config->get('alert_response_ms'),
    '#min' => 500,
    '#max' => 30000,
    '#field_suffix' => 'ms',
  );

  $form['thresholds']['failure_threshold'] = array(
    '#type' => 'select',
    '#title' => t('Consecutive failures before DOWN'),
    '#description' => t('How many consecutive failed checks are required before a site is marked DOWN and alerts are sent. Prevents false alarms from transient blips.'),
    '#options' => array(
      1 => t('1 (immediate)'),
      2 => t('2 consecutive'),
      3 => t('3 consecutive (recommended)'),
      4 => t('4 consecutive'),
      5 => t('5 consecutive'),
    ),
    '#default_value' => $config->get('failure_threshold') ?: 3,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save settings'),
  );

  return $form;
}

/**
 * Settings form submit.
 */
function dropmonitor_settings_form_submit($form, &$form_state) {
  $config = config('dropmonitor.settings');
  $config->set('linode_api_token', $form_state['values']['linode_api_token']);
  $config->set('uptime_token', $form_state['values']['uptime_token']);
  $config->set('slack_alert_channel', $form_state['values']['slack_alert_channel']);
  $config->set('slack_escalation_channel', $form_state['values']['slack_escalation_channel']);
  $config->set('check_interval', $form_state['values']['check_interval']);
  $config->set('alert_disk_percent', $form_state['values']['alert_disk_percent']);
  $config->set('alert_cert_days', $form_state['values']['alert_cert_days']);
  $config->set('alert_response_ms', $form_state['values']['alert_response_ms']);
  $config->save();
  backdrop_set_message(t('Settings saved.'));
}

// -------------------------------------------------------------------------
// Server list.
// -------------------------------------------------------------------------

/**
 * Server list page.
 */
function dropmonitor_servers_list() {
  $output = '';

  // Sync button.
  $output .= '<div style="margin-bottom: 20px;">';
  $output .= l(t('Sync from Linode API'), 'drop/monitor/servers/sync', array(
    'query' => array('token' => backdrop_get_token('dropmonitor-sync')),
    'attributes' => array('class' => array('button', 'button-primary')),
  ));
  $output .= '</div>';

  $servers = dropmonitor_get_servers(FALSE);

  $header = array(
    t('Label'),
    t('Site URL'),
    t('Linode ID'),
    t('Active'),
    t('Actions'),
  );

  $rows = array();
  foreach ($servers as $server) {
    $actions = array(
      l(t('Edit'), 'drop/monitor/servers/' . $server->sid . '/edit'),
      l(t('Delete'), 'drop/monitor/servers/' . $server->sid . '/delete'),
    );
    $rows[] = array(
      check_plain($server->label),
      l($server->site_url, $server->site_url, array('external' => TRUE)),
      $server->linode_id ? $server->linode_id : '—',
      $server->active ? t('Yes') : t('No'),
      implode(' | ', $actions),
    );
  }

  if (empty($rows)) {
    $rows[] = array(array(
      'data' => t('No servers configured. !add', array('!add' => l(t('Add a server'), 'drop/monitor/servers/add'))),
      'colspan' => 5,
    ));
  }

  $output .= theme('table', array('header' => $header, 'rows' => $rows));
  return $output;
}

// -------------------------------------------------------------------------
// Server add/edit form.
// -------------------------------------------------------------------------

/**
 * Server add/edit form.
 */
function dropmonitor_server_form($form, &$form_state, $server = NULL) {
  $form['#server'] = $server;

  $form['label'] = array(
    '#type' => 'textfield',
    '#title' => t('Label'),
    '#description' => t('A name for this server (e.g. "Production — Linode 1").'),
    '#default_value' => $server ? $server->label : '',
    '#required' => TRUE,
    '#maxlength' => 255,
  );

  $form['site_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Site URL'),
    '#description' => t('Primary URL for HTTP checks (e.g. https://example.com).'),
    '#default_value' => $server ? $server->site_url : '',
    '#required' => TRUE,
    '#maxlength' => 255,
  );

  $form['agent_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Status agent URL'),
    '#description' => t('URL to the status.php agent script on this server. Leave empty if not deployed yet.'),
    '#default_value' => $server ? $server->agent_url : '',
    '#maxlength' => 255,
  );

  $form['agent_token'] = array(
    '#type' => 'textfield',
    '#title' => t('Agent token'),
    '#description' => t('Shared secret matching the DROPMONITOR_TOKEN in status.php on the remote server.'),
    '#default_value' => $server ? $server->agent_token : '',
    '#maxlength' => 128,
  );

  $form['origin_ip'] = array(
    '#type' => 'textfield',
    '#title' => t('Origin IP'),
    '#description' => t('If this server is behind Cloudflare or another CDN, enter the origin server IP here. Agent checks will hit the IP directly, bypassing the CDN.'),
    '#default_value' => $server ? $server->origin_ip : '',
    '#maxlength' => 45,
  );

  $form['linode_id'] = array(
    '#type' => 'number',
    '#title' => t('Linode ID'),
    '#description' => t('Linode instance ID for API checks. Leave empty to skip Linode API checks for this server.'),
    '#default_value' => $server ? $server->linode_id : '',
    '#min' => 0,
  );

  $form['active'] = array(
    '#type' => 'checkbox',
    '#title' => t('Actively monitor this server'),
    '#default_value' => $server ? $server->active : 1,
  );

  $form['weight'] = array(
    '#type' => 'number',
    '#title' => t('Weight'),
    '#description' => t('Lower numbers appear first on the dashboard.'),
    '#default_value' => $server ? $server->weight : 0,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => $server ? t('Update server') : t('Add server'),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => 'drop/monitor/servers',
  );

  return $form;
}

/**
 * Server form submit.
 */
function dropmonitor_server_form_submit($form, &$form_state) {
  $server = $form['#server'];
  $values = $form_state['values'];

  $fields = array(
    'label' => $values['label'],
    'site_url' => $values['site_url'],
    'agent_url' => $values['agent_url'],
    'agent_token' => $values['agent_token'],
    'origin_ip' => $values['origin_ip'],
    'linode_id' => !empty($values['linode_id']) ? (int) $values['linode_id'] : NULL,
    'active' => (int) $values['active'],
    'weight' => (int) $values['weight'],
  );

  if ($server) {
    db_update('dropmonitor_server')
      ->fields($fields)
      ->condition('sid', $server->sid)
      ->execute();
    backdrop_set_message(t('Server %label updated.', array('%label' => $values['label'])));
  }
  else {
    $fields['created'] = REQUEST_TIME;
    db_insert('dropmonitor_server')
      ->fields($fields)
      ->execute();
    backdrop_set_message(t('Server %label added.', array('%label' => $values['label'])));
  }

  $form_state['redirect'] = 'drop/monitor/servers';
}

// -------------------------------------------------------------------------
// Server delete form.
// -------------------------------------------------------------------------

/**
 * Server delete confirmation form.
 */
function dropmonitor_server_delete_form($form, &$form_state, $server) {
  $form['#server'] = $server;
  return confirm_form(
    $form,
    t('Delete server %label?', array('%label' => $server->label)),
    'drop/monitor/servers',
    t('This will remove the server and all its check history. This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Server delete submit.
 */
function dropmonitor_server_delete_form_submit($form, &$form_state) {
  $server = $form['#server'];
  db_delete('dropmonitor_check')->condition('sid', $server->sid)->execute();
  db_delete('dropmonitor_server')->condition('sid', $server->sid)->execute();
  backdrop_set_message(t('Server %label deleted.', array('%label' => $server->label)));
  $form_state['redirect'] = 'drop/monitor/servers';
}

// -------------------------------------------------------------------------
// Sync from Linode API.
// -------------------------------------------------------------------------

/**
 * Callback for syncing servers from Linode API.
 */
function dropmonitor_sync_callback() {
  if (!isset($_GET['token']) || !backdrop_valid_token($_GET['token'], 'dropmonitor-sync')) {
    watchdog('dropmonitor', 'Sync attempt with invalid or missing CSRF token.', array(), WATCHDOG_WARNING);
    backdrop_access_denied();
    return;
  }

  $result = dropmonitor_sync_servers_from_linode();

  if (isset($result['error'])) {
    backdrop_set_message(t('Sync failed: @error', array('@error' => $result['error'])), 'error');
  }
  else {
    backdrop_set_message(t('Sync complete: Added @added servers, updated @updated servers.', array(
      '@added' => $result['added'],
      '@updated' => $result['updated'],
    )));
  }

  backdrop_goto('drop/monitor/servers');
}
