<?php
/**
 * @file
 * Transaction list and form callbacks for Drop Finance.
 */

/**
 * Page callback: transaction list with filters.
 *
 * @return array
 *   A render array.
 */
function dropfinance_transaction_list() {
  backdrop_set_title(t('Transactions'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
  ));

  // Get filter values from query string.
  $filter_type = isset($_GET['type']) ? $_GET['type'] : '';
  $filter_category = isset($_GET['category']) ? $_GET['category'] : '';
  $filter_personal = isset($_GET['personal']) ? $_GET['personal'] : '';
  $filter_search = isset($_GET['search']) ? $_GET['search'] : '';

  $query = db_select('drop_transaction', 't')
    ->fields('t')
    ->extend('PagerDefault')
    ->limit(50)
    ->orderBy('t.date', 'DESC')
    ->orderBy('t.tid', 'DESC');

  if ($filter_type !== '') {
    $query->condition('t.type', $filter_type);
  }
  if ($filter_category === '__uncategorized') {
    $query->condition('t.category', '');
  }
  elseif ($filter_category !== '') {
    $query->condition('t.category', $filter_category);
  }
  if ($filter_personal !== '') {
    $query->condition('t.is_personal', (int) $filter_personal);
  }
  if ($filter_search !== '') {
    $query->condition('t.description', '%' . db_like($filter_search) . '%', 'LIKE');
  }

  $transactions = $query->execute()->fetchAll();

  // Build category options for filter.
  $categories = db_select('drop_transaction', 't')
    ->fields('t', array('category'))
    ->groupBy('t.category')
    ->orderBy('t.category')
    ->execute()
    ->fetchCol();

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Filter form.
  $output['filters'] = array(
    '#type' => 'fieldset',
    '#title' => t('Filters'),
    '#collapsible' => TRUE,
    '#collapsed' => empty($filter_type) && empty($filter_category) && empty($filter_personal) && empty($filter_search),
    '#weight' => -10,
  );
  $output['filters']['form'] = backdrop_get_form('dropfinance_transaction_filter_form', $filter_type, $filter_category, $filter_personal, $filter_search, $categories);

  // Summary.
  $total_count = db_select('drop_transaction', 't')
    ->countQuery()
    ->execute()
    ->fetchField();
  $uncategorized = db_select('drop_transaction', 't')
    ->condition('t.category', '')
    ->countQuery()
    ->execute()
    ->fetchField();

  $output['summary'] = array(
    '#markup' => '<p class="dropfinance-transaction-summary">'
      . t('@total transactions total, @uncat uncategorized.', array(
        '@total' => $total_count,
        '@uncat' => $uncategorized,
      ))
      . '</p>',
    '#weight' => -5,
  );

  // Table.
  $rows = array();
  foreach ($transactions as $txn) {
    $type_label = $txn->type === 'credit'
      ? '<span class="dropfinance-positive">' . t('Credit') . '</span>'
      : t('Debit');
    $cat = !empty($txn->category) ? check_plain($txn->category) : '<em>' . t('Uncategorized') . '</em>';
    $vendor = !empty($txn->vendor_name) ? check_plain($txn->vendor_name) : '';
    $personal_flag = !empty($txn->is_personal) ? t('Yes') : '';

    $actions = l(t('Edit'), 'drop/finance/transactions/' . $txn->tid . '/edit')
      . ' &nbsp;·&nbsp; '
      . l(t('Delete'), 'drop/finance/transactions/' . $txn->tid . '/delete');

    $row_class = empty($txn->category) ? 'dropfinance-rule-unmatched' : '';
    $rows[] = array(
      'data' => array(
        format_date($txn->date, 'custom', 'M j, Y'),
        check_plain(backdrop_substr($txn->description, 0, 60)),
        '$' . number_format($txn->amount, 2),
        $type_label,
        $cat,
        $vendor,
        $personal_flag,
        $actions,
      ),
      'class' => $row_class ? array($row_class) : array(),
    );
  }

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => array(
      t('Date'), t('Description'), t('Amount'), t('Type'),
      t('Category'), t('Vendor'), t('Personal'), t('Actions'),
    ),
    '#rows'   => $rows,
    '#empty'  => t('No transactions found.'),
    '#weight' => 0,
  );

  $output['pager'] = array(
    '#theme' => 'pager',
    '#weight' => 10,
  );

  return $output;
}

/**
 * Filter form for the transaction list.
 */
