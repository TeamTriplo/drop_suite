<?php
/**
 * @file
 * Inbound email handling for DropBridge.
 *
 * Receives SendGrid Inbound Parse webhook POSTs, creates or updates tickets,
 * and optionally forwards a copy to a configured support address.
 */

// ---------------------------------------------------------------------------
// Webhook endpoint
// ---------------------------------------------------------------------------

/**
 * Page callback: receive a SendGrid Inbound Parse POST.
 *
 * Access is open (no Backdrop auth); requests are validated by a shared-secret
 * token passed as a query parameter: dropbridge/inbound-email?token=XXX
 *
 * Always returns HTTP 200 so SendGrid does not retry the delivery.
 */
function dropbridge_inbound_email_webhook() {
  // Must be POST.
  if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    backdrop_add_http_header('Status', '405 Method Not Allowed');
    backdrop_exit();
  }

  $config = config('dropbridge.inbound');

  // If inbound processing is disabled, acknowledge and exit.
  if (!$config->get('enabled')) {
    backdrop_add_http_header('Status', '200 OK');
    print 'Inbound email disabled.';
    backdrop_exit();
  }

  // Validate the shared-secret token.
  $expected_token = (string) $config->get('webhook_token');
  $given_token    = isset($_GET['token']) ? (string) $_GET['token'] : '';
  if (!empty($expected_token) && !hash_equals($expected_token, $given_token)) {
    watchdog('dropbridge', 'Inbound email: invalid or missing token from @ip.',
      array('@ip' => ip_address()), WATCHDOG_WARNING);
    backdrop_add_http_header('Status', '200 OK');
    print 'Unauthorized.';
    backdrop_exit();
  }

  // Parse the SendGrid POST payload.
  $email = _dropbridge_parse_inbound_post($_POST);
  if (!$email) {
    watchdog('dropbridge', 'Inbound email: could not parse POST data.', array(), WATCHDOG_WARNING);
    backdrop_add_http_header('Status', '200 OK');
    print 'Parse error.';
    backdrop_exit();
  }

  $post_keys = implode(', ', array_keys($_POST));
  watchdog('dropbridge', 'Inbound email received: from=@from subject=@subject body_len=@len text_field=@text html_field=@html post_keys=[@keys].',
    array(
      '@from'    => $email['from_email'],
      '@subject' => $email['subject'],
      '@len'     => strlen($email['body']),
      '@text'    => isset($_POST['text']) ? strlen($_POST['text']) . ' chars' : 'absent',
      '@html'    => isset($_POST['html']) ? strlen($_POST['html']) . ' chars' : 'absent',
      '@keys'    => $post_keys,
    ), WATCHDOG_INFO);

  // Check allow-list.
  $allowlist       = $config->get('allowlist') ?: array();
  $drop_unverified = (bool) $config->get('drop_unverified');
  $default_pid     = (int) $config->get('default_pid');
  watchdog('dropbridge', 'Inbound config: default_pid=@pid allowlist_count=@count drop_unverified=@drop.',
    array('@pid' => $default_pid, '@count' => count($allowlist), '@drop' => $drop_unverified ? 'yes' : 'no'), WATCHDOG_INFO);

  // An empty allowlist means "accept all"; a populated list restricts senders.
  $is_verified = empty($allowlist)
    || in_array(strtolower($email['from_email']), array_map('strtolower', $allowlist));

  if (!$is_verified && $drop_unverified) {
    watchdog('dropbridge', 'Inbound email dropped — sender not on allowlist: @from.',
      array('@from' => $email['from_email']), WATCHDOG_INFO);
    backdrop_add_http_header('Status', '200 OK');
    print 'Dropped.';
    backdrop_exit();
  }

  // Check for reply threading: [DROP-{pid}-{ticket_number}] in subject.
  $ticket = _dropbridge_find_ticket_by_subject($email['subject']);

  if ($ticket) {
    // Add a comment to the existing ticket.
    _dropbridge_create_comment_from_email($ticket, $email);
    watchdog('dropbridge', 'Inbound email threaded to ticket tid=@tid (#@num).',
      array('@tid' => $ticket->tid, '@num' => $ticket->ticket_number), WATCHDOG_INFO);
  }
  else {
    // Create a brand-new ticket.
    $ticket = _dropbridge_create_ticket_from_email($email, $config, $is_verified);
    watchdog('dropbridge', 'Inbound email created new ticket tid=@tid.',
      array('@tid' => $ticket->tid), WATCHDOG_INFO);
  }

  // Forward a copy to the configured support address.
  $forward_to = (string) $config->get('forward_to');
  if (!empty($forward_to)) {
    _dropbridge_forward_email($email, $ticket, $forward_to);
  }

  backdrop_add_http_header('Status', '200 OK');
  print 'OK';
  backdrop_exit();
}

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------

