<?php
/**
 * @file
 * Expense list and form callbacks for Drop Finance.
 */

/**
 * Page callback: expense list.
 *
 * @return array
 *   A render array.
 */
function dropfinance_expense_list() {
  backdrop_set_title(t('Expenses'));

  $expenses = db_select('drop_expense', 'e')
    ->fields('e')
    ->orderBy('e.active', 'DESC')
    ->orderBy('e.label', 'ASC')
    ->execute()
    ->fetchAll();

  // Load vendor names.
  $vendor_names = _dropfinance_vendor_options();

  $rows = array();
  foreach ($expenses as $exp) {
    // Show vendor name or category.
    $source = '';
    if (!empty($exp->vid) && isset($vendor_names[$exp->vid])) {
      $source = $vendor_names[$exp->vid];
    }
    elseif (!empty($exp->category)) {
      $source = $exp->category;
    }
    else {
      $source = '—';
    }

    $freq_options = dropfinance_frequency_options();
    $freq = isset($freq_options[$exp->frequency]) ? $freq_options[$exp->frequency] : $exp->frequency;

    $actions = l(t('Edit'), 'drop/finance/expenses/' . $exp->eid . '/edit')
      . ' &nbsp;·&nbsp; '
      . l(t('Delete'), 'drop/finance/expenses/' . $exp->eid . '/delete');

    $row_class = $exp->active ? '' : 'dropfinance-inactive';
    $rows[] = array(
      'data' => array(
        check_plain($exp->label),
        check_plain($source),
        '$' . number_format($exp->amount, 2),
        $freq,
        $exp->active ? t('Active') : t('Paused'),
        $actions,
      ),
      'class' => $row_class ? array($row_class) : array(),
    );
  }

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  $actions = array(
    l(t('+ Add Expense'), 'drop/finance/expenses/add'),
  );
  $output['actions'] = array(
    '#markup' => drop_base_page_actions($actions),
    '#weight' => -100,
  );

  $output['help'] = array(
    '#markup' => '<div class="dropfinance-help-text">'
      . t('Expense lines represent expected recurring costs. Active lines are included in the dashboard projection. Amounts were initially calculated as 6-month averages of recent bank transactions per vendor/category.')
      . '</div>',
    '#weight' => -10,
  );

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Label'), t('Vendor / Category'), t('Amount'), t('Frequency'), t('Status'), t('Actions')),
    '#rows'   => $rows,
    '#empty'  => t('No expense lines yet.'),
  );

  return $output;
}

/**
 * Form: add or edit an expense line.
 *
 * @param array $form
 * @param array $form_state
 * @param DropExpense|null $expense
 *   Existing expense line for edit, or NULL for add.
 *
 * @return array
 */
function dropfinance_expense_form($form, &$form_state, $expense = NULL) {
  if (empty($expense)) {
    $expense = new DropExpense();
    $expense->start_date = REQUEST_TIME;
  }
  $form['#expense'] = $expense;
  $is_edit = !empty($expense->eid);

  $form['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Breadcrumbs.
  $crumbs = array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Expenses'), 'drop/finance/expenses'),
  );
  if ($is_edit) {
    $crumbs[] = check_plain($expense->label);
  }
  backdrop_set_breadcrumb($crumbs);

  $form['label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Label'),
    '#description'   => t('Describe this expense (e.g. "Linode servers", "Insurance").'),
    '#default_value' => $expense->label,
    '#required'      => TRUE,
    '#maxlength'     => 255,
    '#autofocus'     => TRUE,
    '#weight'        => -20,
  );

  $form['amount'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Amount ($)'),
    '#description'   => t('Dollar amount per occurrence.'),
    '#default_value' => $is_edit ? number_format($expense->amount, 2, '.', '') : '',
    '#required'      => TRUE,
    '#size'          => 12,
    '#weight'        => -15,
  );

  $form['frequency'] = array(
    '#type'          => 'select',
    '#title'         => t('Frequency'),
    '#options'       => dropfinance_frequency_options(),
    '#default_value' => $expense->frequency,
    '#weight'        => -10,
  );

  // Vendor select.
  $vendor_options = array('' => t('— None —')) + _dropfinance_vendor_options();
  $form['vid'] = array(
    '#type'          => 'select',
    '#title'         => t('Vendor'),
    '#options'       => $vendor_options,
    '#default_value' => $expense->vid ? $expense->vid : '',
    '#description'   => t('Select a vendor, or leave blank and enter a category below.'),
    '#weight'        => -5,
  );

  $existing_cats = db_query("SELECT DISTINCT category FROM {drop_transaction} WHERE category != '' ORDER BY category")->fetchCol();
  $cat_options_list = array('' => t('— None —'));
  foreach ($existing_cats as $cat) {
    $cat_options_list[$cat] = $cat;
  }
  $cat_options_list['__other'] = t('— Other (new category) —');
  $form['category'] = array(
    '#type'          => 'select',
    '#title'         => t('Category'),
    '#description'   => t('Used when no vendor is selected.'),
    '#options'       => $cat_options_list,
    '#default_value' => $expense->category,
    '#weight'        => -4,
  );
  $form['category_new'] = array(
    '#type'          => 'textfield',
    '#title'         => t('New category name'),
    '#maxlength'     => 100,
    '#weight'        => -3,
    '#states' => array(
      'visible' => array(
        ':input[name="category"]' => array('value' => '__other'),
      ),
    ),
  );

  // Start date.
  $start_ts = !empty($expense->start_date) ? $expense->start_date : REQUEST_TIME;
  $form['start_date'] = array(
    '#type'          => 'date',
    '#title'         => t('Start date'),
    '#default_value' => array(
      'year'  => (int) date('Y', $start_ts),
      'month' => (int) date('n', $start_ts),
      'day'   => (int) date('j', $start_ts),
    ),
    '#required'      => TRUE,
    '#weight'        => 0,
  );

  // End date.
  $has_end = !empty($expense->end_date);
  $end_ts = $has_end ? $expense->end_date : REQUEST_TIME;
  $form['has_end_date'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Has end date'),
    '#default_value' => $has_end ? 1 : 0,
    '#weight'        => 5,
  );
  $form['end_date'] = array(
    '#type'          => 'date',
    '#title'         => t('End date'),
    '#default_value' => array(
      'year'  => (int) date('Y', $end_ts),
      'month' => (int) date('n', $end_ts),
      'day'   => (int) date('j', $end_ts),
    ),
    '#states' => array(
      'visible' => array(
        ':input[name="has_end_date"]' => array('checked' => TRUE),
      ),
    ),
    '#weight'        => 6,
  );

  $form['notes'] = array(
    '#type'          => 'textarea',
    '#title'         => t('Notes'),
    '#default_value' => $expense->notes,
    '#rows'          => 3,
    '#weight'        => 10,
  );

  $form['active'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Include in projections'),
    '#default_value' => isset($expense->eid) ? $expense->active : 1,
    '#weight'        => 50,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Save') : t('Add expense'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/finance/expenses'),
  );

  return $form;
}

