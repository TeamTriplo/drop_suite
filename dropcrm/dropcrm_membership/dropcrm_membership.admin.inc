<?php
/**
 * @file
 * Admin pages and forms for DropCRM Membership.
 *
 * NOTE: The admin list at drop/crm/memberships is temporarily a hand-coded
 * table. It will be replaced by the default Views config
 * (views.view.crm_memberships_admin.json) in the Views step of the build.
 */

/**
 * Hand-coded list page at drop/crm/memberships. Will be replaced by a View.
 */
function dropcrm_membership_list_page() {
  $header = array(
    array('data' => t('Member ID'),    'field' => 'm.member_id'),
    array('data' => t('Contact'),      'field' => 'c.name'),
    array('data' => t('Type'),         'field' => 'm.type'),
    array('data' => t('Status'),       'field' => 'm.status'),
    array('data' => t('Term ends')),
    array('data' => t('Operations')),
  );

  $query = db_select('crm_membership', 'm')->extend('TableSort')->extend('PagerDefault');
  $query->leftJoin('crm_contact', 'c', 'c.cid = m.cid');
  $query->fields('m', array('mid', 'member_id', 'cid', 'type', 'status'));
  $query->fields('c', array('name'));
  $query->orderByHeader($header);
  $query->limit(50);

  // Filter: status (GET param).
  if (!empty($_GET['status']) && $_GET['status'] !== 'all') {
    $status_map = array(
      'active'    => DROPCRM_MEMBERSHIP_STATUS_ACTIVE,
      'grace'     => DROPCRM_MEMBERSHIP_STATUS_GRACE,
      'lapsed'    => DROPCRM_MEMBERSHIP_STATUS_LAPSED,
      'pending'   => DROPCRM_MEMBERSHIP_STATUS_PENDING,
      'cancelled' => DROPCRM_MEMBERSHIP_STATUS_CANCELLED,
    );
    if (isset($status_map[$_GET['status']])) {
      $query->condition('m.status', $status_map[$_GET['status']]);
    }
  }

  $rows = array();
  $types = _dropcrm_membership_types();
  foreach ($query->execute() as $row) {
    $type_label = isset($types[$row->type]['label']) ? $types[$row->type]['label'] : $row->type;
    $end_date = db_query('SELECT end_date FROM {crm_membership_term} WHERE mid = :mid ORDER BY start_date DESC, tid DESC LIMIT 1', array(':mid' => $row->mid))->fetchField();

    $ops = array();
    $ops[] = l(t('View'), 'drop/crm/memberships/' . $row->mid);
    if (user_access('manage dropcrm_membership')) {
      $ops[] = l(t('Renew'), 'drop/crm/memberships/' . $row->mid . '/renew');
    }

    $rows[] = array(
      'data' => array(
        check_plain($row->member_id),
        l($row->name, 'drop/crm/contact/' . $row->cid),
        check_plain($type_label),
        array('data' => _dropcrm_membership_status_badge($row->status)),
        $end_date ? check_plain($end_date) : t('—'),
        implode(' | ', $ops),
      ),
    );
  }

  $build = array();

  // Status filter pills.
  $build['filters'] = array(
    '#markup' => dropcrm_membership_status_filter_pills(),
  );

  $build['table'] = array(
    '#theme'  => 'table',
    '#header' => $header,
    '#rows'   => $rows,
    '#empty'  => t('No memberships yet. <a href="@url">Add one</a>.', array('@url' => url('drop/crm/memberships/add'))),
  );
  $build['pager'] = array('#theme' => 'pager');

  return $build;
}

/**
 * Renders the status filter pill navigation for the list page.
 */
