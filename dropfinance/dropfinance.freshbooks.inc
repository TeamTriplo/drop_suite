<?php
/**
 * @file
 * FreshBooks invoice import and client revenue report for Drop Finance.
 */

/**
 * Form: FreshBooks CSV import.
 */
function dropfinance_freshbooks_import_form($form, &$form_state) {
  backdrop_set_title(t('Import FreshBooks Invoices'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Import'), 'drop/finance/import'),
  ));

  $form['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Step 2: Preview.
  if (!empty($form_state['storage']['parsed'])) {
    return _dropfinance_freshbooks_preview_form($form, $form_state);
  }

  // Show current state.
  $existing = db_select('drop_invoice', 'i')
    ->countQuery()
    ->execute()
    ->fetchField();
  if ($existing > 0) {
    $form['notice'] = array(
      '#markup' => '<p>' . t('@count invoices already imported. Importing again will update existing records and add new ones.', array('@count' => $existing)) . '</p>',
    );
  }

  $form['info'] = array(
    '#markup' => '<p>' . t('Upload a FreshBooks invoice export CSV. Expected columns: Client Name, Invoice #, Date Issued, Date Due, Invoice Status, Date Paid, Item Name, Item Description, Rate, Quantity, Discount Percentage, Line Subtotal, Tax 1 Type, Tax 1 Amount, Tax 2 Type, Tax 2 Amount, Line Total, Currency.') . '</p>',
  );

  $form['csv_file'] = array(
    '#type'              => 'managed_file',
    '#title'             => t('FreshBooks CSV file'),
    '#upload_location'   => 'temporary://',
    '#upload_validators' => array(
      'file_validate_extensions' => array('csv'),
      'file_validate_size'       => array(5 * 1024 * 1024),
    ),
    '#required'          => TRUE,
  );

  $form['create_revenue'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Create revenue lines for active clients'),
    '#description'   => t('Automatically create monthly revenue projection lines based on recent invoicing patterns. Only clients with paid invoices in the last 12 months will be included.'),
    '#default_value' => 1,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Upload and preview'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/finance/import'),
  );

  return $form;
}

/**
 * Validate handler: parse the FreshBooks CSV.
 */
function dropfinance_freshbooks_import_form_validate($form, &$form_state) {
  if (!empty($form_state['storage']['parsed'])) {
    return;
  }

  $fid = $form_state['values']['csv_file'];
  if (empty($fid)) {
    return;
  }

  $file = file_load($fid);
  if (!$file) {
    form_set_error('csv_file', t('Could not load the uploaded file.'));
    return;
  }

  $path = backdrop_realpath($file->uri);
  $parsed = _dropfinance_parse_freshbooks_csv($path);

  if (empty($parsed)) {
    form_set_error('csv_file', t('No valid invoices found in the CSV file.'));
    return;
  }

  // Match client names to CRM contacts.
  $crm_map = _dropfinance_build_crm_client_map();
  foreach ($parsed as $inv_num => &$inv) {
    $name = $inv['client_name'];
    $lower = strtolower($name);
    if (isset($crm_map[$name])) {
      $inv['cid'] = $crm_map[$name];
    }
    elseif (isset($crm_map[$lower])) {
      $inv['cid'] = $crm_map[$lower];
    }
  }
  unset($inv);

  // Check for existing invoices.
  $existing_nums = db_select('drop_invoice', 'i')
    ->fields('i', array('invoice_number'))
    ->condition('i.invoice_number', array_keys($parsed), 'IN')
    ->execute()
    ->fetchCol();

  $new_count = 0;
  $update_count = 0;
  foreach ($parsed as $inv_num => $inv) {
    if (in_array($inv_num, $existing_nums)) {
      $update_count++;
    }
    else {
      $new_count++;
    }
  }

  $form_state['storage']['parsed'] = $parsed;
  $form_state['storage']['new_count'] = $new_count;
  $form_state['storage']['update_count'] = $update_count;
  $form_state['storage']['crm_map'] = $crm_map;
  $form_state['storage']['create_revenue'] = $form_state['values']['create_revenue'];
  $form_state['rebuild'] = TRUE;
}