/**
 * Parses raw SendGrid POST fields into a normalised email array.
 *
 * SendGrid Inbound Parse sends the following relevant fields:
 *   from, to, subject, text (plain), html.
 *
 * @param array $post
 *   The $_POST superglobal from the webhook request.
 *
 * @return array|FALSE
 *   Associative array with keys:
 *     - from_email (string)
 *     - from_name  (string, may be empty)
 *     - to_email   (string)
 *     - subject    (string)
 *     - body       (string, plain text, reply-chain stripped)
 *   Returns FALSE if the required "from" field is absent.
 */
function _dropbridge_parse_inbound_post($post) {
  $raw_from = isset($post['from']) ? trim($post['from']) : '';

  if (empty($raw_from)) {
    return FALSE;
  }

  // Parse "Display Name <email>" or a bare email address.
  if (preg_match('/"?([^"<]*)"?\s*<([^>]+)>/', $raw_from, $m)) {
    $from_name  = trim($m[1], ' "');
    $from_email = strtolower(trim($m[2]));
  }
  else {
    $from_name  = '';
    $from_email = strtolower($raw_from);
  }

  // Parse the To address.
  $to_raw   = isset($post['to']) ? trim($post['to']) : '';
  $to_email = '';
  if ($to_raw) {
    if (preg_match('/<([^>]+)>/', $to_raw, $m)) {
      $to_email = strtolower(trim($m[1]));
    }
    else {
      $to_email = strtolower($to_raw);
    }
  }

  $subject = isset($post['subject']) ? trim($post['subject']) : '(No subject)';
  if (empty($subject)) {
    $subject = '(No subject)';
  }

  // Prefer plain text; fall back to HTML stripped of tags and decoded.
  if (!empty($post['text'])) {
    $body = $post['text'];
  }
  elseif (!empty($post['html'])) {
    $body = html_entity_decode(strip_tags($post['html']), ENT_QUOTES, 'UTF-8');
  }
  else {
    $body = '';
  }

  // Remove the reply thread signature footer we insert on outbound emails.
  $body = _dropbridge_strip_reply_footer($body);

  // Strip quoted reply content.
  $body = _dropbridge_strip_quoted_reply($body);

  return array(
    'from_email' => $from_email,
    'from_name'  => $from_name,
    'to_email'   => $to_email,
    'subject'    => $subject,
    'body'       => trim($body),
  );
}

/**
 * Removes the outbound-email footer that dropemail appends to ticket emails.
 *
 * The footer starts with the sentinel line
 * "-- [DROP REPLY SEPARATOR] --"
 * Everything from that line onward is stripped.
 *
 * @param string $text
 *   Raw plain-text body.
 *
 * @return string
 */
function _dropbridge_strip_reply_footer($text) {
  $sep = '-- [DROP REPLY SEPARATOR] --';
  $pos = strpos($text, $sep);
  if ($pos !== FALSE) {
    $text = substr($text, 0, $pos);
  }
  return $text;
}

/**
 * Strips common reply-chain quoting patterns from email plain text.
 *
 * Removes everything from the first occurrence of:
 *  - "On [date] ... wrote:" (Gmail / Apple Mail style)
 *  - Lines starting with ">" (quoted text)
 *  - "-----Original Message-----" (Outlook style)
 *
 * @param string $text
 *   Plain-text body.
 *
 * @return string
 *   Text with quoted sections removed.
 */
