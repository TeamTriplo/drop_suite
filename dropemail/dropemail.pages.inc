<?php
/**
 * @file
 * Page callbacks for Drop Email compose forms.
 */

// ---------------------------------------------------------------------------
// Contact email form
// ---------------------------------------------------------------------------

/**
 * Form: compose and send an email to a CRM contact.
 */
function dropemail_contact_form($form, &$form_state, $contact) {
  $form['#contact'] = $contact;
  $form['#attached']['css'][] = backdrop_get_path('module', 'dropemail') . '/dropemail.css';

  $email   = dropemail_get_contact_primary_email($contact);
  $default = dropemail_get_default_from();

  backdrop_set_title(t('Send email to @name', array('@name' => entity_label('crm_contact', $contact))));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('CRM'), 'drop/crm'),
    l(entity_label('crm_contact', $contact), 'drop/crm/contact/' . $contact->cid),
  ));

  $form['to_display'] = array(
    '#markup' => '<div class="form-item dropemail-to-display">'
      . '<label>' . t('To') . '</label>'
      . '<span class="dropemail-to-value">' . check_plain($email) . '</span>'
      . '</div>',
  );
  $form['to'] = array(
    '#type'  => 'hidden',
    '#value' => $email,
  );

  $form['from_email'] = array(
    '#type'          => 'textfield',
    '#title'         => t('From'),
    '#default_value' => $default['address'],
    '#description'   => t('Use your own email address to have replies come back to you (support address becomes Reply-To).'),
    '#required'      => TRUE,
  );

  $form['subject'] = array(
    '#type'     => 'textfield',
    '#title'    => t('Subject'),
    '#required' => TRUE,
    '#maxlength' => 500,
  );

  $form['body'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Message'),
    '#required'      => TRUE,
    '#rows'          => 10,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Send email'),
  );
  $form['actions']['cancel'] = array(
    '#type'       => 'link',
    '#title'      => t('Cancel'),
    '#href'       => 'drop/crm/contact/' . $contact->cid,
    '#attributes' => array('class' => array('button', 'button-secondary')),
  );

  return $form;
}

/**
 * Validation for dropemail_contact_form().
 */
function dropemail_contact_form_validate($form, &$form_state) {
  $from = trim($form_state['values']['from_email']);
  if (!valid_email_address($from)) {
    form_set_error('from_email', t('Please enter a valid From email address.'));
  }
}

/**
 * Submit handler for dropemail_contact_form().
 */
function dropemail_contact_form_submit($form, &$form_state) {
  $contact = $form['#contact'];

  $eid = dropemail_send_and_log(array(
    'to'         => $form_state['values']['to'],
    'from_email' => trim($form_state['values']['from_email']),
    'subject'    => $form_state['values']['subject'],
    'body'       => $form_state['values']['body'],
    'cid'        => $contact->cid,
  ));

  if ($eid) {
    backdrop_set_message(t('Email sent and logged as a CRM activity.'));
  }
  else {
    backdrop_set_message(t('Email could not be sent. Please check your SendGrid configuration.'), 'error');
  }

  $form_state['redirect'] = 'drop/crm/contact/' . $contact->cid;
}

// ---------------------------------------------------------------------------
// Ticket email form
// ---------------------------------------------------------------------------

/**
 * Form: compose and send an email from a ticket to one or more contacts.
 */
