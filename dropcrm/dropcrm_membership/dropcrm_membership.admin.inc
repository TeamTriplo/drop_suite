<?php
/**
 * @file
 * Admin pages and forms for DropCRM Membership.
 *
 * The admin list at drop/crm/memberships is rendered by the default View
 * config (views.view.crm_memberships_admin.json). The expiring block and
 * contact-page Memberships section are also Views — block displays
 * placed via the Layouts system. This file provides only: the view page,
 * add / edit / renew / cancel / payment / term CRUD forms, and the
 * reports page at drop/crm/reports/memberships.
 */

/**
 * Membership view page.
 */
function dropcrm_membership_view_page($membership) {
  $build = array();
  $contact = entity_load('crm_contact', $membership->cid);
  $types = _dropcrm_membership_types();
  $type_label = isset($types[$membership->type]['label']) ? $types[$membership->type]['label'] : $membership->type;

  // Action bar — per-entity verbs. Edit sits with View in Local Tasks (not
  // here). Delete is admin-only since it loses all history; soft-remove is
  // via Cancel.
  $actions = array();
  if (dropcrm_membership_access('update', $membership)) {
    $actions[] = l(t('Renew'), 'drop/crm/memberships/' . $membership->mid . '/renew');
    $actions[] = l(t('Record payment'), 'drop/crm/memberships/' . $membership->mid . '/payment/add');
  }
  if (user_access('cancel dropcrm_membership') && (int) $membership->status !== DROPCRM_MEMBERSHIP_STATUS_CANCELLED) {
    $actions[] = l(t('Cancel membership'), 'drop/crm/memberships/' . $membership->mid . '/cancel');
  }
  if ($actions) {
    $build['actions'] = array(
      '#markup' => drop_base_page_actions($actions),
      '#weight' => -100,
    );
  }

  // Summary.
  $rows = array();
  $rows[] = array(t('Contact'), $contact ? l($contact->label(), 'drop/crm/contact/' . $contact->cid) : t('(unknown)'));
  $rows[] = array(t('Member ID'), check_plain($membership->member_id));
  $rows[] = array(t('Type'), check_plain($type_label));
  $rows[] = array(t('Status'), array('data' => _dropcrm_membership_status_badge($membership->status)));
  $rows[] = array(t('Joined'), $membership->joinedDate() ?: t('—'));
  $rows[] = array(t('Current term ends'), $membership->currentEndDate() ?: t('—'));
  $rows[] = array(t('Total paid'), '$' . number_format(_dropcrm_membership_total_paid($membership->mid), 2));
  $build['summary'] = array(
    '#theme'  => 'table',
    '#rows'   => $rows,
    '#attributes' => array('class' => array('crm-membership-summary')),
  );

  // Term history.
  $term_rows = array();
  $term_query = db_query('SELECT tid, start_date, end_date, status, notes FROM {crm_membership_term} WHERE mid = :mid ORDER BY start_date DESC, tid DESC', array(':mid' => $membership->mid));
  $term_count = 0;
  $term_records = array();
  foreach ($term_query as $t) {
    $term_records[] = $t;
    $term_count++;
  }
  foreach ($term_records as $t) {
    $ops = array();
    if (user_access('manage dropcrm_membership')) {
      $ops[] = l(t('Edit'), 'drop/crm/memberships/' . $membership->mid . '/term/' . $t->tid . '/edit');
    }
    // Only offer delete if at least one term will remain — the membership
    // must always have at least one term to have meaningful state.
    if (user_access('administer dropcrm_membership') && $term_count > 1) {
      $ops[] = l(t('Delete'), 'drop/crm/memberships/' . $membership->mid . '/term/' . $t->tid . '/delete');
    }
    $term_rows[] = array(
      $t->start_date,
      !empty($t->end_date) ? $t->end_date : t('lifetime'),
      array('data' => _dropcrm_membership_status_badge($t->status)),
      check_plain($t->notes),
      implode(' | ', $ops),
    );
  }
  $build['terms_header'] = array('#markup' => '<h3>' . t('Term history') . '</h3>');
  $build['terms'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Start'), t('End'), t('Status'), t('Notes'), t('Operations')),
    '#rows'   => $term_rows,
    '#empty'  => t('No terms recorded.'),
  );

  // Payment history.
  $pay_rows = array();
  $pay_query = db_query('SELECT pid, paid_date, amount, method, reference, notes FROM {crm_membership_payment} WHERE mid = :mid ORDER BY paid_date DESC, pid DESC', array(':mid' => $membership->mid));
  foreach ($pay_query as $p) {
    $ops = array();
    if (user_access('manage dropcrm_membership')) {
      $ops[] = l(t('Edit'), 'drop/crm/memberships/' . $membership->mid . '/payment/' . $p->pid . '/edit');
    }
    if (user_access('administer dropcrm_membership')) {
      $ops[] = l(t('Delete'), 'drop/crm/memberships/' . $membership->mid . '/payment/' . $p->pid . '/delete');
    }
    $pay_rows[] = array(
      $p->paid_date,
      '$' . number_format((float) $p->amount, 2),
      check_plain($p->method),
      check_plain($p->reference),
      check_plain($p->notes),
      implode(' | ', $ops),
    );
  }
  $build['payments_header'] = array('#markup' => '<h3>' . t('Payment history') . '</h3>');
  $build['payments'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Date'), t('Amount'), t('Method'), t('Reference'), t('Notes'), t('Operations')),
    '#rows'   => $pay_rows,
    '#empty'  => t('No payments recorded.'),
  );

  return $build;
}

