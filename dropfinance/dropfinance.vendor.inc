<?php
/**
 * @file
 * Vendor list and form callbacks for Drop Finance.
 */

/**
 * Page callback: vendor list.
 *
 * @return array
 *   A render array.
 */
function dropfinance_vendor_list() {
  backdrop_set_title(t('Vendors'));

  $vendors = db_select('drop_vendor', 'v')
    ->fields('v')
    ->orderBy('v.name', 'ASC')
    ->execute()
    ->fetchAll();

  // Get transaction counts per vendor.
  $txn_counts = db_query("
    SELECT vid, COUNT(*) AS cnt, SUM(amount) AS total
    FROM {drop_transaction}
    WHERE vid IS NOT NULL
    GROUP BY vid
  ")->fetchAllAssoc('vid');

  $rows = array();
  foreach ($vendors as $vendor) {
    $count = isset($txn_counts[$vendor->vid]) ? $txn_counts[$vendor->vid]->cnt : 0;
    $total = isset($txn_counts[$vendor->vid]) ? (float) $txn_counts[$vendor->vid]->total : 0;

    $actions = l(t('Transactions'), 'drop/finance/vendors/' . $vendor->vid)
      . ' &nbsp;·&nbsp; '
      . l(t('Edit'), 'drop/finance/vendors/' . $vendor->vid . '/edit')
      . ' &nbsp;·&nbsp; '
      . l(t('Delete'), 'drop/finance/vendors/' . $vendor->vid . '/delete');

    $rows[] = array(
      check_plain($vendor->name),
      $count,
      '$' . number_format($total, 2),
      check_plain($vendor->notes),
      $actions,
    );
  }

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  $actions = array(
    l(t('+ Add Vendor'), 'drop/finance/vendors/add'),
  );
  $output['actions'] = array(
    '#markup' => drop_base_page_actions($actions),
    '#weight' => -100,
  );

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Name'), t('Transactions'), t('Total'), t('Notes'), t('Actions')),
    '#rows'   => $rows,
    '#empty'  => t('No vendors yet.'),
  );

  return $output;
}

/**
 * Page callback: view a vendor's transactions.
 *
 * @param DropVendor $vendor
 *   The vendor entity.
 *
 * @return array
 *   A render array.
 */
function dropfinance_vendor_view(DropVendor $vendor) {
  backdrop_set_title(check_plain($vendor->name));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Vendors'), 'drop/finance/vendors'),
  ));

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Summary.
  $summary = db_query("
    SELECT type, COUNT(*) AS cnt, SUM(amount) AS total
    FROM {drop_transaction}
    WHERE vid = :vid
    GROUP BY type
  ", array(':vid' => $vendor->vid))->fetchAllAssoc('type');

  $debit_count = isset($summary['debit']) ? $summary['debit']->cnt : 0;
  $debit_total = isset($summary['debit']) ? (float) $summary['debit']->total : 0;
  $credit_count = isset($summary['credit']) ? $summary['credit']->cnt : 0;
  $credit_total = isset($summary['credit']) ? (float) $summary['credit']->total : 0;

  $output['summary'] = array(
    '#markup' => '<div class="dropfinance-import-summary">'
      . t('@debits debits totaling $@debit_total, @credits credits totaling $@credit_total.', array(
          '@debits'       => $debit_count,
          '@debit_total'  => number_format($debit_total, 2),
          '@credits'      => $credit_count,
          '@credit_total' => number_format($credit_total, 2),
        ))
      . '</div>',
    '#weight' => -10,
  );

  // Transaction list.
  $transactions = db_select('drop_transaction', 't')
    ->fields('t')
    ->condition('t.vid', $vendor->vid)
    ->extend('PagerDefault')
    ->limit(50)
    ->orderBy('t.date', 'DESC')
    ->execute()
    ->fetchAll();

  $rows = array();
  foreach ($transactions as $txn) {
    $type_label = $txn->type === 'credit'
      ? '<span class="dropfinance-positive">' . t('Credit') . '</span>'
      : t('Debit');

    $rows[] = array(
      format_date($txn->date, 'custom', 'M j, Y'),
      check_plain(backdrop_substr($txn->description, 0, 60)),
      '$' . number_format($txn->amount, 2),
      $type_label,
      check_plain($txn->category),
      l(t('Edit'), 'drop/finance/transactions/' . $txn->tid . '/edit'),
    );
  }

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Date'), t('Description'), t('Amount'), t('Type'), t('Category'), ''),
    '#rows'   => $rows,
    '#empty'  => t('No transactions for this vendor.'),
    '#weight' => 0,
  );

  $output['pager'] = array(
    '#theme'  => 'pager',
    '#weight' => 10,
  );

  $output['actions'] = array(
    '#markup' => '<p>' . l(t('Edit vendor'), 'drop/finance/vendors/' . $vendor->vid . '/edit') . '</p>',
    '#weight' => 20,
  );

  return $output;
}

