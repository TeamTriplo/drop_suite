<?php
/**
 * @file
 * Sample-content generator for Drop CRM Membership.
 *
 * Provides a confirmation form at drop/crm/memberships/sample-content that
 * seeds a varied set of memberships, terms, and payments covering the full
 * state-machine surface (active / expiring soon / grace / lapsed / cancelled
 * / lifetime) so admins can demo the module and QA can verify the
 * crm_memberships_expiring view and related Views without hand-building
 * fixture data.
 *
 * This is purposefully separate from drop_start's broader sample content
 * generator so the dependency lives entirely inside drop_crm_membership and
 * sites that don't enable drop_start can still seed membership demo data.
 */

/**
 * Form callback: confirmation form for the membership sample content.
 */
function drop_crm_membership_demo_form($form, &$form_state) {
  $form['#tree'] = FALSE;

  $contact_count = (int) db_query('SELECT COUNT(*) FROM {crm_contact}')->fetchField();
  $member_count  = (int) db_query('SELECT COUNT(*) FROM {crm_membership}')->fetchField();

  $form['intro'] = array(
    '#markup' => '<p>' . t('This will create a varied set of sample memberships across the full state-machine surface — active, expiring soon, in grace, lapsed, cancelled, and (when configured) a lifetime membership with multi-term renewal history. Payments are attached using the configured payment methods so the payment list also has demo content.') . '</p>'
      . '<p>' . t('Use this for QA, demos, or fresh installs where you want the expiring-soon view and reports to have something to render. Existing memberships are not modified.') . '</p>',
  );

  $items = array();
  $items[] = t('Existing CRM contacts: @n', array('@n' => $contact_count));
  $items[] = t('Existing memberships: @n', array('@n' => $member_count));
  if ($contact_count < 8) {
    $items[] = t('Fewer than 8 contacts found — the generator will create lightweight sample contacts to attach memberships to.');
  }
  $form['stats'] = array(
    '#theme' => 'item_list',
    '#items' => $items,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Generate sample memberships'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/crm/memberships'),
  );

  return $form;
}

/**
 * Submit handler: runs the generator and reports a summary message.
 */
function drop_crm_membership_demo_form_submit($form, &$form_state) {
  $summary = _drop_crm_membership_demo_generate();

  $parts = array();
  if (!empty($summary['contacts_created'])) {
    $parts[] = format_plural($summary['contacts_created'], 'Created 1 sample contact', 'Created @count sample contacts');
  }
  if (!empty($summary['active'])) {
    $parts[] = format_plural($summary['active'], 'Created 1 active membership', 'Created @count active memberships');
  }
  if (!empty($summary['expiring'])) {
    $parts[] = format_plural($summary['expiring'], 'Created 1 expiring-soon membership', 'Created @count expiring-soon memberships');
  }
  if (!empty($summary['grace'])) {
    $parts[] = format_plural($summary['grace'], 'Created 1 grace-period membership', 'Created @count grace-period memberships');
  }
  if (!empty($summary['lapsed'])) {
    $parts[] = format_plural($summary['lapsed'], 'Created 1 lapsed membership', 'Created @count lapsed memberships');
  }
  if (!empty($summary['cancelled'])) {
    $parts[] = format_plural($summary['cancelled'], 'Created 1 cancelled membership', 'Created @count cancelled memberships');
  }
  if (!empty($summary['lifetime'])) {
    $parts[] = format_plural($summary['lifetime'], 'Created 1 lifetime membership', 'Created @count lifetime memberships');
  }
  if (!empty($summary['terms'])) {
    $parts[] = format_plural($summary['terms'], '1 membership term', '@count membership terms');
  }
  if (!empty($summary['payments'])) {
    $parts[] = format_plural($summary['payments'], '1 payment', '@count payments');
  }

  if (empty($parts)) {
    backdrop_set_message(t('Nothing was created — see watchdog for details.'), 'warning');
  }
  else {
    backdrop_set_message(t('Sample membership content: !list.', array('!list' => implode(', ', $parts))));
  }
  if (!empty($summary['errors'])) {
    foreach ($summary['errors'] as $err) {
      backdrop_set_message($err, 'warning');
    }
  }

  $form_state['redirect'] = 'drop/crm/memberships';
}

/**
 * Core generator. Creates sample memberships, terms, and payments and
 * returns a summary array for the UI message.
 *
 * Idempotent in the sense that it never updates existing rows — every call
 * inserts fresh data. Calling it multiple times is safe but produces extra
 * memberships each time.
 *
 * @return array
 *   Keyed counts: contacts_created, active, expiring, grace, lapsed,
 *   cancelled, lifetime, terms, payments. Plus 'errors' (array of strings).
 */