/**
 * Add/edit form for a membership.
 *
 * On create: also generates the initial term based on the chosen type's
 * term_length. On edit: edits the membership row only. Renewals go through
 * the separate renew form.
 */
function dropcrm_membership_form($form, &$form_state, $membership = NULL) {
  $is_new = !$membership;
  if ($is_new) {
    $membership = entity_create('crm_membership', array(
      'cid'       => isset($_GET['cid']) ? (int) $_GET['cid'] : 0,
      'member_id' => '',
      'type'      => '',
      'status'    => DROPCRM_MEMBERSHIP_STATUS_ACTIVE,
    ));
  }
  $form_state['crm_membership'] = $membership;

  // Contact picker — textfield with dropcrm's contact autocomplete callback.
  // The callback returns entries like "Julia Whitney [cid:42]" — the cid is
  // parsed out in the submit handler.
  $contact_default = '';
  if (!empty($membership->cid)) {
    $existing = entity_load('crm_contact', array($membership->cid));
    if ($existing) {
      $existing = reset($existing);
      $contact_default = $existing->label() . ' [cid:' . $membership->cid . ']';
    }
  }
  $form['cid'] = array(
    '#type'              => 'textfield',
    '#title'             => t('Member (contact)'),
    '#description'       => t('The person, organization, or household being enrolled. Start typing a name; pick a match from the list. This is the member — Membership Type below is the plan they are signing up for.'),
    '#autocomplete_path' => 'drop/crm/autocomplete/contacts',
    '#default_value'     => $contact_default,
    '#required'          => TRUE,
    '#disabled'          => !$is_new,
    '#maxlength'         => 255,
  );

  // Member ID is auto-generated on save. Shown here read-only on edit so the
  // admin can see it; hidden on create since it isn't assigned yet.
  if (!$is_new && !empty($membership->member_id)) {
    $form['member_id_display'] = array(
      '#type'        => 'item',
      '#title'       => t('Member ID'),
      '#markup'      => '<code>' . check_plain($membership->member_id) . '</code>',
      '#description' => t('Assigned automatically when the membership was created.'),
    );
  }

  $form['type'] = array(
    '#type'          => 'select',
    '#title'         => t('Membership Type'),
    '#options'       => _dropcrm_membership_type_options(),
    '#default_value' => $membership->type,
    '#required'      => TRUE,
    '#empty_option'  => t('- Choose -'),
  );

  $form['status'] = array(
    '#type'          => 'select',
    '#title'         => t('Status'),
    '#options'       => _dropcrm_membership_status_options(),
    '#default_value' => $membership->status,
    '#description'   => t('Status is normally managed automatically. Override only if needed.'),
  );

  if ($is_new) {
    $form['initial_term'] = array(
      '#type'        => 'fieldset',
      '#title'       => t('Initial term'),
      '#description' => t("The first term is created along with the membership. Leave end date blank for a lifetime membership."),
    );
    $form['initial_term']['start_date'] = array(
      '#type'          => 'date',
      '#title'         => t('Start date'),
      '#default_value' => _dropcrm_membership_date_default_today(),
      '#required'      => TRUE,
    );
    $form['initial_term']['use_term_length'] = array(
      '#type'          => 'checkbox',
      '#title'         => t('Auto-calculate end date from type term length'),
      '#default_value' => 1,
      '#description'   => t('Uncheck to set end date manually (or leave blank for lifetime).'),
    );
    $today = format_date(REQUEST_TIME, 'custom', 'Y-m-d');
    $default_end = _dropcrm_membership_add_term_length($today, '1 year');
    $form['initial_term']['end_date'] = array(
      '#type'          => 'date',
      '#title'         => t('End date'),
      '#description'   => t('Used only if auto-calculate is off. Leave blank for a lifetime membership.'),
      '#default_value' => _dropcrm_membership_date_to_element($default_end),
    );
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_new ? t('Create membership') : t('Save'),
  );
  if (!$is_new) {
    $form['actions']['cancel'] = array(
      '#type'  => 'link',
      '#title' => t('Cancel'),
      '#href'  => 'drop/crm/memberships/' . $membership->mid,
    );
  }

  return $form;
}

/**
 * Validate handler for the membership form.
 */
function dropcrm_membership_form_validate($form, &$form_state) {
  $v = $form_state['values'];

  // Parse the contact picker's "Name [cid:NNN]" autocomplete format.
  // Only parse on a new membership (field is disabled when editing).
  if (empty($form_state['crm_membership']->mid)) {
    $cid = _dropcrm_membership_parse_cid($v['cid']);
    if (!$cid) {
      form_set_error('cid', t('Please pick a contact from the autocomplete suggestions.'));
    }
    else {
      $exists = db_query('SELECT cid FROM {crm_contact} WHERE cid = :cid', array(':cid' => $cid))->fetchField();
      if (!$exists) {
        form_set_error('cid', t('That contact could not be found. Pick again from the autocomplete suggestions.'));
      }
      else {
        $form_state['values']['cid'] = $cid;
      }
    }
  }

  // If creating: validate initial term dates.
  if (empty($form_state['crm_membership']->mid)) {
    $start = _dropcrm_membership_date_from_element($v['start_date']);
    if (empty($start)) {
      form_set_error('start_date', t('Start date is required.'));
    }
    if (empty($v['use_term_length']) && !empty($v['end_date'])) {
      $end = _dropcrm_membership_date_from_element($v['end_date']);
      if ($end < $start) {
        form_set_error('end_date', t('End date must be on or after the start date.'));
      }
    }
  }
}