function _dropbridge_strip_quoted_reply($text) {
  $lines  = preg_split('/\r?\n/', $text);
  $result = array();

  foreach ($lines as $line) {
    $trimmed = trim($line);

    // "On <date>, <name> wrote:" — Gmail / Apple Mail attribution line.
    if (preg_match('/^On .{10,} wrote:\s*$/i', $trimmed)) {
      break;
    }

    // Outlook "-----Original Message-----".
    if (preg_match('/^-{3,}\s*Original Message\s*-{3,}/i', $trimmed)) {
      break;
    }

    // Quoted lines starting with ">".
    if (strpos($trimmed, '>') === 0) {
      break;
    }

    $result[] = $line;
  }

  return implode("\n", $result);
}

// ---------------------------------------------------------------------------
// Threading
// ---------------------------------------------------------------------------

/**
 * Finds an existing ticket whose [DROP-PID-NUM] tag matches the subject.
 *
 * @param string $subject
 *   The inbound email subject line.
 *
 * @return object|NULL
 *   DropTicket entity if found, NULL otherwise.
 */
function _dropbridge_find_ticket_by_subject($subject) {
  if (!preg_match('/\[DROP-(\d+)-(\d+)\]/i', $subject, $m)) {
    return NULL;
  }

  $pid  = (int) $m[1];
  $tnum = (int) $m[2];

  $tid = db_select('drop_ticket', 't')
    ->fields('t', array('tid'))
    ->condition('t.pid', $pid)
    ->condition('t.ticket_number', $tnum)
    ->range(0, 1)
    ->execute()
    ->fetchField();

  return $tid ? drop_ticket_load($tid) : NULL;
}

// ---------------------------------------------------------------------------
// Ticket / comment creation
// ---------------------------------------------------------------------------

/**
 * Creates a new DropTicket from an inbound email.
 *
 * @param array $email
 *   Parsed email (from _dropbridge_parse_inbound_post()).
 * @param Config $config
 *   The dropbridge.inbound config object.
 * @param bool $is_verified
 *   Whether the sender is on the allowlist.
 *
 * @return object
 *   The newly saved DropTicket entity.
 */
function _dropbridge_create_ticket_from_email($email, $config, $is_verified) {
  $default_pid = (int) $config->get('default_pid');

  // Strip any stray [DROP-X-X] tag from the title (defensive).
  $title = trim(preg_replace('/\s*\[DROP-\d+-\d+\]\s*/i', '', $email['subject']));
  if (empty($title)) {
    $title = '(No subject)';
  }

  $note = t('This ticket was submitted by email from @email.', array('@email' => $email['from_email']));
  $divider = '- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -';
  // Strip [image: filename] placeholders inserted by email clients for inline images.
  $clean_body = preg_replace('/\[image:[^\]]*\]\s*/i', '', $email['body']);
  $body = $note . "\n" . $divider . "\n\n" . trim($clean_body);

  $ticket = entity_create('drop_ticket', array(
    'title'         => $title,
    'body'          => $body,
    'body_format'   => 'plain_text',
    'type'          => 'task',
    'status'        => 'open',
    'priority'      => 1,
    'pid'           => $default_pid,
    'inbound_email' => $email['from_email'],
    'is_verified'   => $is_verified ? 1 : 0,
  ));

  $ticket->save();

  // Attach any files/images from the email.
  _dropbridge_attach_email_files($ticket);

  return $ticket;
}

/**
 * Saves SendGrid email attachments as managed files on a ticket.
 *
 * SendGrid sends attachments in $_FILES as attachment1, attachment2, etc.
 * Metadata (original filenames, MIME types) is in $_POST['attachment-info']
 * as a JSON object keyed by the same names.
 *
 * @param object $ticket
 *   The DropTicket entity to attach files to.
 */