function dropcrm_membership_status_filter_pills() {
  $current = isset($_GET['status']) ? $_GET['status'] : 'all';
  $pills = array(
    'all'       => t('All'),
    'active'    => t('Active'),
    'grace'     => t('Grace'),
    'lapsed'    => t('Lapsed'),
    'pending'   => t('Pending'),
    'cancelled' => t('Cancelled'),
  );
  $links = array();
  foreach ($pills as $key => $label) {
    $class = $current === $key ? ' class="active"' : '';
    $query = $key === 'all' ? array() : array('status' => $key);
    $links[] = '<li' . $class . '>' . l($label, 'drop/crm/memberships', array('query' => $query)) . '</li>';
  }
  return '<ul class="crm-membership-filter-pills">' . implode('', $links) . '</ul>';
}

/**
 * Membership view page.
 */
function dropcrm_membership_view_page($membership) {
  $build = array();
  $contact = entity_load('crm_contact', $membership->cid);
  $types = _dropcrm_membership_types();
  $type_label = isset($types[$membership->type]['label']) ? $types[$membership->type]['label'] : $membership->type;

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
  foreach ($term_query as $t) {
    $term_rows[] = array(
      $t->start_date,
      !empty($t->end_date) ? $t->end_date : t('lifetime'),
      array('data' => _dropcrm_membership_status_badge($t->status)),
      check_plain($t->notes),
    );
  }
  $build['terms_header'] = array('#markup' => '<h3>' . t('Term history') . '</h3>');
  $build['terms'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Start'), t('End'), t('Status'), t('Notes')),
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

  $form['cid'] = array(
    '#type'              => 'entityreference',
    '#title'             => t('Contact'),
    '#era_entity_type'   => 'crm_contact',
    '#default_value'     => !empty($membership->cid) ? $membership->cid : NULL,
    '#required'          => TRUE,
    '#description'       => t('The CRM contact who is the member.'),
    '#disabled'          => !$is_new,
  );
  // Fall back to a plain autocomplete textfield if the entityreference element
  // isn't available. (In practice it is, since dropcrm depends on the module.)
  if (!module_exists('entityreference')) {
    $form['cid'] = array(
      '#type'              => 'textfield',
      '#title'             => t('Contact'),
      '#autocomplete_path' => 'crm/autocomplete/contacts',
      '#default_value'     => !empty($membership->cid) ? $membership->cid : '',
      '#required'          => TRUE,
      '#disabled'          => !$is_new,
    );
  }

  $form['member_id'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Member ID'),
    '#description'   => t('Optional human-friendly identifier (e.g. M-0042). Must be unique if set.'),
    '#default_value' => $membership->member_id,
    '#maxlength'     => 64,
  );

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
    $form['initial_term']['end_date'] = array(
      '#type'          => 'date',
      '#title'         => t('End date'),
      '#description'   => t('Used only if auto-calculate is off.'),
      '#default_value' => NULL,
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
  // Uniqueness check for member_id.
  if (!empty($v['member_id'])) {
    $query = db_select('crm_membership', 'm')
      ->fields('m', array('mid'))
      ->condition('member_id', $v['member_id']);
    if (!empty($form_state['crm_membership']->mid)) {
      $query->condition('mid', $form_state['crm_membership']->mid, '<>');
    }
    $existing = $query->execute()->fetchField();
    if ($existing) {
      form_set_error('member_id', t('That Member ID is already in use.'));
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

  // Normalize cid (might be entityreference array or plain int).
  $cid = $v['cid'];
  if (is_array($cid)) {
    $cid = isset($cid['target_id']) ? $cid['target_id'] : reset($cid);
  }
  $membership->cid       = (int) $cid;
  $membership->member_id = trim($v['member_id']) !== '' ? trim($v['member_id']) : NULL;
  $membership->type      = $v['type'];
  $membership->status    = (int) $v['status'];

  $membership->save();

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

  $prior_end = $membership->currentEndDate();
  $default_start = ($prior_end && $prior_end > format_date(REQUEST_TIME, 'custom', 'Y-m-d'))
    ? $prior_end
    : format_date(REQUEST_TIME, 'custom', 'Y-m-d');
  $default_end = _dropcrm_membership_add_term_length($default_start, $term_length);

  $form['help'] = array(
    '#markup' => '<p>' . t('Records a new term for this membership. The prior term is preserved in the history.') . '</p>',
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