/**
 * Build the preview step of the FreshBooks import.
 */
function _dropfinance_freshbooks_preview_form(&$form, &$form_state) {
  $parsed = $form_state['storage']['parsed'];
  $new_count = $form_state['storage']['new_count'];
  $update_count = $form_state['storage']['update_count'];

  // Summary by status.
  $status_counts = array();
  $total_amount = 0;
  foreach ($parsed as $inv) {
    $s = $inv['status'];
    if (!isset($status_counts[$s])) {
      $status_counts[$s] = 0;
    }
    $status_counts[$s]++;
    $total_amount += $inv['amount'];
  }

  $status_parts = array();
  foreach ($status_counts as $s => $c) {
    $status_parts[] = $c . ' ' . $s;
  }

  $form['summary'] = array(
    '#markup' => '<div class="dropfinance-import-summary">'
      . '<strong>' . t('FreshBooks import preview') . '</strong><br>'
      . t('@total invoices found (@new new, @update existing). Total: $@amount.', array(
          '@total'  => count($parsed),
          '@new'    => $new_count,
          '@update' => $update_count,
          '@amount' => number_format($total_amount, 2),
        )) . '<br>'
      . t('Statuses: @statuses', array('@statuses' => implode(', ', $status_parts)))
      . '</div>',
    '#weight' => -10,
  );

  // Unique clients.
  $clients = array();
  foreach ($parsed as $inv) {
    if (!isset($clients[$inv['client_name']])) {
      $clients[$inv['client_name']] = array(
        'count' => 0,
        'total' => 0,
        'cid'   => $inv['cid'],
      );
    }
    $clients[$inv['client_name']]['count']++;
    $clients[$inv['client_name']]['total'] += $inv['amount'];
  }
  ksort($clients);

  $client_rows = array();
  foreach ($clients as $name => $data) {
    $linked = $data['cid'] ? t('Yes') : '';
    $client_rows[] = array(
      check_plain($name),
      $data['count'],
      '$' . number_format($data['total'], 2),
      $linked,
    );
  }

  $form['clients'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Client'), t('Invoices'), t('Total'), t('CRM Linked')),
    '#rows'   => $client_rows,
    '#weight' => -5,
  );

  if (!empty($form_state['storage']['create_revenue'])) {
    $form['revenue_note'] = array(
      '#markup' => '<p><em>' . t('Revenue lines will be created for active clients (paid invoices in the last 12 months).') . '</em></p>',
      '#weight' => -4,
    );
  }

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Import @count invoices', array('@count' => count($parsed))),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/finance/import'),
  );

  return $form;
}

/**
 * Submit handler: import FreshBooks invoices and create revenue lines.
 */
function dropfinance_freshbooks_import_form_submit($form, &$form_state) {
  if (empty($form_state['storage']['parsed'])) {
    return;
  }

  $parsed = $form_state['storage']['parsed'];
  $create_revenue = !empty($form_state['storage']['create_revenue']);
  global $user;

  $new = 0;
  $updated = 0;

  foreach ($parsed as $inv_num => $inv) {
    $existing = db_select('drop_invoice', 'i')
      ->fields('i', array('iid'))
      ->condition('i.invoice_number', $inv_num)
      ->execute()
      ->fetchField();

    $fields = array(
      'invoice_number' => $inv_num,
      'client_name'    => $inv['client_name'],
      'cid'            => $inv['cid'],
      'date_issued'    => $inv['date_issued'],
      'date_paid'      => $inv['date_paid'],
      'status'         => $inv['status'],
      'amount'         => $inv['amount'],
      'currency'       => $inv['currency'],
    );

    if ($existing) {
      db_update('drop_invoice')
        ->fields($fields)
        ->condition('iid', $existing)
        ->execute();
      $updated++;
    }
    else {
      $fields['uid'] = $user->uid;
      $fields['created'] = REQUEST_TIME;
      db_insert('drop_invoice')
        ->fields($fields)
        ->execute();
      $new++;
    }
  }

  backdrop_set_message(t('Imported @new new and updated @updated existing invoices.', array(
    '@new'     => $new,
    '@updated' => $updated,
  )));

  // Create revenue lines if requested.
  if ($create_revenue) {
    $created = _dropfinance_create_revenue_from_invoices();
    if ($created > 0) {
      backdrop_set_message(t('Created @count revenue projection lines from invoice data.', array('@count' => $created)));
    }
  }

  $form_state['redirect'] = 'drop/finance/revenue/clients';
}