function _drop_crm_membership_demo_generate() {
  $summary = array(
    'contacts_created' => 0,
    'active'           => 0,
    'expiring'         => 0,
    'grace'            => 0,
    'lapsed'           => 0,
    'cancelled'        => 0,
    'lifetime'         => 0,
    'terms'            => 0,
    'payments'         => 0,
    'errors'           => array(),
  );

  $types = _drop_crm_membership_types();
  if (empty($types)) {
    $summary['errors'][] = t('No membership types are configured. Visit the CRM settings page and add at least one type, then try again.');
    return $summary;
  }

  // Pick reasonable type keys. Fall back to whatever the site has if the
  // canonical demo keys aren't present.
  $type_keys = array_keys($types);
  $individual_type = isset($types['individual']) ? 'individual' : reset($type_keys);
  $general_type    = isset($types['general'])    ? 'general'    : $individual_type;
  $student_type    = isset($types['student'])    ? 'student'    : $individual_type;
  $lifetime_type   = isset($types['lifetime'])   ? 'lifetime'   : NULL;

  // Available payment methods (preserve order).
  $methods = _drop_crm_membership_payment_method_options();
  $methods = array_values(array_keys($methods));
  if (empty($methods)) {
    $methods = array('Check', 'Cash', 'Credit Card', 'Other');
  }

  // Get or create at least 8 contacts to spread memberships across.
  $contact_ids = _drop_crm_membership_demo_get_contacts(8, $summary);
  if (count($contact_ids) < 1) {
    $summary['errors'][] = t('Could not find or create any CRM contacts to attach memberships to.');
    return $summary;
  }

  $today_ts = strtotime('today');

  // Scenario list. Each entry:
  //   [contact_index, type_key, end_offset_days, status_override, summary_key,
  //    note, payment_method_index]
  // end_offset_days is days from today for the term's end_date. start_date is
  // 365 days before end_date by default. Negative = past end date.
  // payment_method_index NULL means no payment recorded.
  $scenarios = array(
    // Active mid-term + payment.
    array(0, $individual_type, 200,  NULL,                                'active',    'active mid-term',        0),
    array(1, $general_type,    280,  NULL,                                'active',    'active general',         1),
    array(2, $student_type,    150,  NULL,                                'active',    'active student',         2),
    // Expiring soon (1-30 days). The crm_memberships_expiring View filters
    // status IN (active, grace) AND current_end_date BETWEEN today AND +30d.
    array(3, $individual_type, 5,    NULL,                                'expiring',  'expiring in 5 days',     0),
    array(4, $individual_type, 15,   NULL,                                'expiring',  'expiring in 15 days',    3),
    array(5, $general_type,    25,   NULL,                                'expiring',  'expiring in 25 days',    1),
    // Grace period — end_date past but within grace_days.
    array(6, $individual_type, -10,  DROPCRM_MEMBERSHIP_STATUS_GRACE,     'grace',     'recently expired, grace', 2),
    // Lapsed — past grace.
    array(7, $individual_type, -120, DROPCRM_MEMBERSHIP_STATUS_LAPSED,    'lapsed',    'lapsed (past grace)',    NULL),
    // Cancelled.
    array(0, $individual_type, 100,  DROPCRM_MEMBERSHIP_STATUS_CANCELLED, 'cancelled', 'cancelled membership',   0),
  );

  foreach ($scenarios as $s) {
    list($i, $type_key, $end_offset, $status_override, $summary_key, $note, $method_idx) = $s;
    if (!isset($contact_ids[$i])) {
      // Wrap around if we have fewer contacts than scenarios reference.
      $i = $i % max(count($contact_ids), 1);
      if (!isset($contact_ids[$i])) {
        continue;
      }
    }
    $cid      = $contact_ids[$i];
    $end_ts   = strtotime($end_offset . ' days', $today_ts);
    $start_ts = strtotime(($end_offset - 365) . ' days', $today_ts);
    $status   = isset($status_override) ? $status_override : DROPCRM_MEMBERSHIP_STATUS_ACTIVE;

    $mid = _drop_crm_membership_demo_create_membership($cid, $type_key, $status);
    if (!$mid) {
      continue;
    }
    $summary[$summary_key]++;

    $tid = _drop_crm_membership_demo_create_term($mid, $start_ts, $end_ts, DROPCRM_MEMBERSHIP_STATUS_ACTIVE, $note);
    if ($tid) {
      $summary['terms']++;
    }

    if ($method_idx !== NULL && isset($methods[$method_idx])) {
      $dues = isset($types[$type_key]['default_dues']) ? (float) $types[$type_key]['default_dues'] : 50.00;
      if ($dues <= 0) {
        $dues = 50.00;
      }
      $pid = _drop_crm_membership_demo_create_payment($mid, $tid, $dues, $start_ts, $methods[$method_idx], 'sample');
      if ($pid) {
        $summary['payments']++;
      }
    }
  }

  // Lifetime membership with two-term renewal history and two payments
  // using two different methods. Demos the term-history + payment-history
  // panels on the membership view.
  if ($lifetime_type) {
    $cid_lifetime = isset($contact_ids[0]) ? $contact_ids[0] : reset($contact_ids);
    $mid = _drop_crm_membership_demo_create_membership($cid_lifetime, $lifetime_type, DROPCRM_MEMBERSHIP_STATUS_ACTIVE);
    if ($mid) {
      $summary['lifetime']++;

      // Lifetime terms: end_date NULL means "no expiration".
      $t1_start = strtotime('-700 days', $today_ts);
      $t1_end   = strtotime('-336 days', $today_ts);
      $t2_start = strtotime('-335 days', $today_ts);
      // Latest term has NULL end_date (the lifetime sentinel) so cron
      // leaves the membership in active state forever.
      $t2_end   = NULL;

      $term_specs = array(
        array($t1_start, $t1_end, 'initial term', 0),
        array($t2_start, $t2_end, 'lifetime renewal', isset($methods[2]) ? 2 : 0),
      );
      foreach ($term_specs as $idx => $ts) {
        $tid = _drop_crm_membership_demo_create_term($mid, $ts[0], $ts[1], DROPCRM_MEMBERSHIP_STATUS_ACTIVE, $ts[2]);
        if ($tid) {
          $summary['terms']++;
          $dues = isset($types[$lifetime_type]['default_dues']) ? (float) $types[$lifetime_type]['default_dues'] : 250.00;
          if ($dues <= 0) {
            $dues = 250.00;
          }
          $pid = _drop_crm_membership_demo_create_payment($mid, $tid, $dues, $ts[0], $methods[$ts[3]], 'sample lifetime');
          if ($pid) {
            $summary['payments']++;
          }
        }
      }
    }
  }
  else {
    // No lifetime type configured — give one of the active memberships an
    // extra renewal term anyway so renewal history is demo-able.
    $renewal_cid = isset($contact_ids[0]) ? $contact_ids[0] : reset($contact_ids);
    $mid = _drop_crm_membership_demo_create_membership($renewal_cid, $individual_type, DROPCRM_MEMBERSHIP_STATUS_ACTIVE);
    if ($mid) {
      $summary['active']++;
      // Two terms, both already ended — but the latest extends ~6 months out.
      $t1_start = strtotime('-700 days', $today_ts);
      $t1_end   = strtotime('-336 days', $today_ts);
      $t2_start = strtotime('-335 days', $today_ts);
      $t2_end   = strtotime('+180 days', $today_ts);
      foreach (array(
        array($t1_start, $t1_end, 'initial term'),
        array($t2_start, $t2_end, 'first renewal'),
      ) as $ts) {
        $tid = _drop_crm_membership_demo_create_term($mid, $ts[0], $ts[1], DROPCRM_MEMBERSHIP_STATUS_ACTIVE, $ts[2]);
        if ($tid) {
          $summary['terms']++;
          $dues = isset($types[$individual_type]['default_dues']) ? (float) $types[$individual_type]['default_dues'] : 50.00;
          if ($dues <= 0) {
            $dues = 50.00;
          }
          $method_idx = ($ts[2] === 'initial term') ? 0 : (isset($methods[2]) ? 2 : 0);
          $pid = _drop_crm_membership_demo_create_payment($mid, $tid, $dues, $ts[0], $methods[$method_idx], 'sample renewal');
          if ($pid) {
            $summary['payments']++;
          }
        }
      }
    }
  }

  return $summary;
}