/**
 * Form: add or edit a vendor.
 *
 * @param array $form
 * @param array $form_state
 * @param DropVendor|null $vendor
 *   Existing vendor for edit, or NULL for add.
 *
 * @return array
 */
function dropfinance_vendor_form($form, &$form_state, $vendor = NULL) {
  if (empty($vendor)) {
    $vendor = new DropVendor();
  }
  $form['#vendor'] = $vendor;
  $is_edit = !empty($vendor->vid);

  $form['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Breadcrumbs.
  $crumbs = array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Vendors'), 'drop/finance/vendors'),
  );
  if ($is_edit) {
    $crumbs[] = check_plain($vendor->name);
  }
  backdrop_set_breadcrumb($crumbs);

  $form['name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Vendor name'),
    '#default_value' => $vendor->name,
    '#required'      => TRUE,
    '#maxlength'     => 255,
    '#autofocus'     => TRUE,
    '#weight'        => -20,
  );

  $form['notes'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Notes'),
    '#default_value' => $vendor->notes,
    '#rows'          => 3,
    '#weight'        => -10,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Save') : t('Add vendor'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/finance/vendors'),
  );

  return $form;
}

/**
 * Submit handler for dropfinance_vendor_form().
 */
function dropfinance_vendor_form_submit($form, &$form_state) {
  $vendor = $form['#vendor'];
  $vendor->name  = $form_state['values']['name'];
  $vendor->notes = $form_state['values']['notes'];
  $vendor->save();

  backdrop_set_message(t('Vendor %name saved.', array('%name' => $vendor->name)));
  $form_state['redirect'] = 'drop/finance/vendors';
}

/**
 * Form: confirm vendor deletion.
 *
 * @param array $form
 * @param array $form_state
 * @param DropVendor $vendor
 *
 * @return array
 */
function dropfinance_vendor_delete_form($form, &$form_state, DropVendor $vendor) {
  $form['#vendor'] = $vendor;
  return confirm_form(
    $form,
    t('Are you sure you want to delete the vendor %name?', array('%name' => $vendor->name)),
    'drop/finance/vendors',
    t('Any expense lines linked to this vendor will keep their data but lose the vendor link. This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for dropfinance_vendor_delete_form().
 */
function dropfinance_vendor_delete_form_submit($form, &$form_state) {
  $vendor = $form['#vendor'];

  // Clear vendor references on expense lines.
  db_update('drop_expense')
    ->fields(array('vid' => NULL))
    ->condition('vid', $vendor->vid)
    ->execute();

  $vendor->delete();
  backdrop_set_message(t('Vendor %name deleted.', array('%name' => $vendor->name)));
  $form_state['redirect'] = 'drop/finance/vendors';
}

/**
 * Autocomplete callback for vendor names.
 *
 * @param string $string
 *   The search string.
 */
function dropfinance_vendor_autocomplete($string = '') {
  $matches = array();
  if (strlen($string) >= 2) {
    $results = db_select('drop_vendor', 'v')
      ->fields('v', array('vid', 'name'))
      ->condition('v.name', '%' . db_like($string) . '%', 'LIKE')
      ->orderBy('v.name')
      ->range(0, 10)
      ->execute();
    foreach ($results as $row) {
      $matches[$row->name] = check_plain($row->name);
    }
  }
  backdrop_json_output($matches);
}
