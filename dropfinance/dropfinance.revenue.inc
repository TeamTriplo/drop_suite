<?php
/**
 * @file
 * Revenue list and form callbacks for Drop Finance.
 */

/**
 * Page callback: revenue list.
 *
 * @return array
 *   A render array.
 */
function dropfinance_revenue_list() {
  backdrop_set_title(t('Revenue'));

  $revenues = db_select('drop_revenue', 'r')
    ->fields('r')
    ->orderBy('r.active', 'DESC')
    ->orderBy('r.label', 'ASC')
    ->execute()
    ->fetchAll();

  // Load client names.
  $client_names = _dropfinance_client_options();

  // Load pipeline totals per revenue line.
  $pipeline_totals = array();
  if (db_table_exists('drop_revenue_pipeline')) {
    $result = db_query("SELECT rid, COUNT(*) as cnt, SUM(amount) as total FROM {drop_revenue_pipeline} GROUP BY rid");
    foreach ($result as $row) {
      $pipeline_totals[$row->rid] = $row;
    }
  }

  $rows = array();
  foreach ($revenues as $rev) {
    $client = !empty($rev->cid) && isset($client_names[$rev->cid])
      ? $client_names[$rev->cid] : '—';
    $freq_options = dropfinance_frequency_options();
    $freq = isset($freq_options[$rev->frequency]) ? $freq_options[$rev->frequency] : $rev->frequency;

    // Pipeline indicator.
    $pipeline_label = '';
    if (isset($pipeline_totals[$rev->rid])) {
      $pt = $pipeline_totals[$rev->rid];
      $pipeline_label = '<br><small>' . t('+@count projects ($@total)', array(
        '@count' => $pt->cnt,
        '@total' => number_format($pt->total, 2),
      )) . '</small>';
    }

    $actions = l(t('Edit'), 'drop/finance/revenue/' . $rev->rid . '/edit')
      . ' &nbsp;·&nbsp; '
      . l(t('Delete'), 'drop/finance/revenue/' . $rev->rid . '/delete');

    $row_class = $rev->active ? '' : 'dropfinance-inactive';
    $rows[] = array(
      'data' => array(
        check_plain($rev->label),
        check_plain($client),
        '$' . number_format($rev->amount, 2) . $pipeline_label,
        $freq,
        $rev->active ? t('Active') : t('Paused'),
        $actions,
      ),
      'class' => $row_class ? array($row_class) : array(),
    );
  }

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  $actions = array(
    l(t('+ Add Revenue'), 'drop/finance/revenue/add'),
  );
  $output['actions'] = array(
    '#markup' => drop_base_page_actions($actions),
    '#weight' => -100,
  );

  $output['help'] = array(
    '#markup' => '<div class="dropfinance-help-text">'
      . t('Revenue lines represent expected recurring income. Active lines are included in the dashboard projection. Amounts were initially calculated as 12-month averages of paid FreshBooks invoices per client.')
      . ' ' . l(t('View Client Revenue Report'), 'drop/finance/revenue/clients') . '.'
      . '</div>',
    '#weight' => -10,
  );

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Label'), t('Client'), t('Amount'), t('Frequency'), t('Status'), t('Actions')),
    '#rows'   => $rows,
    '#empty'  => t('No revenue lines yet.'),
  );

  // Show clients without revenue lines.
  $all_clients = _dropfinance_client_options();
  $used_cids = db_query("SELECT DISTINCT cid FROM {drop_revenue} WHERE cid IS NOT NULL")->fetchCol();
  $unused_clients = array_diff_key($all_clients, array_flip($used_cids));
  if (!empty($unused_clients)) {
    $output['unused'] = array(
      '#markup' => '<details class="dropfinance-unused-clients"><summary>'
        . t('@count clients without revenue lines', array('@count' => count($unused_clients)))
        . '</summary><p>' . check_plain(implode(', ', $unused_clients)) . '</p></details>',
      '#weight' => 10,
    );
  }

  return $output;
}

/**
 * Form: add or edit a revenue line.
 *
 * @param array $form
 * @param array $form_state
 * @param DropRevenue|null $revenue
 *   Existing revenue line for edit, or NULL for add.
 *
 * @return array
 */