/**
 * Submit handler: saves the membership row and (on create) the initial term.
 */
function dropcrm_membership_form_submit($form, &$form_state) {
  $v = $form_state['values'];
  $membership = $form_state['crm_membership'];
  $is_new = empty($membership->mid);

  // By this point validate has parsed $v['cid'] to a plain int. On edit the
  // field is disabled and cid is unchanged on the entity.
  $membership->cid    = (int) $v['cid'];
  $membership->type   = $v['type'];
  $membership->status = (int) $v['status'];

  $membership->save();

  // Auto-assign Member ID derived from the entity's mid. Saves twice on
  // create because mid isn't known until after the first save. Existing
  // memberships with a manually-set member_id are left alone.
  if (empty($membership->member_id)) {
    $membership->member_id = _dropcrm_membership_generate_member_id($membership->mid);
    $membership->save();
  }

  if ($is_new) {
    $start_date = _dropcrm_membership_date_from_element($v['start_date']);
    if (!empty($v['use_term_length'])) {
      $types = _dropcrm_membership_types();
      $term_length = isset($types[$v['type']]['term_length']) ? $types[$v['type']]['term_length'] : '1 year';
      $end_date = _dropcrm_membership_add_term_length($start_date, $term_length);
    }
    else {
      $end_date = !empty($v['end_date']) ? _dropcrm_membership_date_from_element($v['end_date']) : NULL;
    }

    $term = entity_create('crm_membership_term', array(
      'mid'        => $membership->mid,
      'start_date' => $start_date,
      'end_date'   => $end_date,
      'status'     => DROPCRM_MEMBERSHIP_STATUS_ACTIVE,
      'notes'      => '',
    ));
    $term->save();

    backdrop_set_message(t('Membership created. <a href="@url">View it</a>.', array('@url' => url('drop/crm/memberships/' . $membership->mid))));
  }
  else {
    backdrop_set_message(t('Membership updated.'));
  }

  $form_state['redirect'] = 'drop/crm/memberships/' . $membership->mid;
}

/**
 * Renewal form: creates a new term for an existing membership.
 */
function dropcrm_membership_renew_form($form, &$form_state, $membership) {
  $form_state['crm_membership'] = $membership;

  $types = _dropcrm_membership_types();
  $type_conf = isset($types[$membership->type]) ? $types[$membership->type] : array();
  $term_length = isset($type_conf['term_length']) ? $type_conf['term_length'] : '1 year';
  $default_dues = isset($type_conf['default_dues']) ? $type_conf['default_dues'] : '';
  $type_label = isset($type_conf['label']) ? $type_conf['label'] : $membership->type;

  $prior_end = $membership->currentEndDate();
  $default_start = ($prior_end && $prior_end > format_date(REQUEST_TIME, 'custom', 'Y-m-d'))
    ? $prior_end
    : format_date(REQUEST_TIME, 'custom', 'Y-m-d');
  $default_end = _dropcrm_membership_add_term_length($default_start, $term_length);

  // Confirmation block: show which member we are renewing so the user can
  // verify they are on the right page before committing. Guards against
  // URL-tampering / misrouting bugs adding a term to the wrong contact.
  $contact = entity_load('crm_contact', $membership->cid);
  $contact_name = $contact ? check_plain($contact->label()) : t('(unknown contact)');
  $contact_link = $contact
    ? l($contact->label(), 'drop/crm/contact/' . $contact->cid, array('attributes' => array('target' => '_blank')))
    : $contact_name;
  $rows = array(
    array(array('header' => TRUE, 'data' => t('Member')), $contact_link),
    array(array('header' => TRUE, 'data' => t('Membership type')), check_plain($type_label)),
    array(array('header' => TRUE, 'data' => t('Current term ends')), $prior_end ?: t('—')),
  );
  if (!empty($membership->member_id)) {
    array_splice($rows, 1, 0, array(array(array('header' => TRUE, 'data' => t('Member ID')), check_plain($membership->member_id))));
  }
  $form['confirm_member'] = array(
    '#theme'      => 'table',
    '#rows'       => $rows,
    '#attributes' => array('class' => array('crm-membership-renew-confirm')),
    '#prefix'     => '<div class="crm-membership-renew-confirm-wrap"><h3>' . t('Renewing this membership:') . '</h3>',
    '#suffix'     => '</div>',
    '#weight'     => -10,
  );

  $form['help'] = array(
    '#markup' => '<p>' . t('Records a new term for this membership. The prior term is preserved in the history.') . '</p>',
    '#weight' => -5,
  );

  $form['start_date'] = array(
    '#type'          => 'date',
    '#title'         => t('Term start'),
    '#default_value' => _dropcrm_membership_date_to_element($default_start),
    '#required'      => TRUE,
  );
  $form['end_date'] = array(
    '#type'          => 'date',
    '#title'         => t('Term end'),
    '#default_value' => _dropcrm_membership_date_to_element($default_end),
    '#description'   => t('Leave blank for a lifetime term.'),
  );
  $form['notes'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Term notes'),
    '#maxlength'     => 255,
  );

  $form['payment'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Record payment with this renewal'),
    '#description' => t('Optional. Leave amount blank to skip.'),
  );
  $form['payment']['pay_amount'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Amount'),
    '#default_value' => $default_dues,
    '#size'          => 10,
    '#field_prefix'  => '$',
  );
  $form['payment']['pay_paid_date'] = array(
    '#type'          => 'date',
    '#title'         => t('Paid date'),
    '#default_value' => _dropcrm_membership_date_default_today(),
  );
  $form['payment']['pay_method'] = array(
    '#type'          => 'select',
    '#title'         => t('Method'),
    '#options'       => _dropcrm_membership_payment_method_options(),
    '#empty_option'  => t('- Choose -'),
  );
  $form['payment']['pay_reference'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Reference'),
    '#maxlength'     => 255,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Renew'),
  );
  $form['actions']['cancel'] = array(
    '#type'  => 'link',
    '#title' => t('Cancel'),
    '#href'  => 'drop/crm/memberships/' . $membership->mid,
  );

  return $form;
}