/**
 * Parse a FreshBooks invoice export CSV.
 *
 * Aggregates line items by invoice number.
 *
 * @param string $file_path
 *   Full path to the CSV file.
 *
 * @return array
 *   Associative array keyed by invoice number, each with keys: client_name,
 *   date_issued, date_paid, status, amount, currency, cid.
 */
function _dropfinance_parse_freshbooks_csv($file_path) {
  $invoices = array();
  $handle = fopen($file_path, 'r');
  if (!$handle) {
    return $invoices;
  }

  $header_skipped = FALSE;

  while (($data = fgetcsv($handle)) !== FALSE) {
    if (!$header_skipped) {
      $header_skipped = TRUE;
      if (isset($data[0]) && stripos($data[0], 'Client') !== FALSE) {
        continue;
      }
    }

    if (count($data) < 17) {
      continue;
    }

    $client_name   = trim($data[0]);
    $invoice_num   = trim($data[1]);
    $date_issued   = trim($data[2]);
    $status        = trim($data[4]);
    $date_paid_str = trim($data[5]);
    $line_total    = (float) trim($data[16]);
    $currency      = isset($data[17]) ? trim($data[17]) : 'USD';

    if (empty($invoice_num) || empty($client_name)) {
      continue;
    }

    if (!isset($invoices[$invoice_num])) {
      $invoices[$invoice_num] = array(
        'client_name' => $client_name,
        'date_issued' => strtotime($date_issued) ?: 0,
        'date_paid'   => !empty($date_paid_str) ? strtotime($date_paid_str) : NULL,
        'status'      => $status,
        'amount'      => 0,
        'currency'    => $currency,
        'cid'         => NULL,
      );
    }

    $invoices[$invoice_num]['amount'] += $line_total;
  }

  fclose($handle);
  return $invoices;
}

/**
 * Build a mapping of FreshBooks client names to CRM contact IDs.
 *
 * Uses case-insensitive matching plus known name variations.
 *
 * @return array
 *   Associative array of FreshBooks client name => CRM cid.
 */
function _dropfinance_build_crm_client_map() {
  $map = array();

  if (!db_table_exists('crm_contact')) {
    return $map;
  }

  // Load all CRM organizations keyed by lowercase name.
  $orgs = db_query("SELECT cid, name FROM {crm_contact} WHERE type = 'crm_organization'")
    ->fetchAllKeyed();
  $by_lower = array();
  foreach ($orgs as $cid => $name) {
    $by_lower[strtolower($name)] = $cid;
  }

  // Known aliases: FreshBooks name (lowercase) => CRM name (lowercase).
  $aliases = array(
    'friends of the mississippi river' => 'fmr',
    'friends of spco'                  => 'friends of the st paul chamber orchestra',
    'particular collective'            => 'particular',
    'holy union sisters (susc.org)'    => 'susc.org',
    'catho-lit'                        => 'catholit',
    'minnesota center for philosophy of science' => 'minnesota center for the philosophy of science',
    'adc minnesota'                    => 'adc minnesota / nevada',
    'flatbush food coop'               => 'flatbush food coop',
  );

  // Build the map: first try exact (case-insensitive) match, then aliases.
  // We don't know FreshBooks names yet, so build a lookup function.
  // Store both the alias map and CRM lookup for use in the validate handler.
  foreach ($orgs as $cid => $name) {
    // Store CRM name -> cid (original case).
    $map[$name] = $cid;
  }
  foreach ($aliases as $fb_lower => $crm_lower) {
    if (isset($by_lower[$crm_lower])) {
      $map[$fb_lower] = $by_lower[$crm_lower];
    }
  }

  return $map;
}