/**
 * Validation handler for dropfinance_expense_form().
 */
function dropfinance_expense_form_validate($form, &$form_state) {
  $amount = $form_state['values']['amount'];
  if (!is_numeric($amount) || $amount < 0) {
    form_set_error('amount', t('Amount must be a positive number.'));
  }
}

/**
 * Submit handler for dropfinance_expense_form().
 */
function dropfinance_expense_form_submit($form, &$form_state) {
  $expense = $form['#expense'];
  $v = $form_state['values'];

  $expense->label     = $v['label'];
  $expense->amount    = (float) $v['amount'];
  $expense->frequency = $v['frequency'];
  $expense->vid       = !empty($v['vid']) ? (int) $v['vid'] : NULL;
  $expense->category  = $v['category'] === '__other' ? trim($v['category_new']) : $v['category'];
  $expense->notes     = $v['notes'];
  $expense->active    = (int) $v['active'];

  // Start date.
  $sd = $v['start_date'];
  $expense->start_date = strtotime(sprintf('%04d-%02d-%02d', $sd['year'], $sd['month'], $sd['day']));

  // End date.
  if (!empty($v['has_end_date'])) {
    $ed = $v['end_date'];
    $expense->end_date = strtotime(sprintf('%04d-%02d-%02d', $ed['year'], $ed['month'], $ed['day']));
  }
  else {
    $expense->end_date = NULL;
  }

  $expense->save();

  backdrop_set_message(t('Expense line %label saved.', array('%label' => $expense->label)));
  $form_state['redirect'] = 'drop/finance/expenses';
}

/**
 * Form: confirm expense line deletion.
 *
 * @param array $form
 * @param array $form_state
 * @param DropExpense $expense
 *
 * @return array
 */
function dropfinance_expense_delete_form($form, &$form_state, DropExpense $expense) {
  $form['#expense'] = $expense;
  return confirm_form(
    $form,
    t('Are you sure you want to delete the expense line %label?', array('%label' => $expense->label)),
    'drop/finance/expenses',
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for dropfinance_expense_delete_form().
 */
function dropfinance_expense_delete_form_submit($form, &$form_state) {
  $expense = $form['#expense'];
  $expense->delete();
  backdrop_set_message(t('Expense line %label deleted.', array('%label' => $expense->label)));
  $form_state['redirect'] = 'drop/finance/expenses';
}

/**
 * Returns an options array of vendors keyed by vid.
 *
 * @return array
 *   Associative array of vid => name.
 */
function _dropfinance_vendor_options() {
  $options = array();
  $results = db_select('drop_vendor', 'v')
    ->fields('v', array('vid', 'name'))
    ->orderBy('v.name')
    ->execute();
  foreach ($results as $row) {
    $options[$row->vid] = $row->name;
  }
  return $options;
}
