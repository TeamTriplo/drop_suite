<?php
/**
 * @file
 * Settings form for DropBridge.
 */

/**
 * Form: DropBridge settings.
 */
function dropbridge_settings_form($form, &$form_state) {
  $config = config('dropbridge.settings');

  $form['calendar'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Global Calendars'),
    '#description' => t('These iCal feeds appear on <strong>everyone\'s</strong> dashboard. For personal calendars that only appear on one person\'s dashboard, each person can add them under their own account settings. Both private and public iCal URLs are supported.'),
  );

  $form['calendar']['calendar_ics_url'] = array(
    '#type'          => 'textarea',
    '#title'         => t('iCal URL(s)'),
    '#default_value' => $config->get('calendar_ics_url'),
    '#rows'          => 5,
    '#description'   => t('One URL per line. Optionally add a label after a pipe character: <code>URL | Label</code> (e.g. <code>https://…/basic.ics | Wolves</code>). Without a label, events show as "Event". Private and public iCal URLs both work.'),
  );

  $form['calendar']['calendar_cache_seconds'] = array(
    '#type'          => 'select',
    '#title'         => t('Refresh interval'),
    '#default_value' => $config->get('calendar_cache_seconds') ?: 1800,
    '#options'       => array(
      300   => t('5 minutes'),
      900   => t('15 minutes'),
      1800  => t('30 minutes'),
      3600  => t('1 hour'),
      10800 => t('3 hours'),
      86400 => t('24 hours'),
    ),
    '#description'   => t('How often to re-fetch each calendar feed.'),
  );

  $form['slack'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Slack Integration'),
    '#description' => t('Used for direct-message notifications to individual team members. Create a Slack app at <a href="https://api.slack.com/apps" target="_blank">api.slack.com/apps</a>, add the <code>chat:write</code> scope under Bot Token Scopes, install it to your workspace, and paste the bot token here. Each person sets their own Slack member ID in their account settings.'),
  );

  $form['slack']['slack_bot_token'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Bot token'),
    '#default_value' => $config->get('slack_bot_token'),
    '#description'   => t('Starts with <code>xoxb-</code>. Leave blank to disable all Slack notifications.'),
    '#placeholder'   => 'xoxb-…',
    '#attributes'    => array('autocomplete' => 'off'),
  );

  $form['slack']['slack_debug_mode'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Debug mode'),
    '#default_value' => $config->get('slack_debug_mode'),
    '#description'   => t('When enabled, reminder checks ignore the scheduled time window and the "already sent today" guard — every cron run (or the test button below) will fire if no time has been logged. Disable when done testing.'),
  );

  $form['announcement'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Site Announcement'),
    '#description' => t('Displays a dismissible banner at the top of every page for all team members. Clear the message to remove the banner. Each new message resets dismissals — everyone will see it again.'),
  );

  $form['announcement']['announcement_text'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Message'),
    '#default_value' => $config->get('announcement_text'),
    '#rows'          => 3,
    '#description'   => t('Leave blank to show no announcement. Plain text only.'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save settings'),
  );
  $form['actions']['fetch'] = array(
    '#type'   => 'submit',
    '#value'  => t('Clear cache & fetch now'),
    '#submit' => array('dropbridge_settings_form_fetch'),
  );
  $form['actions']['test_reminder'] = array(
    '#type'   => 'submit',
    '#value'  => t('Send test reminder now'),
    '#submit' => array('dropbridge_settings_form_test_reminder'),
  );

  return $form;
}

/**
 * Submit handler for dropbridge_settings_form() — save settings.
 */
function dropbridge_settings_form_submit($form, &$form_state) {
  $config = config('dropbridge.settings');
  $config->set('calendar_ics_url', trim($form_state['values']['calendar_ics_url']));
  $config->set('calendar_cache_seconds', (int) $form_state['values']['calendar_cache_seconds']);
  $config->set('announcement_text', trim($form_state['values']['announcement_text']));
  $config->set('slack_bot_token', trim($form_state['values']['slack_bot_token']));
  $config->set('slack_debug_mode', !empty($form_state['values']['slack_debug_mode']));
  $config->save();

  cache_clear_all('dropbridge_calendar_', 'cache', TRUE);

  backdrop_set_message(t('Settings saved.'));
  $form_state['redirect'] = 'drop';
}

/**
 * Submit handler for the "Send test reminder now" button.
 *
 * Runs the time reminder check immediately for the current user, forcing
 * debug mode on regardless of the saved setting. Does not save form values.
 */
function dropbridge_settings_form_test_reminder($form, &$form_state) {
  global $user;
  _dropbridge_cron_time_reminders(TRUE);
  backdrop_set_message(t('Test reminder check complete. Check watchdog for details, and your Slack DM if a reminder was sent.'));
  $form_state['redirect'] = 'drop/settings';
}

/**
 * Submit handler for the "Clear cache & fetch now" button.
 *
 * Clears cached ICS data without saving any form changes, then sends the
 * user to the daily list where a fresh fetch happens immediately.
 */
function dropbridge_settings_form_fetch($form, &$form_state) {
  cache_clear_all('dropbridge_calendar_', 'cache', TRUE);
  backdrop_set_message(t('Calendar cache cleared. Fetching fresh data now.'));
  $form_state['redirect'] = 'drop';
}

/**
 * Form: permissions for all enabled Drop modules.
 *
 * Reuses core's user_admin_permissions theme and save logic, filtered to
 * modules whose machine name starts with "drop".
 */
function dropbridge_permissions_form($form, &$form_state) {
  $roles = user_roles(FALSE, NULL, TRUE);
  $form['roles'] = array('#type' => 'value', '#value' => $roles);

  $module_info = system_get_info('module');
  $options     = array();
  $status      = array();
  $idx         = 0;

  // Collect permissions from enabled drop* modules, sorted by display name.
  $drop_modules = array();
  foreach (module_implements('permission') as $module) {
    if ($module === 'dropbridge' || strpos($module, 'drop') === 0) {
      $drop_modules[$module] = isset($module_info[$module]['name']) ? $module_info[$module]['name'] : $module;
    }
  }
  asort($drop_modules);

  foreach ($drop_modules as $module => $display_name) {
    $permissions = module_invoke($module, 'permission');
    if (empty($permissions)) {
      continue;
    }

    // Module heading row (numeric key).
    $form['permission'][$idx] = array(
      '#markup' => $display_name,
      '#id'     => $module,
    );
    $idx++;

    foreach ($permissions as $perm => $perm_item) {
      $perm_item += array(
        'description'     => '',
        'restrict access' => FALSE,
        'warning'         => '',
      );
      $options[$perm] = '';
      $form['permission'][$perm] = array(
        '#type'        => 'item',
        '#markup'      => $perm_item['title'],
        '#description' => theme('user_permission_description', array('permission_item' => $perm_item)),
        '#id'          => drupal_html_id('permission-' . $perm),
      );
      foreach ($roles as $role_name => $role) {
        if (in_array($perm, $role->permissions)) {
          $status[$role_name][] = $perm;
        }
      }
    }
  }

  foreach ($roles as $role_name => $role) {
    $form['checkboxes'][$role_name] = array(
      '#type'          => 'checkboxes',
      '#options'       => $options,
      '#default_value' => isset($status[$role_name]) ? $status[$role_name] : array(),
      '#attributes'    => array('class' => array('role-' . $role_name)),
    );
    $form['role_names'][$role_name] = array(
      '#markup'      => check_plain($role->label),
      '#description' => filter_xss_admin($role->description),
      '#tree'        => TRUE,
    );
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save permissions'),
  );

  $form['#theme']    = 'user_admin_permissions';
  $form['#attached']['js'][] = backdrop_get_path('module', 'user') . '/js/user.permissions.js';

  return $form;
}

/**
 * Submit handler for dropbridge_permissions_form().
 */
function dropbridge_permissions_form_submit($form, &$form_state) {
  foreach ($form_state['values']['roles'] as $role_name => $role) {
    user_role_change_permissions($role->name, $form_state['values'][$role->name]);
  }
  backdrop_set_message(t('Permissions saved.'));
}

// ---------------------------------------------------------------------------
// Inbound email settings
// ---------------------------------------------------------------------------

/**
 * Form: inbound email settings.
 */
function dropbridge_inbound_settings_form($form, &$form_state) {
  $config = config('dropbridge.inbound');

  $form['enabled'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Enable inbound email processing'),
    '#default_value' => $config->get('enabled'),
    '#description'   => t('When enabled, the webhook endpoint accepts POSTs from SendGrid and creates or updates tickets.'),
  );

  // Auto-generate and immediately persist a token if none exists yet.
  $token = $config->get('webhook_token');
  if (empty($token)) {
    $token = backdrop_random_key(32);
    $config->set('webhook_token', $token);
    $config->save();
  }

  $webhook_url = url('dropbridge/inbound-email', array(
    'absolute' => TRUE,
    'query'    => array('token' => $token),
  ));

  $form['webhook_token'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Webhook token'),
    '#default_value' => $token,
    '#description'   => t('Shared secret appended as <code>?token=…</code> to the webhook URL. Regenerate by clearing this field and saving.'),
    '#required'      => TRUE,
    '#attributes'    => array('autocomplete' => 'off'),
  );

  $form['webhook_url_display'] = array(
    '#type'   => 'item',
    '#title'  => t('Webhook URL'),
    '#markup' => '<code>' . check_plain($webhook_url) . '</code>',
    '#description' => t('Enter this URL in the SendGrid Inbound Parse settings. Configure an MX record for the inbound subdomain first.'),
  );

  $form['forward_to'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Forward copy to'),
    '#default_value' => $config->get('forward_to'),
    '#description'   => t('Every inbound email is forwarded to this address with a link to the ticket. Leave blank to disable forwarding.'),
    '#placeholder'   => 'support@example.com',
    '#size'          => 40,
  );

  // Build project options.
  $project_options = array(0 => t('— None —'));
  if (module_exists('droptickets')) {
    $projects = db_select('drop_project', 'p')
      ->fields('p', array('pid', 'name'))
      ->orderBy('p.name')
      ->execute()
      ->fetchAllKeyed();
    $project_options += $projects;
  }

  $form['default_pid'] = array(
    '#type'          => 'select',
    '#title'         => t('Default project'),
    '#default_value' => (int) $config->get('default_pid'),
    '#options'       => $project_options,
    '#description'   => t('New tickets created from inbound email are assigned to this project. Choose "None" to leave unassigned.'),
  );

  $form['drop_unverified'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Drop emails from unverified senders'),
    '#default_value' => $config->get('drop_unverified'),
    '#description'   => t('When checked, emails from senders not on the allowlist are silently discarded. When unchecked, they create tickets marked as unverified.'),
  );

  $allowlist = $config->get('allowlist') ?: array();
  $form['allowlist'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Sender allowlist'),
    '#default_value' => implode("\n", $allowlist),
    '#rows'          => 6,
    '#description'   => t('One email address per line. If this list is empty, all senders are accepted (and all tickets are marked verified). If populated, only listed senders create verified tickets — others are either dropped or marked unverified depending on the setting above.'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save settings'),
  );

  return $form;
}

/**
 * Submit handler for dropbridge_inbound_settings_form().
 */
function dropbridge_inbound_settings_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $config = config('dropbridge.inbound');

  // Parse allowlist textarea into an array.
  $allowlist_raw = $values['allowlist'];
  $allowlist = array_values(array_filter(array_map('trim', explode("\n", $allowlist_raw))));

  $config->set('enabled', !empty($values['enabled']));
  $config->set('webhook_token', trim($values['webhook_token']));
  $config->set('forward_to', trim($values['forward_to']));
  $config->set('default_pid', (int) $values['default_pid']);
  $config->set('drop_unverified', !empty($values['drop_unverified']));
  $config->set('allowlist', $allowlist);
  $config->save();

  backdrop_set_message(t('Inbound email settings saved.'));
}

// ---------------------------------------------------------------------------
// Unverified tickets report
// ---------------------------------------------------------------------------

/**
 * Page callback: list all unverified tickets.
 */
function dropbridge_unverified_tickets_page() {
  if (!module_exists('droptickets')) {
    return t('DropTickets is not enabled.');
  }

  $header = array(
    t('Ticket'),
    t('From'),
    t('Created'),
    t('Actions'),
  );

  $query = db_select('drop_ticket', 't')
    ->fields('t', array('tid', 'title', 'inbound_email', 'created', 'pid', 'ticket_number'))
    ->condition('t.is_verified', 0)
    ->orderBy('t.created', 'DESC');

  $tickets = $query->execute()->fetchAll();

  $rows = array();
  foreach ($tickets as $t) {
    $ticket_label = '#' . ($t->pid ? $t->pid . '-' . $t->ticket_number : $t->tid);
    $actions  = array();
    $actions[] = l(t('View ticket'), 'drop/tickets/' . $t->tid);
    $actions[] = l(t('Verify sender'), 'drop/tickets/verify-sender/' . $t->tid,
      array('query' => array('destination' => 'drop/tickets/unverified')));

    $rows[] = array(
      l($ticket_label . ': ' . check_plain($t->title), 'drop/tickets/' . $t->tid),
      check_plain($t->inbound_email ?: '—'),
      format_date($t->created, 'short'),
      implode(' | ', $actions),
    );
  }

  $output  = '<h2>' . t('Unverified tickets') . '</h2>';
  $output .= '<p>' . t('These tickets were created by email senders not on the allowlist. Verify a sender to add them to the allowlist and mark all their tickets as verified.') . '</p>';
  $output .= theme('table', array(
    'header' => $header,
    'rows'   => $rows,
    'empty'  => t('No unverified tickets.'),
  ));

  return $output;
}

// ---------------------------------------------------------------------------
// Verify sender
// ---------------------------------------------------------------------------

/**
 * Page callback: add a ticket's inbound sender to the allowlist and mark
 * all their tickets as verified.
 *
 * @param int $tid
 *   The ticket ID whose sender should be verified.
 */
function dropbridge_verify_sender($tid) {
  $ticket = drop_ticket_load((int) $tid);
  if (!$ticket || empty($ticket->inbound_email)) {
    backdrop_set_message(t('Ticket not found or has no inbound email address.'), 'error');
    backdrop_goto('drop/tickets/unverified');
  }

  $email = strtolower(trim($ticket->inbound_email));

  // Add to allowlist (de-duplicate).
  $config    = config('dropbridge.inbound');
  $allowlist = $config->get('allowlist') ?: array();
  $allowlist_lower = array_map('strtolower', $allowlist);
  if (!in_array($email, $allowlist_lower)) {
    $allowlist[] = $email;
    $config->set('allowlist', $allowlist);
    $config->save();
  }

  // Mark all unverified tickets from this email as verified.
  $count = db_update('drop_ticket')
    ->fields(array('is_verified' => 1))
    ->condition('inbound_email', $email)
    ->condition('is_verified', 0)
    ->execute();

  backdrop_set_message(t('@email added to the allowlist. @count ticket(s) marked as verified.',
    array('@email' => $email, '@count' => $count)));

  backdrop_goto(isset($_GET['destination']) ? $_GET['destination'] : 'drop/tickets/unverified');
}