/**
 * Validate handler for the renew form.
 */
function dropcrm_membership_renew_form_validate($form, &$form_state) {
  $v = $form_state['values'];
  $start = _dropcrm_membership_date_from_element($v['start_date']);
  if (empty($start)) {
    form_set_error('start_date', t('Start date is required.'));
  }
  if (!empty($v['end_date'])) {
    $end = _dropcrm_membership_date_from_element($v['end_date']);
    if (!empty($end) && $end < $start) {
      form_set_error('end_date', t('End date must be on or after the start date.'));
    }
  }
  if (trim((string) $v['pay_amount']) !== '' && !is_numeric($v['pay_amount'])) {
    form_set_error('pay_amount', t('Payment amount must be numeric.'));
  }
}

/**
 * Submit handler for the renew form.
 */
function dropcrm_membership_renew_form_submit($form, &$form_state) {
  $v = $form_state['values'];
  $membership = $form_state['crm_membership'];

  $start = _dropcrm_membership_date_from_element($v['start_date']);
  $end = !empty($v['end_date']) ? _dropcrm_membership_date_from_element($v['end_date']) : NULL;

  $term = entity_create('crm_membership_term', array(
    'mid'        => $membership->mid,
    'start_date' => $start,
    'end_date'   => $end,
    'status'     => DROPCRM_MEMBERSHIP_STATUS_ACTIVE,
    'notes'      => isset($v['notes']) ? $v['notes'] : '',
  ));
  $term->save();

  // Recompute status from the new latest term. Normally flips to ACTIVE, but
  // correctly handles backdated end dates.
  _dropcrm_membership_recompute_status($membership->mid);

  // Optional payment.
  if (trim((string) $v['pay_amount']) !== '') {
    $payment = entity_create('crm_membership_payment', array(
      'mid'        => $membership->mid,
      'tid'        => $term->tid,
      'amount'     => (float) $v['pay_amount'],
      'paid_date'  => _dropcrm_membership_date_from_element($v['pay_paid_date']),
      'method'     => $v['pay_method'],
      'reference'  => $v['pay_reference'],
      'notes'      => '',
    ));
    $payment->save();
  }

  // Log a CRM activity.
  _dropcrm_membership_log_activity($membership, 'membership_renewal', $start, $end);

  backdrop_set_message(t('Membership renewed.'));
  $form_state['redirect'] = 'drop/crm/memberships/' . $membership->mid;
}

/**
 * Payment add/edit form.
 */
function dropcrm_membership_payment_form($form, &$form_state, $membership, $payment = NULL) {
  $is_new = !$payment;
  if ($is_new) {
    $payment = entity_create('crm_membership_payment', array(
      'mid'       => $membership->mid,
      'tid'       => NULL,
      'amount'    => '',
      'paid_date' => format_date(REQUEST_TIME, 'custom', 'Y-m-d'),
      'method'    => '',
      'reference' => '',
      'notes'     => '',
    ));
  }
  $form_state['crm_membership'] = $membership;
  $form_state['crm_membership_payment'] = $payment;

  // Offer to link to a specific term (optional).
  $term_options = array('' => t('- Not linked to a specific term -'));
  $term_query = db_query('SELECT tid, start_date, end_date FROM {crm_membership_term} WHERE mid = :mid ORDER BY start_date DESC, tid DESC', array(':mid' => $membership->mid));
  foreach ($term_query as $t) {
    $term_options[$t->tid] = $t->start_date . ' – ' . (!empty($t->end_date) ? $t->end_date : t('lifetime'));
  }
  $form['tid'] = array(
    '#type'          => 'select',
    '#title'         => t('Applies to term'),
    '#options'       => $term_options,
    '#default_value' => !empty($payment->tid) ? $payment->tid : '',
  );

  $form['amount'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Amount'),
    '#default_value' => $payment->amount,
    '#required'      => TRUE,
    '#size'          => 10,
    '#field_prefix'  => '$',
  );
  $form['paid_date'] = array(
    '#type'          => 'date',
    '#title'         => t('Paid date'),
    '#default_value' => _dropcrm_membership_date_to_element($payment->paid_date),
    '#required'      => TRUE,
  );
  $form['method'] = array(
    '#type'          => 'select',
    '#title'         => t('Method'),
    '#options'       => _dropcrm_membership_payment_method_options(),
    '#default_value' => $payment->method,
    '#empty_option'  => t('- Choose -'),
    '#required'      => TRUE,
  );
  $form['reference'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Reference'),
    '#description'   => t('Check number, transaction ID, etc.'),
    '#default_value' => $payment->reference,
    '#maxlength'     => 255,
  );
  $form['notes'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Notes'),
    '#default_value' => $payment->notes,
    '#rows'          => 3,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_new ? t('Record payment') : t('Save'),
  );
  $form['actions']['cancel'] = array(
    '#type'  => 'link',
    '#title' => t('Cancel'),
    '#href'  => 'drop/crm/memberships/' . $membership->mid,
  );

  return $form;
}