function dropemail_ticket_form($form, &$form_state, $ticket) {
  $form['#ticket'] = $ticket;
  $form['#attached']['css'][] = backdrop_get_path('module', 'dropemail') . '/dropemail.css';

  $default = dropemail_get_default_from();
  $contact = dropemail_get_ticket_client_contact($ticket);

  $loaded  = !empty($ticket->pid) ? entity_load('drop_project', array($ticket->pid)) : array();
  $project = $loaded ? reset($loaded) : NULL;
  $label   = droptickets_ticket_label($ticket, $project);

  backdrop_set_title(t('Send email — @label: @title',
    array('@label' => $label, '@title' => $ticket->title)));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Tickets'), 'drop/tickets'),
    l($label . ': ' . $ticket->title, 'drop/tickets/' . $ticket->tid),
  ));

  // How many recipient rows to show (grows via AJAX).
  $count = isset($form_state['recipient_count']) ? (int) $form_state['recipient_count'] : 1;

  // Default value for the first recipient.
  // Prefer the linked CRM contact; fall back to the inbound sender email.
  $first_default = '';
  if ($contact) {
    $first_default = entity_label('crm_contact', $contact) . ' [cid:' . $contact->cid . ']';
  }
  elseif (!empty($ticket->inbound_email)) {
    $first_default = check_plain($ticket->inbound_email);
  }

  // Warning banner shown after a failed association check.
  if (!empty($form_state['storage']['unassociated_warning'])) {
    $names = $form_state['storage']['unassociated_warning'];
    $form['warning'] = array(
      '#markup' => '<div class="dropemail-warning messages warning">'
        . '<strong>' . t('Warning:') . '</strong> '
        . t('The following recipients are not associated with this ticket\'s project: %names. Check the box below to send anyway.',
            array('%names' => implode(', ', $names)))
        . '</div>',
      '#weight' => -10,
    );
    $form['confirm_unassociated'] = array(
      '#type'  => 'checkbox',
      '#title' => t('Send anyway to unassociated contacts'),
      '#weight' => -9,
    );
  }

  // Recipient list.
  // Use storage['recipient_defaults'] as source of truth after any add/remove.
  // Fall back to raw input (since #limit_validation_errors clears values).
  if (isset($form_state['storage']['recipient_defaults'])) {
    $defaults = $form_state['storage']['recipient_defaults'];
  }
  else {
    $raw = isset($form_state['input']['recipients']) ? $form_state['input']['recipients'] : array();
    $defaults = array();
    for ($j = 0; $j < $count; $j++) {
      $defaults[$j] = isset($raw[$j]) ? $raw[$j] : ($j === 0 ? $first_default : '');
    }
  }
  $count = max(1, count($defaults));

  $form['recipients'] = array(
    '#type'   => 'container',
    '#tree'   => TRUE,
    '#prefix' => '<div id="dropemail-recipient-area"><div id="dropemail-recipients-wrapper">',
    '#suffix' => '</div>',
  );

  foreach ($defaults as $i => $default_val) {
    // Resolve email for display to the right of the input.
    $email_suffix = '<span class="dropemail-recipient-hint">' . t('type to search') . '</span>';
    $cid = _dropemail_parse_cid($default_val);
    if ($cid) {
      $rc = crm_contact_load($cid);
      if ($rc) {
        $rc_email = dropemail_get_contact_primary_email($rc);
        $email_suffix = $rc_email
          ? '<span class="dropemail-recipient-email">' . check_plain($rc_email) . '</span>'
          : '<span class="dropemail-recipient-no-email">' . t('no email') . '</span>';
      }
    }

    $form['recipients'][$i] = array(
      '#type'              => 'textfield',
      '#title'             => $i === 0 ? t('To') : t('Also to'),
      '#autocomplete_path' => 'drop/crm/autocomplete/contacts',
      '#default_value'     => $default_val,
      '#field_suffix'      => $email_suffix,
      '#required'          => $i === 0,
    );
  }

  $form['recipient_actions'] = array(
    '#type'   => 'container',
    '#suffix' => '</div>',
    '#attributes' => array('class' => array('dropemail-recipient-actions')),
  );
  $form['recipient_actions']['add_recipient'] = array(
    '#type'                    => 'submit',
    '#value'                   => t('+ Add'),
    '#submit'                  => array('dropemail_ticket_form_add_recipient'),
    '#limit_validation_errors' => array(),
    '#ajax'                    => array(
      'callback' => 'dropemail_ticket_form_area_callback',
      'wrapper'  => 'dropemail-recipient-area',
    ),
  );
  if ($count > 1) {
    $form['recipient_actions']['remove_last'] = array(
      '#type'                    => 'submit',
      '#value'                   => t('− Remove last'),
      '#submit'                  => array('dropemail_ticket_form_remove_recipient'),
      '#limit_validation_errors' => array(),
      '#ajax'                    => array(
        'callback' => 'dropemail_ticket_form_area_callback',
        'wrapper'  => 'dropemail-recipient-area',
      ),
    );
  }


  $form['from_email'] = array(
    '#type'          => 'textfield',
    '#title'         => t('From'),
    '#default_value' => $default['address'],
    '#description'   => t('Use your own email address to have replies come back to you (support address becomes Reply-To).'),
    '#required'      => TRUE,
  );

  $form['subject'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Subject'),
    '#default_value' => t('Re: @title', array('@title' => $ticket->title)),
    '#required'      => TRUE,
    '#maxlength'     => 500,
  );

  $form['body'] = array(
    '#type'     => 'textarea',
    '#title'    => t('Message'),
    '#required' => TRUE,
    '#rows'     => 10,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Send email'),
  );
  $form['actions']['cancel'] = array(
    '#type'       => 'link',
    '#title'      => t('Cancel'),
    '#href'       => 'drop/tickets/' . $ticket->tid,
    '#attributes' => array('class' => array('button', 'button-secondary')),
  );

  return $form;
}