function dropfinance_transaction_filter_form($form, &$form_state, $filter_type = '', $filter_category = '', $filter_personal = '', $filter_search = '', $categories = array()) {
  $form['#method'] = 'get';
  $form['#action'] = url('drop/finance/transactions');
  $form['#token'] = FALSE;

  $form['search'] = array(
    '#type' => 'textfield',
    '#title' => t('Search'),
    '#default_value' => $filter_search,
    '#size' => 30,
    '#attributes' => array('placeholder' => t('Description...')),
  );

  $form['type'] = array(
    '#type' => 'select',
    '#title' => t('Type'),
    '#options' => array('' => t('- All -'), 'debit' => t('Debit'), 'credit' => t('Credit')),
    '#default_value' => $filter_type,
  );

  $cat_options = array('' => t('- All -'));
  $has_uncategorized = FALSE;
  foreach ($categories as $cat) {
    if ($cat !== '') {
      $cat_options[$cat] = $cat;
    }
    else {
      $has_uncategorized = TRUE;
    }
  }
  if ($has_uncategorized) {
    $cat_options['__uncategorized'] = t('Uncategorized');
  }
  $form['category'] = array(
    '#type' => 'select',
    '#title' => t('Category'),
    '#options' => $cat_options,
    '#default_value' => $filter_category,
  );

  $form['personal'] = array(
    '#type' => 'select',
    '#title' => t('Personal'),
    '#options' => array('' => t('- All -'), '1' => t('Personal'), '0' => t('Business')),
    '#default_value' => $filter_personal,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Filter'),
  );
  $form['actions']['reset'] = array(
    '#markup' => l(t('Reset'), 'drop/finance/transactions'),
  );

  return $form;
}

/**
 * Form: edit a transaction.
 *
 * @param array $form
 * @param array $form_state
 * @param DropTransaction $transaction
 *
 * @return array
 */
function dropfinance_transaction_form($form, &$form_state, DropTransaction $transaction) {
  $form['#transaction'] = $transaction;

  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Transactions'), 'drop/finance/transactions'),
  ));

  $form['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Read-only info.
  $form['info'] = array(
    '#markup' => '<div class="dropfinance-txn-info">'
      . '<strong>' . t('Date:') . '</strong> ' . format_date($transaction->date, 'custom', 'M j, Y') . '<br>'
      . '<strong>' . t('Description:') . '</strong> ' . check_plain($transaction->description) . '<br>'
      . '<strong>' . t('Amount:') . '</strong> $' . number_format($transaction->amount, 2) . '<br>'
      . '<strong>' . t('Type:') . '</strong> ' . check_plain($transaction->type)
      . '</div>',
    '#weight' => -20,
  );

  $form['vendor_name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Vendor name'),
    '#default_value' => $transaction->vendor_name,
    '#maxlength'     => 255,
    '#weight'        => -10,
  );

  // Build category options from existing categories.
  $existing_cats = db_query("SELECT DISTINCT category FROM {drop_transaction} WHERE category != '' ORDER BY category")->fetchCol();
  $cat_options = array('' => t('— None —'));
  foreach ($existing_cats as $cat) {
    $cat_options[$cat] = $cat;
  }
  $cat_options['__other'] = t('— Other (new category) —');

  $form['category'] = array(
    '#type'          => 'select',
    '#title'         => t('Category'),
    '#options'       => $cat_options,
    '#default_value' => $transaction->category,
    '#weight'        => -5,
  );

  $form['category_new'] = array(
    '#type'          => 'textfield',
    '#title'         => t('New category name'),
    '#maxlength'     => 100,
    '#weight'        => -4,
    '#states' => array(
      'visible' => array(
        ':input[name="category"]' => array('value' => '__other'),
      ),
    ),
  );

  $form['is_personal'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Personal expense (not business)'),
    '#default_value' => $transaction->is_personal,
    '#weight'        => 0,
  );

  $form['notes'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Notes'),
    '#default_value' => $transaction->notes,
    '#rows'          => 3,
    '#weight'        => 10,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/finance/transactions'),
  );

  return $form;
}

/**
 * Submit handler for dropfinance_transaction_form().
 */
function dropfinance_transaction_form_submit($form, &$form_state) {
  $transaction = $form['#transaction'];
  $v = $form_state['values'];

  $transaction->vendor_name = $v['vendor_name'];
  $transaction->category    = $v['category'] === '__other' ? trim($v['category_new']) : $v['category'];
  $transaction->is_personal = (int) $v['is_personal'];
  $transaction->notes       = $v['notes'];
  $transaction->save();

  backdrop_set_message(t('Transaction updated.'));
  $form_state['redirect'] = 'drop/finance/transactions';
}

/**
 * Form: confirm transaction deletion.
 *
 * @param array $form
 * @param array $form_state
 * @param DropTransaction $transaction
 *
 * @return array
 */
function dropfinance_transaction_delete_form($form, &$form_state, DropTransaction $transaction) {
  $form['#transaction'] = $transaction;
  return confirm_form(
    $form,
    t('Are you sure you want to delete this transaction from @date?', array(
      '@date' => format_date($transaction->date, 'custom', 'M j, Y'),
    )),
    'drop/finance/transactions',
    t('Description: %desc — $@amount', array(
      '%desc' => $transaction->description,
      '@amount' => number_format($transaction->amount, 2),
    )),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for dropfinance_transaction_delete_form().
 */
function dropfinance_transaction_delete_form_submit($form, &$form_state) {
  $transaction = $form['#transaction'];
  $transaction->delete();
  backdrop_set_message(t('Transaction deleted.'));
  $form_state['redirect'] = 'drop/finance/transactions';
}