/**
 * Validate handler for payment form.
 */
function dropcrm_membership_payment_form_validate($form, &$form_state) {
  $v = $form_state['values'];
  if (!is_numeric($v['amount'])) {
    form_set_error('amount', t('Amount must be numeric.'));
  }
}

/**
 * Submit handler for payment form.
 */
function dropcrm_membership_payment_form_submit($form, &$form_state) {
  $v = $form_state['values'];
  $membership = $form_state['crm_membership'];
  $payment = $form_state['crm_membership_payment'];

  $payment->mid       = $membership->mid;
  $payment->tid       = !empty($v['tid']) ? (int) $v['tid'] : NULL;
  $payment->amount    = (float) $v['amount'];
  $payment->paid_date = _dropcrm_membership_date_from_element($v['paid_date']);
  $payment->method    = $v['method'];
  $payment->reference = $v['reference'];
  $payment->notes     = $v['notes'];
  $payment->save();

  backdrop_set_message(t('Payment recorded.'));
  $form_state['redirect'] = 'drop/crm/memberships/' . $membership->mid;
}

/**
 * Confirm + delete form for a payment.
 */
function dropcrm_membership_payment_delete_form($form, &$form_state, $membership, $payment) {
  $form_state['crm_membership'] = $membership;
  $form_state['crm_membership_payment'] = $payment;
  return confirm_form(
    $form,
    t('Delete this payment of $@amount?', array('@amount' => number_format((float) $payment->amount, 2))),
    'drop/crm/memberships/' . $membership->mid,
    t('This cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

function dropcrm_membership_payment_delete_form_submit($form, &$form_state) {
  $pid = $form_state['crm_membership_payment']->pid;
  entity_get_controller('crm_membership_payment')->delete(array($pid));
  backdrop_set_message(t('Payment deleted.'));
  $form_state['redirect'] = 'drop/crm/memberships/' . $form_state['crm_membership']->mid;
}

/**
 * Confirmation form to cancel a membership.
 */
function dropcrm_membership_cancel_form($form, &$form_state, $membership) {
  $form_state['crm_membership'] = $membership;
  return confirm_form(
    $form,
    t('Cancel this membership?'),
    'drop/crm/memberships/' . $membership->mid,
    t('The membership will be marked cancelled. Term and payment history are preserved. An activity will be logged on the contact timeline.'),
    t('Cancel membership'),
    t('Keep')
  );
}

function dropcrm_membership_cancel_form_submit($form, &$form_state) {
  $membership = $form_state['crm_membership'];
  $membership->status = DROPCRM_MEMBERSHIP_STATUS_CANCELLED;
  $membership->save();
  _dropcrm_membership_log_activity($membership, 'membership_cancelled', NULL, NULL);
  backdrop_set_message(t('Membership cancelled.'));
  $form_state['redirect'] = 'drop/crm/memberships/' . $membership->mid;
}

// ---------------------------------------------------------------------------
// Date + activity helpers (admin-inc scope)
// ---------------------------------------------------------------------------

/**
 * Backdrop's #type date returns an array of year/month/day. Converts to
 * YYYY-MM-DD. Returns NULL if empty.
 */
function _dropcrm_membership_date_from_element($value) {
  if (is_string($value) && preg_match('/^\d{4}-\d{2}-\d{2}$/', $value)) {
    return $value;
  }
  if (is_array($value) && !empty($value['year']) && !empty($value['month']) && !empty($value['day'])) {
    return sprintf('%04d-%02d-%02d', (int) $value['year'], (int) $value['month'], (int) $value['day']);
  }
  return NULL;
}

/**
 * Converts a YYYY-MM-DD string into the array format expected by #type date.
 */
function _dropcrm_membership_date_to_element($date) {
  if (empty($date) || $date === '0000-00-00') {
    return NULL;
  }
  $ts = strtotime($date);
  return array(
    'year'  => (int) format_date($ts, 'custom', 'Y'),
    'month' => (int) format_date($ts, 'custom', 'n'),
    'day'   => (int) format_date($ts, 'custom', 'j'),
  );
}

function _dropcrm_membership_date_default_today() {
  return _dropcrm_membership_date_to_element(format_date(REQUEST_TIME, 'custom', 'Y-m-d'));
}

/**
 * Parses a cid out of the "Name [cid:NNN]" autocomplete format emitted by
 * dropcrm_contact_autocomplete(). Returns 0 if no cid can be extracted.
 */
function _dropcrm_membership_parse_cid($value) {
  if (is_numeric($value)) {
    return (int) $value;
  }
  if (is_string($value) && preg_match('/\[cid:(\d+)\]/', $value, $m)) {
    return (int) $m[1];
  }
  return 0;
}

/**
 * Creates a crm_activity row for a membership event.
 */
function _dropcrm_membership_log_activity($membership, $activity_type, $start_date = NULL, $end_date = NULL) {
  if (!module_exists('dropcrm')) {
    return;
  }
  $types = _dropcrm_membership_types();
  $type_label = isset($types[$membership->type]['label']) ? $types[$membership->type]['label'] : $membership->type;

  switch ($activity_type) {
    case 'membership_renewal':
      $subject = t('Renewed @type membership', array('@type' => $type_label));
      if (!empty($end_date)) {
        $subject .= ' ' . t('through @end', array('@end' => $end_date));
      }
      break;
    case 'membership_cancelled':
      $subject = t('Cancelled @type membership', array('@type' => $type_label));
      break;
    default:
      $subject = t('Membership event');
  }

  global $user;
  $activity = entity_create('crm_activity', array(
    'subject'       => (string) $subject,
    'type'          => $activity_type,
    'status'        => 'completed',
    'priority'      => 1,
    'activity_date' => REQUEST_TIME,
    'is_outreach'   => 0,
    'uid'           => $user->uid,
  ));
  // Attach the contact via field_crm_contacts.
  $activity->field_crm_contacts[LANGUAGE_NONE][0]['target_id'] = $membership->cid;
  $activity->save();
}

// ---------------------------------------------------------------------------
// Term edit + delete
// ---------------------------------------------------------------------------

/**
 * Form builder: edit an existing membership term.
 *
 * Admin can correct start/end dates, status, and notes after the fact —
 * useful for fixing a miskeyed renewal or back-dating a manually-recorded
 * term. Creation of new terms still goes through the Renew form.
 */
function dropcrm_membership_term_form($form, &$form_state, $membership, $term) {
  $form_state['crm_membership']      = $membership;
  $form_state['crm_membership_term'] = $term;

  backdrop_set_title(t('Edit term for %label', array('%label' => $membership->label())));

  $form['help'] = array(
    '#markup' => '<p>' . t('Editing a term after the fact. Save will also recompute the membership\'s overall status from all current terms.') . '</p>',
  );

  $form['start_date'] = array(
    '#type'          => 'date',
    '#title'         => t('Term start'),
    '#default_value' => _dropcrm_membership_date_to_element($term->start_date),
    '#required'      => TRUE,
  );
  $form['end_date'] = array(
    '#type'          => 'date',
    '#title'         => t('Term end'),
    '#default_value' => !empty($term->end_date) ? _dropcrm_membership_date_to_element($term->end_date) : NULL,
    '#description'   => t('Leave blank for a lifetime term.'),
  );
  $form['status'] = array(
    '#type'          => 'select',
    '#title'         => t('Term status'),
    '#options'       => _dropcrm_membership_status_options(),
    '#default_value' => $term->status,
  );
  $form['notes'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Notes'),
    '#default_value' => $term->notes,
    '#rows'          => 3,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save term'),
  );
  $form['actions']['cancel'] = array(
    '#type'  => 'link',
    '#title' => t('Cancel'),
    '#href'  => 'drop/crm/memberships/' . $membership->mid,
  );

  return $form;
}

/**
 * Validate: end date can't be before start date (unless blank = lifetime).
 */
function dropcrm_membership_term_form_validate($form, &$form_state) {
  $start = _dropcrm_membership_date_from_element($form_state['values']['start_date']);
  if (empty($start)) {
    form_set_error('start_date', t('Start date is required.'));
    return;
  }
  if (!empty($form_state['values']['end_date'])) {
    $end = _dropcrm_membership_date_from_element($form_state['values']['end_date']);
    if ($end && $end < $start) {
      form_set_error('end_date', t('End date must be on or after the start date.'));
    }
  }
}

/**
 * Submit: save the term and recompute the membership's current status.
 */
function dropcrm_membership_term_form_submit($form, &$form_state) {
  $membership = $form_state['crm_membership'];
  $term       = $form_state['crm_membership_term'];

  $term->start_date = _dropcrm_membership_date_from_element($form_state['values']['start_date']);
  $term->end_date   = !empty($form_state['values']['end_date'])
    ? _dropcrm_membership_date_from_element($form_state['values']['end_date'])
    : NULL;
  $term->status = (int) $form_state['values']['status'];
  $term->notes  = (string) $form_state['values']['notes'];
  $term->save();

  _dropcrm_membership_recompute_status($membership->mid);

  backdrop_set_message(t('Term updated.'));
  $form_state['redirect'] = 'drop/crm/memberships/' . $membership->mid;
}

/**
 * Confirm-delete form for a membership term.
 */
function dropcrm_membership_term_delete_form($form, &$form_state, $membership, $term) {
  $form_state['crm_membership']      = $membership;
  $form_state['crm_membership_term'] = $term;

  $range = !empty($term->end_date)
    ? t('from @start to @end', array('@start' => $term->start_date, '@end' => $term->end_date))
    : t('starting @start (lifetime)', array('@start' => $term->start_date));

  return confirm_form(
    $form,
    t('Delete this term (@range)?', array('@range' => $range)),
    'drop/crm/memberships/' . $membership->mid,
    t('Term history is an audit of each renewal. Delete only to correct a mistaken entry. The membership\'s overall status will be recomputed from the remaining terms. This cannot be undone.'),
    t('Delete term'),
    t('Cancel')
  );
}

/**
 * Submit: delete the term and recompute the membership's status.
 */
function dropcrm_membership_term_delete_form_submit($form, &$form_state) {
  $membership = $form_state['crm_membership'];
  $term       = $form_state['crm_membership_term'];

  // Safety: never delete the last remaining term — the membership would
  // have no temporal data. UI already hides the link in this case, but
  // guard in case of a direct URL visit.
  $count = (int) db_query('SELECT COUNT(*) FROM {crm_membership_term} WHERE mid = :mid', array(':mid' => $membership->mid))->fetchField();
  if ($count <= 1) {
    backdrop_set_message(t('Cannot delete the only remaining term for this membership.'), 'warning');
    $form_state['redirect'] = 'drop/crm/memberships/' . $membership->mid;
    return;
  }

  entity_delete('crm_membership_term', $term->tid);
  _dropcrm_membership_recompute_status($membership->mid);

  backdrop_set_message(t('Term deleted.'));
  $form_state['redirect'] = 'drop/crm/memberships/' . $membership->mid;
}

// ---------------------------------------------------------------------------
// Reports page
// ---------------------------------------------------------------------------

/**
 * Page callback: Memberships report at drop/crm/reports/memberships.
 *
 * Shows active count + per-type breakdown, new memberships in period,
 * renewals in period, and revenue in period + per-type breakdown. Period
 * is read from GET params so the URL is bookmarkable.
 */
function dropcrm_membership_report_page() {
  $period = _dropcrm_membership_report_resolve_period();

  $output = array();

  // Period selector form at top.
  $output['period_form'] = backdrop_get_form('dropcrm_membership_report_period_form', $period);

  // Resolve human period label for headings.
  $output['period_label'] = array(
    '#markup' => '<h3>' . t('Showing: @label', array('@label' => $period['label'])) . '</h3>',
  );

  $types = _dropcrm_membership_types();

  // ---- Active memberships ----
  $active_total = (int) db_query('SELECT COUNT(*) FROM {crm_membership} WHERE status = :s', array(':s' => DROPCRM_MEMBERSHIP_STATUS_ACTIVE))->fetchField();
  $grace_total  = (int) db_query('SELECT COUNT(*) FROM {crm_membership} WHERE status = :s', array(':s' => DROPCRM_MEMBERSHIP_STATUS_GRACE))->fetchField();

  $active_by_type = array();
  $result = db_query('SELECT type, COUNT(*) AS c FROM {crm_membership} WHERE status = :s GROUP BY type', array(':s' => DROPCRM_MEMBERSHIP_STATUS_ACTIVE));
  foreach ($result as $row) {
    $active_by_type[$row->type] = (int) $row->c;
  }

  // ---- New memberships in period ----
  // New = first term's start_date falls in the period.
  $new_count = (int) db_query('
    SELECT COUNT(*) FROM {crm_membership} m
    WHERE (
      SELECT MIN(start_date) FROM {crm_membership_term} t WHERE t.mid = m.mid
    ) BETWEEN :start AND :end
  ', array(':start' => $period['start'], ':end' => $period['end']))->fetchField();

  // ---- Renewals in period ----
  // Renewal = term row whose start_date is in the period AND is not the
  // earliest term for its membership.
  $renewal_count = (int) db_query('
    SELECT COUNT(*) FROM {crm_membership_term} t
    WHERE t.start_date BETWEEN :start AND :end
      AND t.tid != (SELECT MIN(tid) FROM {crm_membership_term} WHERE mid = t.mid)
  ', array(':start' => $period['start'], ':end' => $period['end']))->fetchField();

  // ---- Lapsed in period ----
  // Approximate: memberships whose latest term end_date + grace_days fell in
  // the period. Can't perfectly reconstruct historical status transitions
  // without an audit log, so this is the best proxy.
  $lapsed_count = 0;
  $lapsed_rows = db_query('
    SELECT m.type, m.current_end_date
    FROM {crm_membership} m
    WHERE m.current_end_date IS NOT NULL
      AND m.current_end_date < :end
  ', array(':end' => $period['end']));
  foreach ($lapsed_rows as $row) {
    $grace_days = isset($types[$row->type]['grace_days']) ? (int) $types[$row->type]['grace_days'] : 30;
    $lapsed_date = format_date(strtotime($row->current_end_date) + ($grace_days * 86400), 'custom', 'Y-m-d');
    if ($lapsed_date >= $period['start'] && $lapsed_date <= $period['end']) {
      $lapsed_count++;
    }
  }

  // ---- Revenue in period ----
  $revenue = (float) db_query('
    SELECT COALESCE(SUM(amount), 0) FROM {crm_membership_payment}
    WHERE paid_date BETWEEN :start AND :end
  ', array(':start' => $period['start'], ':end' => $period['end']))->fetchField();

  $revenue_by_type = array();
  $rev_rows = db_query('
    SELECT m.type, COALESCE(SUM(p.amount), 0) AS rev, COUNT(p.pid) AS cnt
    FROM {crm_membership_payment} p
    INNER JOIN {crm_membership} m ON m.mid = p.mid
    WHERE p.paid_date BETWEEN :start AND :end
    GROUP BY m.type
  ', array(':start' => $period['start'], ':end' => $period['end']));
  foreach ($rev_rows as $row) {
    $revenue_by_type[$row->type] = array(
      'amount' => (float) $row->rev,
      'count'  => (int) $row->cnt,
    );
  }

  // ---- Render: four headline numbers ----
  $output['headline'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Metric'), t('Value')),
    '#rows'   => array(
      array(t('Active memberships'), $active_total),
      array(t('In grace period'), $grace_total),
      array(t('New in period'), $new_count),
      array(t('Renewals in period'), $renewal_count),
      array(t('Lapsed in period'), $lapsed_count),
      array(t('Revenue in period'), '$' . number_format($revenue, 2)),
    ),
    '#attributes' => array('class' => array('crm-membership-report-headline')),
  );

  // ---- Render: active by type ----
  if (!empty($active_by_type)) {
    $rows = array();
    foreach ($active_by_type as $type_key => $count) {
      $label = isset($types[$type_key]['label']) ? $types[$type_key]['label'] : $type_key;
      $rows[] = array(check_plain($label), $count);
    }
    $output['active_by_type_header'] = array(
      '#markup' => '<h3>' . t('Active memberships by type') . '</h3>',
    );
    $output['active_by_type'] = array(
      '#theme'  => 'table',
      '#header' => array(t('Type'), t('Active count')),
      '#rows'   => $rows,
    );
  }

  // ---- Render: revenue by type ----
  if (!empty($revenue_by_type)) {
    $rows = array();
    foreach ($revenue_by_type as $type_key => $row) {
      $label = isset($types[$type_key]['label']) ? $types[$type_key]['label'] : $type_key;
      $rows[] = array(
        check_plain($label),
        '$' . number_format($row['amount'], 2),
        $row['count'],
      );
    }
    $output['revenue_by_type_header'] = array(
      '#markup' => '<h3>' . t('Revenue in period by type') . '</h3>',
    );
    $output['revenue_by_type'] = array(
      '#theme'  => 'table',
      '#header' => array(t('Type'), t('Revenue'), t('Payment count')),
      '#rows'   => $rows,
    );
  }
  else {
    $output['revenue_empty'] = array(
      '#markup' => '<p>' . t('No payments recorded in this period.') . '</p>',
    );
  }

  return $output;
}

/**
 * Period selector form for the Memberships report. Submits via GET so the
 * resolved period is part of the URL (bookmarkable, shareable).
 */
function dropcrm_membership_report_period_form($form, &$form_state, $period) {
  $form['#method'] = 'get';

  $form['period'] = array(
    '#type'          => 'select',
    '#title'         => t('Period'),
    '#options'       => array(
      'this_month'  => t('This month'),
      'last_month'  => t('Last month'),
      'this_year'   => t('This year'),
      'last_30'     => t('Last 30 days'),
      'last_90'     => t('Last 90 days'),
      'all_time'    => t('All time'),
      'custom'      => t('Custom range'),
    ),
    '#default_value' => isset($_GET['period']) ? $_GET['period'] : 'this_month',
  );

  $form['start'] = array(
    '#type'          => 'textfield',
    '#title'         => t('From (custom)'),
    '#default_value' => isset($_GET['start']) ? $_GET['start'] : '',
    '#size'          => 12,
    '#description'   => t('YYYY-MM-DD'),
    '#states'        => array(
      'visible' => array(':input[name="period"]' => array('value' => 'custom')),
    ),
  );
  $form['end'] = array(
    '#type'          => 'textfield',
    '#title'         => t('To (custom)'),
    '#default_value' => isset($_GET['end']) ? $_GET['end'] : '',
    '#size'          => 12,
    '#description'   => t('YYYY-MM-DD'),
    '#states'        => array(
      'visible' => array(':input[name="period"]' => array('value' => 'custom')),
    ),
  );

  $form['apply'] = array(
    '#type'  => 'submit',
    '#value' => t('Apply'),
  );

  return $form;
}

/**
 * Resolves the active period from GET params into a [start, end, label]
 * array with YYYY-MM-DD string dates.
 */
function _dropcrm_membership_report_resolve_period() {
  $period = isset($_GET['period']) ? $_GET['period'] : 'this_month';
  $today = REQUEST_TIME;

  switch ($period) {
    case 'last_month':
      $start = format_date(strtotime('first day of last month', $today), 'custom', 'Y-m-d');
      $end   = format_date(strtotime('last day of last month', $today), 'custom', 'Y-m-d');
      $label = t('Last month (@start to @end)', array('@start' => $start, '@end' => $end));
      break;

    case 'this_year':
      $start = format_date(strtotime('first day of january this year', $today), 'custom', 'Y-m-d');
      $end   = format_date($today, 'custom', 'Y-m-d');
      $label = t('This year (@start to today)', array('@start' => $start));
      break;

    case 'last_30':
      $start = format_date(strtotime('-30 days', $today), 'custom', 'Y-m-d');
      $end   = format_date($today, 'custom', 'Y-m-d');
      $label = t('Last 30 days');
      break;

    case 'last_90':
      $start = format_date(strtotime('-90 days', $today), 'custom', 'Y-m-d');
      $end   = format_date($today, 'custom', 'Y-m-d');
      $label = t('Last 90 days');
      break;

    case 'all_time':
      $start = '1970-01-01';
      $end   = format_date($today, 'custom', 'Y-m-d');
      $label = t('All time');
      break;

    case 'custom':
      $start = !empty($_GET['start']) && preg_match('/^\d{4}-\d{2}-\d{2}$/', $_GET['start']) ? $_GET['start'] : format_date(strtotime('first day of this month', $today), 'custom', 'Y-m-d');
      $end   = !empty($_GET['end'])   && preg_match('/^\d{4}-\d{2}-\d{2}$/', $_GET['end'])   ? $_GET['end']   : format_date($today, 'custom', 'Y-m-d');
      $label = t('Custom (@start to @end)', array('@start' => $start, '@end' => $end));
      break;

    case 'this_month':
    default:
      $start = format_date(strtotime('first day of this month', $today), 'custom', 'Y-m-d');
      $end   = format_date($today, 'custom', 'Y-m-d');
      $label = t('This month (@start to today)', array('@start' => $start));
      break;
  }

  return array(
    'start' => $start,
    'end'   => $end,
    'label' => $label,
  );
}