/**
 * AJAX submit: increment recipient count and rebuild.
 */
function dropemail_ticket_form_add_recipient($form, &$form_state) {
  $current = _dropemail_get_current_defaults($form_state);
  $current[] = '';
  $form_state['storage']['recipient_defaults'] = $current;
  $form_state['rebuild'] = TRUE;
}

/**
 * AJAX submit: remove a recipient row by index.
 */
function dropemail_ticket_form_remove_recipient($form, &$form_state) {
  $current = _dropemail_get_current_defaults($form_state);
  if (count($current) > 1) {
    array_pop($current);
  }
  $form_state['storage']['recipient_defaults'] = $current;
  $form_state['rebuild'] = TRUE;
}

/**
 * Reads the current recipient values from raw input (always current) or storage.
 *
 * $form_state['input'] is raw $_POST — always reflects what the user typed,
 * even when #limit_validation_errors empties $form_state['values'].
 * Storage is only used when there is no input at all (e.g. initial build).
 */
function _dropemail_get_current_defaults(&$form_state) {
  $raw = isset($form_state['input']['recipients']) ? $form_state['input']['recipients'] : array();
  // Filter to numeric keys only (ignore any stray form button entries).
  $values = array();
  foreach ($raw as $k => $v) {
    if (is_numeric($k)) {
      $values[(int) $k] = $v;
    }
  }
  ksort($values);
  $values = array_values($values);
  if (!empty($values)) {
    return $values;
  }
  // No input yet — fall back to storage or a single empty slot.
  return isset($form_state['storage']['recipient_defaults'])
    ? $form_state['storage']['recipient_defaults']
    : array('');
}

/**
 * AJAX callback: return recipients + actions as one unit.
 */
function dropemail_ticket_form_area_callback($form, &$form_state) {
  $output = backdrop_render($form['recipients']);
  $output .= backdrop_render($form['recipient_actions']);
  return array('#markup' => $output);
}

/**
 * Parses a contact autocomplete value and returns the cid, or FALSE.
 *
 * Expects "Name [cid:N]" format.
 */
function _dropemail_parse_cid($value) {
  if (preg_match('/\[cid:(\d+)\]/', $value, $m)) {
    return (int) $m[1];
  }
  return FALSE;
}

/**
 * Returns TRUE if a contact is associated with an org (direct match or
 * related via crm_relationship).
 */