function dropfinance_revenue_form($form, &$form_state, $revenue = NULL) {
  if (empty($revenue)) {
    $revenue = new DropRevenue();
    $revenue->start_date = REQUEST_TIME;
  }
  $form['#revenue'] = $revenue;
  $is_edit = !empty($revenue->rid);

  $form['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Breadcrumbs.
  $crumbs = array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Revenue'), 'drop/finance/revenue'),
  );
  if ($is_edit) {
    $crumbs[] = check_plain($revenue->label);
  }
  backdrop_set_breadcrumb($crumbs);

  $form['label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Label'),
    '#description'   => t('Describe this revenue source (e.g. "ACME quarterly support").'),
    '#default_value' => $revenue->label,
    '#required'      => TRUE,
    '#maxlength'     => 255,
    '#autofocus'     => TRUE,
    '#weight'        => -20,
  );

  $form['amount'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Amount ($)'),
    '#description'   => t('Dollar amount per occurrence.'),
    '#default_value' => $is_edit ? number_format($revenue->amount, 2, '.', '') : '',
    '#required'      => TRUE,
    '#size'          => 12,
    '#weight'        => -15,
  );

  $form['frequency'] = array(
    '#type'          => 'select',
    '#title'         => t('Frequency'),
    '#options'       => dropfinance_frequency_options(),
    '#default_value' => $revenue->frequency,
    '#weight'        => -10,
  );

  // Client select — exclude clients that already have revenue lines (on add).
  $all_clients = _dropfinance_client_options();
  if (!$is_edit) {
    $used_cids = db_query("SELECT DISTINCT cid FROM {drop_revenue} WHERE cid IS NOT NULL")->fetchCol();
    foreach ($used_cids as $used_cid) {
      unset($all_clients[$used_cid]);
    }
  }
  $client_options = array('' => t('— None —')) + $all_clients;
  $form['cid'] = array(
    '#type'          => 'select',
    '#title'         => t('Client'),
    '#options'       => $client_options,
    '#default_value' => $revenue->cid ? $revenue->cid : '',
    '#weight'        => -5,
  );

  // Project select.
  $project_options = array('' => t('— None —')) + _dropfinance_project_options();
  $form['pid'] = array(
    '#type'          => 'select',
    '#title'         => t('Project'),
    '#options'       => $project_options,
    '#default_value' => $revenue->pid ? $revenue->pid : '',
    '#weight'        => -4,
  );

  // Start date.
  $start_ts = !empty($revenue->start_date) ? $revenue->start_date : REQUEST_TIME;
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
  $has_end = !empty($revenue->end_date);
  $end_ts = $has_end ? $revenue->end_date : REQUEST_TIME;
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
    '#default_value' => $revenue->notes,
    '#rows'          => 3,
    '#weight'        => 10,
  );

  // Pipeline projects.
  $pipeline_items = array();
  if ($is_edit && db_table_exists('drop_revenue_pipeline')) {
    $pipeline_items = db_select('drop_revenue_pipeline', 'p')
      ->fields('p')
      ->condition('p.rid', $revenue->rid)
      ->orderBy('p.month')
      ->execute()
      ->fetchAll();
  }

  $pipeline_count = isset($form_state['pipeline_count'])
    ? $form_state['pipeline_count']
    : max(count($pipeline_items), 1);
  $form_state['pipeline_count'] = $pipeline_count;

  $form['pipeline'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Upcoming Projects'),
    '#description' => t('One-off project revenue expected from this client. These amounts are added to projections in the specified month.'),
    '#collapsible' => TRUE,
    '#collapsed'   => empty($pipeline_items),
    '#weight'      => 15,
    '#tree'        => TRUE,
    '#prefix'      => '<div id="pipeline-wrapper">',
    '#suffix'      => '</div>',
  );

  // Generate month options: current month + next 23 months.
  $month_options = array('' => t('— Select —'));
  for ($i = 0; $i < 24; $i++) {
    $ts = strtotime("+{$i} months", REQUEST_TIME);
    $key = date('Y-m', $ts);
    $month_options[$key] = date('F Y', $ts);
  }

  // Table header row.
  $form['pipeline']['header'] = array(
    '#markup' => '<div class="pipeline-row pipeline-header">'
      . '<div class="pipeline-col-month">' . t('Month') . '</div>'
      . '<div class="pipeline-col-amount">' . t('Amount ($)') . '</div>'
      . '<div class="pipeline-col-desc">' . t('Description') . '</div>'
      . '</div>',
    '#weight' => -1,
  );

  for ($delta = 0; $delta < $pipeline_count; $delta++) {
    $item = isset($pipeline_items[$delta]) ? $pipeline_items[$delta] : NULL;
    $form['pipeline'][$delta]['#prefix'] = '<div class="pipeline-row">';
    $form['pipeline'][$delta]['#suffix'] = '</div>';
    $form['pipeline'][$delta]['month'] = array(
      '#type'          => 'select',
      '#title'         => t('Month'),
      '#title_display' => 'invisible',
      '#options'       => $month_options,
      '#default_value' => $item ? $item->month : '',
    );
    $form['pipeline'][$delta]['amount'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Amount ($)'),
      '#title_display' => 'invisible',
      '#default_value' => $item ? number_format($item->amount, 2, '.', '') : '',
      '#size'          => 10,
    );
    $form['pipeline'][$delta]['description'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Description'),
      '#title_display' => 'invisible',
      '#default_value' => $item ? $item->description : '',
      '#maxlength'     => 255,
      '#size'          => 40,
    );
  }

  $form['pipeline']['add_more'] = array(
    '#type'   => 'submit',
    '#value'  => t('Add another project'),
    '#submit' => array('dropfinance_revenue_pipeline_add_more'),
    '#ajax'   => array(
      'callback' => 'dropfinance_revenue_pipeline_ajax',
      'wrapper'  => 'pipeline-wrapper',
    ),
    '#limit_validation_errors' => array(),
    '#weight' => 100,
  );

  $form['active'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Include in projections'),
    '#default_value' => isset($revenue->rid) ? $revenue->active : 1,
    '#weight'        => 50,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Save') : t('Add revenue'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/finance/revenue'),
  );

  return $form;
}