/**
 * Returns up to $needed contact IDs, creating lightweight sample contacts
 * if the existing pool is too small.
 *
 * Prefers existing crm_person rows. If fewer than $needed exist, creates
 * additional persons with placeholder names. Drop Start, when present,
 * already creates richer fixture contacts; this fallback is here so the
 * generator works even on bare installs.
 */
function _drop_crm_membership_demo_get_contacts($needed, array &$summary) {
  $existing = db_query_range("
    SELECT cid FROM {crm_contact}
    WHERE type = :t AND status = 1
    ORDER BY cid ASC
  ", 0, $needed, array(':t' => 'crm_person'))->fetchCol();

  if (count($existing) >= $needed) {
    return $existing;
  }

  // Fall back: include any contact, person or otherwise.
  if (count($existing) < $needed) {
    $rest = db_query_range("
      SELECT cid FROM {crm_contact}
      WHERE status = 1
      ORDER BY cid ASC
    ", 0, $needed)->fetchCol();
    foreach ($rest as $cid) {
      if (!in_array($cid, $existing, TRUE) && count($existing) < $needed) {
        $existing[] = $cid;
      }
    }
  }

  // Create lightweight crm_person rows to fill the gap.
  if (count($existing) < $needed) {
    $bundles = field_info_bundles('crm_contact');
    $bundle = isset($bundles['crm_person']) ? 'crm_person' : NULL;
    if (!$bundle) {
      // No person bundle — caller will fail gracefully.
      return $existing;
    }
    $first_names = array('Avery', 'Blair', 'Casey', 'Devon', 'Ellis', 'Frankie', 'Gale', 'Harper', 'Indigo', 'Jordan');
    $last_names  = array('Sample', 'Demo', 'Test', 'Draft', 'Sketch', 'Holder', 'Dummy', 'Stand-In', 'Filler', 'Proxy');
    $deficit = $needed - count($existing);
    $has_first = field_info_field('field_crm_first_name') !== NULL;
    $has_last  = field_info_field('field_crm_last_name') !== NULL;
    for ($i = 0; $i < $deficit; $i++) {
      $first = $first_names[$i % count($first_names)];
      $last  = $last_names[$i % count($last_names)];
      $contact = entity_create('crm_contact', array(
        'type'   => $bundle,
        'name'   => $first . ' ' . $last,
        'status' => 1,
      ));
      if ($has_first) {
        $contact->field_crm_first_name[LANGUAGE_NONE][0]['value'] = $first;
      }
      if ($has_last) {
        $contact->field_crm_last_name[LANGUAGE_NONE][0]['value'] = $last;
      }
      try {
        $contact->save();
        if (!empty($contact->cid)) {
          $existing[] = $contact->cid;
          $summary['contacts_created']++;
        }
      }
      catch (Exception $e) {
        watchdog('drop_crm_membership', 'Demo contact creation failed: @m', array('@m' => $e->getMessage()), WATCHDOG_WARNING);
      }
    }
  }

  return array_values($existing);
}

/**
 * Create a crm_membership row and return its mid. Auto-assigns a member_id.
 */
function _drop_crm_membership_demo_create_membership($cid, $type_key, $status) {
  try {
    $m = entity_create('crm_membership', array(
      'cid'    => (int) $cid,
      'type'   => $type_key,
      'status' => (int) $status,
    ));
    $m->save();
    if (empty($m->member_id) && function_exists('_drop_crm_membership_generate_member_id')) {
      $m->member_id = _drop_crm_membership_generate_member_id($m->mid);
      $m->save();
    }
    return (int) $m->mid;
  }
  catch (Exception $e) {
    watchdog('drop_crm_membership', 'Demo membership creation failed: @m', array('@m' => $e->getMessage()), WATCHDOG_WARNING);
    return 0;
  }
}

/**
 * Create a crm_membership_term row and return its tid.
 *
 * Side effect: hook_crm_membership_term_insert() will recompute the
 * membership's denormalized current_end_date column.
 */
function _drop_crm_membership_demo_create_term($mid, $start_ts, $end_ts, $status, $notes) {
  try {
    $term = entity_create('crm_membership_term', array(
      'mid'        => (int) $mid,
      'start_date' => (int) $start_ts,
      'end_date'   => $end_ts === NULL ? NULL : (int) $end_ts,
      'status'     => (int) $status,
      'notes'      => $notes,
    ));
    $term->save();
    return (int) $term->tid;
  }
  catch (Exception $e) {
    watchdog('drop_crm_membership', 'Demo term creation failed: @m', array('@m' => $e->getMessage()), WATCHDOG_WARNING);
    return 0;
  }
}

/**
 * Create a crm_membership_payment row and return its pid.
 *
 * Side effect: hook_crm_membership_payment_insert() will recompute the
 * membership's denormalized last_payment_date and total_paid.
 */
function _drop_crm_membership_demo_create_payment($mid, $tid, $amount, $paid_ts, $method, $reference) {
  try {
    $p = entity_create('crm_membership_payment', array(
      'mid'       => (int) $mid,
      'tid'       => $tid ? (int) $tid : NULL,
      'amount'    => (float) $amount,
      'paid_date' => (int) $paid_ts,
      'method'    => (string) $method,
      'reference' => (string) $reference,
    ));
    $p->save();
    return (int) $p->pid;
  }
  catch (Exception $e) {
    watchdog('drop_crm_membership', 'Demo payment creation failed: @m', array('@m' => $e->getMessage()), WATCHDOG_WARNING);
    return 0;
  }
}