/**
 * Create revenue projection lines from imported invoice data.
 *
 * Creates one revenue line per active client (paid invoices in the last 12
 * months) with their average monthly amount.
 *
 * @return int
 *   Number of revenue lines created.
 */
function _dropfinance_create_revenue_from_invoices() {
  $twelve_months_ago = strtotime('-12 months', REQUEST_TIME);

  // Get clients with paid invoices in the last 12 months.
  $results = db_query("
    SELECT client_name, cid, COUNT(*) AS inv_count, SUM(amount) AS total,
           MIN(date_issued) AS first_date, MAX(date_issued) AS last_date
    FROM {drop_invoice}
    WHERE status IN ('paid', 'auto-paid')
      AND date_issued >= :since
    GROUP BY client_name, cid
    ORDER BY client_name
  ", array(':since' => $twelve_months_ago));

  $created = 0;
  global $user;

  foreach ($results as $row) {
    // Skip clients with no meaningful revenue.
    if ($row->total < 100) {
      continue;
    }

    // Calculate monthly average.
    $monthly = $row->total / 12.0;

    // Check if a revenue line already exists for this client.
    $query = db_select('drop_revenue', 'r')
      ->condition('r.label', $row->client_name)
      ->countQuery()
      ->execute()
      ->fetchField();
    if ($query > 0) {
      continue;
    }

    // Also check by CRM ID if available.
    if (!empty($row->cid)) {
      $by_cid = db_select('drop_revenue', 'r')
        ->condition('r.cid', $row->cid)
        ->countQuery()
        ->execute()
        ->fetchField();
      if ($by_cid > 0) {
        continue;
      }
    }

    $revenue = new DropRevenue();
    $revenue->label      = $row->client_name;
    $revenue->amount     = round($monthly, 2);
    $revenue->frequency  = 'monthly';
    $revenue->cid        = $row->cid;
    $revenue->start_date = $row->first_date;
    $revenue->active     = 1;
    $revenue->uid        = $user->uid;
    $revenue->save();
    $created++;
  }

  return $created;
}

// ---------------------------------------------------------------------------
// Client Revenue Report
// ---------------------------------------------------------------------------

/**
 * Page callback: client revenue report.
 *
 * Shows revenue per client per year from imported invoice data.
 */
function dropfinance_client_revenue_report() {
  backdrop_set_title(t('Client Revenue'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Revenue'), 'drop/finance/revenue'),
  ));

  $module_path = backdrop_get_path('module', 'dropfinance');

  // Check if we have invoice data.
  $total = db_select('drop_invoice', 'i')
    ->countQuery()
    ->execute()
    ->fetchField();

  if ($total == 0) {
    return array(
      '#markup' => '<p>' . t('No invoice data imported yet. ')
        . l(t('Import FreshBooks invoices'), 'drop/finance/import/freshbooks')
        . '</p>',
    );
  }

  $output = array();
  $output['#attached']['css'][] = $module_path . '/dropfinance.css';
  $output['#attached']['js'][] = $module_path . '/js/chart.umd.min.js';

  // Get all years with data.
  $years = db_query("
    SELECT DISTINCT YEAR(FROM_UNIXTIME(date_issued)) AS yr
    FROM {drop_invoice}
    WHERE status IN ('paid', 'auto-paid')
    ORDER BY yr
  ")->fetchCol();

  // Get revenue by client by year (paid invoices only).
  $data = db_query("
    SELECT client_name, cid,
           YEAR(FROM_UNIXTIME(date_issued)) AS yr,
           SUM(amount) AS total,
           COUNT(*) AS inv_count
    FROM {drop_invoice}
    WHERE status IN ('paid', 'auto-paid')
    GROUP BY client_name, cid, yr
    ORDER BY client_name, yr
  ");

  $client_data = array();
  $year_totals = array_fill_keys($years, 0);
  foreach ($data as $row) {
    if (!isset($client_data[$row->client_name])) {
      $client_data[$row->client_name] = array(
        'cid'    => $row->cid,
        'years'  => array_fill_keys($years, 0),
        'total'  => 0,
      );
    }
    $client_data[$row->client_name]['years'][$row->yr] = (float) $row->total;
    $client_data[$row->client_name]['total'] += (float) $row->total;
    $year_totals[$row->yr] += (float) $row->total;
  }

  // Sort by total revenue descending.
  uasort($client_data, function ($a, $b) {
    return $b['total'] - $a['total'];
  });

  // Build table.
  $header = array(t('Client'));
  foreach ($years as $yr) {
    $header[] = $yr;
  }
  $header[] = t('Total');

  $rows = array();
  $grand_total = 0;
  foreach ($client_data as $client_name => $data) {
    $row = array(check_plain($client_name));
    foreach ($years as $yr) {
      $amt = $data['years'][$yr];
      $row[] = $amt > 0 ? '$' . number_format($amt, 0) : '';
    }
    $row[] = array(
      'data'  => '<strong>$' . number_format($data['total'], 0) . '</strong>',
      'class' => array('dropfinance-total-cell'),
    );
    $rows[] = $row;
    $grand_total += $data['total'];
  }

  // Totals row.
  $totals_row = array(array('data' => '<strong>' . t('Total') . '</strong>'));
  foreach ($years as $yr) {
    $totals_row[] = array(
      'data' => '<strong>$' . number_format($year_totals[$yr], 0) . '</strong>',
    );
  }
  $totals_row[] = array(
    'data'  => '<strong>$' . number_format($grand_total, 0) . '</strong>',
    'class' => array('dropfinance-total-cell'),
  );
  $rows[] = array(
    'data'  => $totals_row,
    'class' => array('dropfinance-totals-row'),
  );

  $output['summary'] = array(
    '#markup' => '<p>' . t('@clients clients, @invoices invoices, $@total total revenue.', array(
      '@clients'  => count($client_data),
      '@invoices' => $total,
      '@total'    => number_format($grand_total, 2),
    )) . '</p>',
    '#weight' => -10,
  );

  $output['table'] = array(
    '#theme'      => 'table',
    '#header'     => $header,
    '#rows'       => $rows,
    '#attributes' => array('class' => array('dropfinance-client-revenue-table')),
    '#weight'     => 0,
  );

  // Revenue by year bar chart.
  $output['chart'] = array(
    '#markup' => '<div class="dropfinance-charts">'
      . '<div class="dropfinance-chart-wrapper">'
      . '<canvas id="dropfinance-yearly-revenue" height="300"></canvas>'
      . '</div>'
      . '<div class="dropfinance-chart-wrapper">'
      . '<canvas id="dropfinance-top-clients" height="300"></canvas>'
      . '</div>'
      . '</div>',
    '#weight' => 10,
  );

  // Top 10 clients for the donut.
  $top_clients = array_slice($client_data, 0, 10, TRUE);
  $top_labels = array_keys($top_clients);
  $top_values = array();
  foreach ($top_clients as $c) {
    $top_values[] = round($c['total'], 2);
  }

  $js_data = array(
    'yearlyRevenue' => array(
      'labels' => array_map('strval', $years),
      'values' => array_values(array_map('floatval', $year_totals)),
    ),
    'topClients' => array(
      'labels' => $top_labels,
      'values' => $top_values,
    ),
  );

  $output['#attached']['js'][] = array(
    'data' => array('dropfinance' => $js_data),
    'type' => 'setting',
  );
  $output['#attached']['js'][] = $module_path . '/js/dropfinance.charts.js';

  return $output;
}