/**
 * AJAX callback for pipeline "Add another project" button.
 */
function dropfinance_revenue_pipeline_ajax($form, $form_state) {
  return $form['pipeline'];
}

/**
 * Submit handler for pipeline "Add another project" button.
 */
function dropfinance_revenue_pipeline_add_more($form, &$form_state) {
  $form_state['pipeline_count']++;
  $form_state['rebuild'] = TRUE;
}

/**
 * Validation handler for dropfinance_revenue_form().
 */
function dropfinance_revenue_form_validate($form, &$form_state) {
  $amount = $form_state['values']['amount'];
  if (!is_numeric($amount) || $amount < 0) {
    form_set_error('amount', t('Amount must be a positive number.'));
  }
}

/**
 * Submit handler for dropfinance_revenue_form().
 */
function dropfinance_revenue_form_submit($form, &$form_state) {
  $revenue = $form['#revenue'];
  $v = $form_state['values'];

  $revenue->label     = $v['label'];
  $revenue->amount    = (float) $v['amount'];
  $revenue->frequency = $v['frequency'];
  $revenue->cid       = !empty($v['cid']) ? (int) $v['cid'] : NULL;
  $revenue->pid       = !empty($v['pid']) ? (int) $v['pid'] : NULL;
  $revenue->notes     = $v['notes'];
  $revenue->active    = (int) $v['active'];

  // Start date.
  $sd = $v['start_date'];
  $revenue->start_date = strtotime(sprintf('%04d-%02d-%02d', $sd['year'], $sd['month'], $sd['day']));

  // End date.
  if (!empty($v['has_end_date'])) {
    $ed = $v['end_date'];
    $revenue->end_date = strtotime(sprintf('%04d-%02d-%02d', $ed['year'], $ed['month'], $ed['day']));
  }
  else {
    $revenue->end_date = NULL;
  }

  $revenue->save();

  // Save pipeline items.
  if (db_table_exists('drop_revenue_pipeline')) {
    // Clear existing pipeline items for this revenue line.
    db_delete('drop_revenue_pipeline')
      ->condition('rid', $revenue->rid)
      ->execute();

    // Insert new items.
    if (!empty($v['pipeline'])) {
      foreach ($v['pipeline'] as $delta => $item) {
        if (!is_array($item) || empty($item['month']) || empty($item['amount'])) {
          continue;
        }
        $amount = (float) $item['amount'];
        if ($amount <= 0) {
          continue;
        }
        db_insert('drop_revenue_pipeline')
          ->fields(array(
            'rid'         => $revenue->rid,
            'month'       => $item['month'],
            'amount'      => $amount,
            'description' => trim($item['description']),
          ))
          ->execute();
      }
    }
  }

  backdrop_set_message(t('Revenue line %label saved.', array('%label' => $revenue->label)));
  $form_state['redirect'] = 'drop/finance/revenue';
}

/**
 * Form: confirm revenue line deletion.
 *
 * @param array $form
 * @param array $form_state
 * @param DropRevenue $revenue
 *
 * @return array
 */
function dropfinance_revenue_delete_form($form, &$form_state, DropRevenue $revenue) {
  $form['#revenue'] = $revenue;
  return confirm_form(
    $form,
    t('Are you sure you want to delete the revenue line %label?', array('%label' => $revenue->label)),
    'drop/finance/revenue',
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for dropfinance_revenue_delete_form().
 */
function dropfinance_revenue_delete_form_submit($form, &$form_state) {
  $revenue = $form['#revenue'];
  // Clean up pipeline items.
  if (db_table_exists('drop_revenue_pipeline')) {
    db_delete('drop_revenue_pipeline')
      ->condition('rid', $revenue->rid)
      ->execute();
  }
  $revenue->delete();
  backdrop_set_message(t('Revenue line %label deleted.', array('%label' => $revenue->label)));
  $form_state['redirect'] = 'drop/finance/revenue';
}

/**
 * Returns an options array of CRM organizations keyed by cid.
 *
 * @return array
 *   Associative array of cid => name.
 */
function _dropfinance_client_options() {
  $options = array();
  if (db_table_exists('crm_contact')) {
    $results = db_select('crm_contact', 'c')
      ->fields('c', array('cid', 'name'))
      ->condition('c.type', 'crm_organization')
      ->orderBy('c.name')
      ->execute();
    foreach ($results as $row) {
      $options[$row->cid] = $row->name;
    }
  }
  return $options;
}

/**
 * Returns an options array of projects keyed by pid.
 *
 * @return array
 *   Associative array of pid => name.
 */
function _dropfinance_project_options() {
  $options = array();
  if (db_table_exists('drop_project')) {
    $results = db_select('drop_project', 'p')
      ->fields('p', array('pid', 'name'))
      ->orderBy('p.name')
      ->execute();
    foreach ($results as $row) {
      $options[$row->pid] = $row->name;
    }
  }
  return $options;
}