function _dropbridge_attach_email_files($ticket) {
  if (empty($_FILES)) {
    return;
  }

  // Parse attachment metadata from SendGrid.
  $attachment_info = array();
  if (!empty($_POST['attachment-info'])) {
    $decoded = json_decode($_POST['attachment-info'], TRUE);
    if (is_array($decoded)) {
      $attachment_info = $decoded;
    }
  }

  $lang     = LANGUAGE_NONE;
  $existing = isset($ticket->field_ticket_files[$lang]) ? $ticket->field_ticket_files[$lang] : array();
  $added    = FALSE;

  foreach ($_FILES as $key => $file_data) {
    // Only process sendgrid attachment fields (attachment1, attachment2, …).
    if (strpos($key, 'attachment') !== 0) {
      continue;
    }
    if (empty($file_data['tmp_name']) || !is_uploaded_file($file_data['tmp_name'])) {
      continue;
    }

    // Determine original filename from SendGrid metadata, falling back to $_FILES name.
    $filename = $file_data['name'];
    if (!empty($attachment_info[$key]['filename'])) {
      $filename = $attachment_info[$key]['filename'];
    }
    $filename = file_munge_filename(basename($filename), 'jpg jpeg png gif pdf doc docx xls xlsx zip txt');

    $dest = 'public://ticket-attachments/' . $ticket->tid . '/';
    file_prepare_directory($dest, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS);

    $file_contents = file_get_contents($file_data['tmp_name']);
    if ($file_contents === FALSE) {
      continue;
    }

    $file = file_save_data($file_contents, $dest . $filename, FILE_EXISTS_RENAME);
    if (!$file) {
      watchdog('dropbridge', 'Failed to save attachment @name for ticket @tid.', array('@name' => $filename, '@tid' => $ticket->tid), WATCHDOG_WARNING);
      continue;
    }

    $file->status = FILE_STATUS_PERMANENT;
    file_save($file);
    file_usage_add($file, 'droptickets', 'drop_ticket', $ticket->tid);

    $existing[] = array('fid' => $file->fid, 'display' => 1, 'description' => '');
    $added = TRUE;
  }

  if ($added) {
    $ticket->field_ticket_files[$lang] = $existing;
    $ticket->save();
  }
}

/**
 * Adds a comment to an existing ticket from a reply email.
 *
 * @param object $ticket
 *   The DropTicket entity to comment on.
 * @param array $email
 *   Parsed email array.
 */
function _dropbridge_create_comment_from_email($ticket, $email) {
  if (!empty($email['from_name'])) {
    $attribution = $email['from_name'] . ' <' . $email['from_email'] . '> wrote:';
  }
  else {
    $attribution = $email['from_email'] . ' wrote:';
  }

  $full_body = $attribution . "\n\n" . $email['body'];

  $comment = entity_create('drop_ticket_comment', array(
    'tid'         => $ticket->tid,
    'body'        => $full_body,
    'body_format' => 'plain_text',
    'uid'         => 0,
    'source'      => 'email_in',
  ));
  $comment->save();
}

// ---------------------------------------------------------------------------
// Forwarding
// ---------------------------------------------------------------------------

/**
 * Forwards a copy of the inbound email to the configured address.
 *
 * Includes a link to the created / updated ticket so the recipient can
 * click through to the admin interface.
 *
 * @param array $email
 *   Parsed email array.
 * @param object $ticket
 *   The DropTicket that was created or updated.
 * @param string $forward_to
 *   Destination email address.
 */
function _dropbridge_forward_email($email, $ticket, $forward_to) {
  $ticket_url = url('drop/tickets/' . $ticket->tid, array('absolute' => TRUE));

  $params = array(
    'original_from' => $email['from_email'],
    'original_name' => $email['from_name'],
    'original_to'   => $email['to_email'],
    'subject'       => $email['subject'],
    'body'          => $email['body'],
    'ticket_url'    => $ticket_url,
    'ticket'        => $ticket,
  );

  $result = backdrop_mail(
    'dropbridge',
    'inbound_forward',
    $forward_to,
    language_default(),
    $params
  );

  if (empty($result['result'])) {
    watchdog('dropbridge', 'Failed to forward inbound email to @addr (ticket tid=@tid).',
      array('@addr' => $forward_to, '@tid' => $ticket->tid), WATCHDOG_WARNING);
  }
}