function _dropemail_contact_associated($cid, $org_cid) {
  if ($cid === $org_cid) {
    return TRUE;
  }
  $related = db_select('crm_relationship', 'r')
    ->fields('r', array('rid'))
    ->condition(db_or()
      ->condition(db_and()
        ->condition('r.cid_a', $org_cid)
        ->condition('r.cid_b', $cid))
      ->condition(db_and()
        ->condition('r.cid_a', $cid)
        ->condition('r.cid_b', $org_cid)))
    ->range(0, 1)
    ->execute()
    ->fetchField();
  return (bool) $related;
}

/**
 * Validation for dropemail_ticket_form().
 */
function dropemail_ticket_form_validate($form, &$form_state) {
  $from = trim($form_state['values']['from_email']);
  if (!valid_email_address($from)) {
    form_set_error('from_email', t('Please enter a valid From email address.'));
  }

  // Resolve recipients — must be valid CRM contacts with email addresses.
  $ticket   = $form['#ticket'];
  $raw_list = isset($form_state['values']['recipients'])
    ? $form_state['values']['recipients'] : array();

  $resolved = array();
  foreach ($raw_list as $i => $value) {
    // Skip the remove_* submit button entries stored under recipients.
    if (!is_numeric($i)) {
      continue;
    }
    $value = trim($value);
    if ($value === '') {
      continue;
    }
    $cid = _dropemail_parse_cid($value);
    if (!$cid) {
      form_set_error('recipients][' . $i, t('"%val" is not a recognised contact. Please select from the autocomplete list.', array('%val' => $value)));
      continue;
    }
    $contact = crm_contact_load($cid);
    if (!$contact) {
      form_set_error('recipients][' . $i, t('Contact not found.'));
      continue;
    }
    $email = dropemail_get_contact_primary_email($contact);
    if (!$email) {
      form_set_error('recipients][' . $i, t('!name does not have a primary email address.',
        array('!name' => entity_label('crm_contact', $contact))));
      continue;
    }
    $resolved[] = array('cid' => $cid, 'email' => $email, 'name' => entity_label('crm_contact', $contact));
  }

  if (empty($resolved) && !form_get_errors()) {
    form_set_error('recipients][0', t('At least one recipient is required.'));
  }

  // Store resolved recipients for the submit handler.
  $form_state['storage']['resolved_recipients'] = $resolved;

  // Check association with the project org — only if no hard errors yet.
  if (!form_get_errors() && !empty($resolved)) {
    $org_cid = NULL;
    if (!empty($ticket->pid)) {
      $org_cid = (int) db_select('drop_project', 'p')
        ->fields('p', array('cid'))
        ->condition('p.pid', $ticket->pid)
        ->execute()
        ->fetchField();
    }

    if ($org_cid && empty($form_state['values']['confirm_unassociated'])) {
      $unassociated = array();
      foreach ($resolved as $r) {
        if (!_dropemail_contact_associated($r['cid'], $org_cid)) {
          $unassociated[] = $r['name'];
        }
      }
      if ($unassociated) {
        $form_state['storage']['unassociated_warning'] = $unassociated;
        // Preserve recipient count so the rebuilt form shows the same rows.
        $form_state['recipient_count'] = count($raw_list);
        $form_state['rebuild'] = TRUE;
      }
    }
  }
}

/**
 * Submit handler for dropemail_ticket_form().
 */
function dropemail_ticket_form_submit($form, &$form_state) {
  $ticket     = $form['#ticket'];
  $recipients = isset($form_state['storage']['resolved_recipients'])
    ? $form_state['storage']['resolved_recipients'] : array();
  $from_email = trim($form_state['values']['from_email']);
  $subject    = $form_state['values']['subject'];
  $body       = $form_state['values']['body'];

  // Append a [DROP-PID-NUM] threading tag to the subject so that inbound
  // replies can be automatically routed back to this ticket.
  $tagged_subject = $subject;
  if (!empty($ticket->pid) && !empty($ticket->ticket_number)) {
    $tag = '[DROP-' . $ticket->pid . '-' . $ticket->ticket_number . ']';
    // Only add if not already present.
    if (strpos($tagged_subject, '[DROP-') === FALSE) {
      $tagged_subject = $tagged_subject . ' ' . $tag;
    }
  }

  // Append a reply-separator footer so that inbound processing can strip it.
  $footer  = "\n\n-- [DROP REPLY SEPARATOR] --\n";
  $footer .= t('When replying, please do not change the subject line — it is used to route your reply to the correct ticket.');
  $body_with_footer = $body . $footer;

  $sent_count = 0;
  $eids       = array();
  foreach ($recipients as $r) {
    $eid = dropemail_send_and_log(array(
      'to'            => $r['email'],
      'from_email'    => $from_email,
      'subject'       => $tagged_subject,
      'body'          => $body_with_footer,
      'cid'           => $r['cid'],
      'tid'           => $ticket->tid,
      'skip_activity' => TRUE,
    ));
    if ($eid) {
      $sent_count++;
      $eids[] = $eid;
    }
  }

  // Create one shared activity listing all recipients.
  if ($sent_count && !empty($recipients)) {
    global $user;
    $lang     = LANGUAGE_NONE;
    $activity = entity_create('crm_activity', array(
      'type'          => 'email',
      'subject'       => $tagged_subject,
      'status'        => 'completed',
      'direction'     => 'outbound',
      'activity_date' => REQUEST_TIME,
      'created'       => REQUEST_TIME,
      'uid'           => $user->uid,
    ));
    foreach ($recipients as $r) {
      $activity->field_crm_contacts[$lang][] = array('target_id' => $r['cid']);
    }
    $activity->field_crm_meeting_notes[$lang][] = array(
      'value'  => $body,
      'format' => 'plain_text',
    );
    try {
      $activity->save();
      // Link all log rows to this shared activity.
      if ($eids) {
        db_update('drop_email_log')
          ->fields(array('aid' => $activity->aid))
          ->condition('eid', $eids, 'IN')
          ->execute();
      }
    }
    catch (Exception $e) {
      watchdog('dropemail', 'Failed to create shared activity for ticket email: %msg',
        array('%msg' => $e->getMessage()), WATCHDOG_WARNING);
    }
  }

  // Create a ticket comment so the sent email appears in the conversation thread.
  if ($sent_count > 0) {
    $to_names = array();
    foreach ($recipients as $r) {
      $to_names[] = $r['name'] . ' <' . $r['email'] . '>';
    }
    $comment_body = '<p><strong>' . t('To:') . '</strong> ' . check_plain(implode(', ', $to_names)) . '<br>'
      . '<strong>' . t('Subject:') . '</strong> ' . check_plain($subject) . '</p>'
      . '<p>' . nl2br(check_plain($body)) . '</p>';

    $comment = new DropTicketComment();
    $comment->tid         = $ticket->tid;
    $comment->uid         = $user->uid;
    $comment->body        = $comment_body;
    $comment->body_format = 'filtered_html';
    $comment->source      = 'email_out';
    $comment->save();
  }

  if ($sent_count === count($recipients)) {
    backdrop_set_message(format_plural($sent_count,
      'Email sent to 1 recipient and logged.',
      'Emails sent to @count recipients and logged.'));
  }
  elseif ($sent_count > 0) {
    backdrop_set_message(t('Sent to @sent of @total recipients. Check watchdog for delivery errors.',
      array('@sent' => $sent_count, '@total' => count($recipients))), 'warning');
  }
  else {
    backdrop_set_message(t('Email could not be sent. Please check your mail configuration.'), 'error');
  }

  $form_state['redirect'] = 'drop/tickets/' . $ticket->tid;
}
