<?php
/**
 * @file
 * CSV import for Drop Finance.
 */

/**
 * Form: CSV import (upload + preview + batch).
 */
function dropfinance_csv_import_form($form, &$form_state) {
  backdrop_set_title(t('Import CSV'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Import'), 'drop/finance/import'),
  ));

  $form['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Step 2: Preview.
  if (!empty($form_state['storage']['parsed'])) {
    return _dropfinance_csv_preview_form($form, $form_state);
  }

  // Step 1: Upload.
  $form['info'] = array(
    '#markup' => '<p>' . t('Upload a bank CSV export to import transactions. The expected format is: Account Number, Post Date, Check, Description, Debit, Credit, Status, Balance.') . '</p>',
  );

  $form['csv_file'] = array(
    '#type'              => 'managed_file',
    '#title'             => t('CSV file'),
    '#upload_location'   => 'temporary://',
    '#upload_validators' => array(
      'file_validate_extensions' => array('csv'),
      'file_validate_size'       => array(5 * 1024 * 1024),
    ),
    '#required'          => TRUE,
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
 * Validate handler: parse the CSV on upload.
 */
function dropfinance_csv_import_form_validate($form, &$form_state) {
  // Only validate file upload step.
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
  $parsed = _dropfinance_parse_csv($path);

  if (empty($parsed)) {
    form_set_error('csv_file', t('No valid transactions found in the CSV file.'));
    return;
  }

  // Check for duplicates.
  $hashes = array();
  foreach ($parsed as $row) {
    $hashes[] = $row['hash'];
  }
  $existing = _dropfinance_check_duplicates($hashes);

  // Apply matching rules and separate duplicates.
  $to_import = array();
  $duplicates = array();
  foreach ($parsed as $row) {
    if (in_array($row['hash'], $existing)) {
      $duplicates[] = $row;
      continue;
    }

    // Apply matching rules.
    $rule = _dropfinance_apply_matching_rules($row['description'], $row['type']);
    if ($rule) {
      $row['vendor_name'] = $rule['vendor_name'];
      $row['vid']         = $rule['vid'];
      $row['category']    = $rule['category'];
      $row['is_personal'] = (int) $rule['is_personal'];
      $row['matched']     = TRUE;
    }
    else {
      $row['matched'] = FALSE;
    }

    $to_import[] = $row;
  }

  $form_state['storage']['parsed'] = $to_import;
  $form_state['storage']['duplicates'] = $duplicates;
  $form_state['rebuild'] = TRUE;
}

/**
 * Build the preview step of the import form.
 */
function _dropfinance_csv_preview_form(&$form, &$form_state) {
  $to_import = $form_state['storage']['parsed'];
  $duplicates = $form_state['storage']['duplicates'];

  $matched = 0;
  $unmatched = 0;
  $personal = 0;
  foreach ($to_import as $row) {
    if ($row['matched']) {
      $matched++;
    }
    else {
      $unmatched++;
    }
    if (!empty($row['is_personal'])) {
      $personal++;
    }
  }

  $form['summary'] = array(
    '#markup' => '<div class="dropfinance-import-summary">'
      . '<strong>' . t('Import preview') . '</strong><br>'
      . t('@total transactions to import (@matched matched by rules, @unmatched unmatched, @personal personal). @dupes duplicates skipped.', array(
          '@total'     => count($to_import),
          '@matched'   => $matched,
          '@unmatched' => $unmatched,
          '@personal'  => $personal,
          '@dupes'     => count($duplicates),
        ))
      . '</div>',
    '#weight' => -10,
  );

  // Show first 100 rows.
  $preview_rows = array_slice($to_import, 0, 100);
  $rows = array();
  foreach ($preview_rows as $row) {
    $type_label = $row['type'] === 'credit'
      ? '<span class="dropfinance-positive">' . t('Credit') . '</span>'
      : t('Debit');
    $cat = !empty($row['category']) ? check_plain($row['category']) : '<em>' . t('Uncategorized') . '</em>';
    $vendor = !empty($row['vendor_name']) ? check_plain($row['vendor_name']) : '';
    $personal_flag = !empty($row['is_personal']) ? t('Yes') : '';

    $row_class = $row['matched'] ? 'dropfinance-rule-matched' : 'dropfinance-rule-unmatched';
    $rows[] = array(
      'data' => array(
        format_date($row['date'], 'custom', 'M j, Y'),
        check_plain(backdrop_substr($row['description'], 0, 60)),
        '$' . number_format($row['amount'], 2),
        $type_label,
        $cat,
        $vendor,
        $personal_flag,
      ),
      'class' => array($row_class),
    );
  }

  if (count($to_import) > 100) {
    $form['more'] = array(
      '#markup' => '<p><em>' . t('Showing first 100 of @total transactions.', array('@total' => count($to_import))) . '</em></p>',
      '#weight' => -5,
    );
  }

  $form['preview'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Date'), t('Description'), t('Amount'), t('Type'), t('Category'), t('Vendor'), t('Personal')),
    '#rows'   => $rows,
    '#weight' => 0,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Import @count transactions', array('@count' => count($to_import))),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/finance/import'),
  );

  return $form;
}

/**
 * Submit handler: run the batch import.
 */
function dropfinance_csv_import_form_submit($form, &$form_state) {
  // Step 1: file upload just triggers rebuild via validate.
  if (empty($form_state['storage']['parsed'])) {
    return;
  }

  // Step 2: batch import.
  $transactions = $form_state['storage']['parsed'];

  $batch = array(
    'title'      => t('Importing transactions...'),
    'operations' => array(),
    'finished'   => '_dropfinance_csv_import_finished',
    'file'       => backdrop_get_path('module', 'dropfinance') . '/dropfinance.csv.inc',
  );

  $chunks = array_chunk($transactions, 50);
  foreach ($chunks as $chunk) {
    $batch['operations'][] = array('_dropfinance_csv_import_batch', array($chunk));
  }

  batch_set($batch);
  $form_state['redirect'] = 'drop/finance/transactions';
}

/**
 * Batch operation: import a chunk of transactions.
 *
 * @param array $chunk
 *   Array of parsed transaction rows.
 * @param array $context
 *   Batch context.
 */
function _dropfinance_csv_import_batch($chunk, &$context) {
  if (!isset($context['results']['count'])) {
    $context['results']['count'] = 0;
  }

  global $user;

  foreach ($chunk as $row) {
    $txn = new DropTransaction();
    $txn->hash         = $row['hash'];
    $txn->date         = $row['date'];
    $txn->description  = $row['description'];
    $txn->amount       = $row['amount'];
    $txn->type         = $row['type'];
    $txn->vendor_name  = isset($row['vendor_name']) ? $row['vendor_name'] : '';
    $txn->vid          = isset($row['vid']) ? $row['vid'] : NULL;
    $txn->category     = isset($row['category']) ? $row['category'] : '';
    $txn->is_personal  = isset($row['is_personal']) ? (int) $row['is_personal'] : 0;
    $txn->check_number = isset($row['check_number']) ? $row['check_number'] : '';
    $txn->balance      = isset($row['balance']) ? $row['balance'] : NULL;
    $txn->uid          = $user->uid;
    $txn->save();

    $context['results']['count']++;
  }

  $context['message'] = t('Imported @count transactions...', array('@count' => $context['results']['count']));
}

/**
 * Batch finished callback.
 */
function _dropfinance_csv_import_finished($success, $results, $operations) {
  if ($success) {
    $count = isset($results['count']) ? $results['count'] : 0;
    backdrop_set_message(t('Successfully imported @count transactions.', array('@count' => $count)));
  }
  else {
    backdrop_set_message(t('Import encountered errors.'), 'error');
  }
}

/**
 * Parse a bank CSV file.
 *
 * @param string $file_path
 *   Full path to the CSV file.
 *
 * @return array
 *   Array of parsed rows, each with keys: hash, date, description, amount,
 *   type, check_number, balance, vendor_name, vid, category, is_personal.
 */
function _dropfinance_parse_csv($file_path) {
  $rows = array();
  $hash_counts = array();
  $handle = fopen($file_path, 'r');
  if (!$handle) {
    return $rows;
  }

  $header_skipped = FALSE;

  while (($data = fgetcsv($handle)) !== FALSE) {
    // Skip header row.
    if (!$header_skipped) {
      $header_skipped = TRUE;
      if (isset($data[0]) && stripos($data[0], 'Account') !== FALSE) {
        continue;
      }
    }

    // Need at least 6 columns.
    if (count($data) < 6) {
      continue;
    }

    $post_date    = trim($data[1]);
    $check        = trim($data[2]);
    $description  = trim($data[3]);
    $debit        = trim($data[4]);
    $credit       = trim($data[5]);
    $balance_val  = isset($data[7]) ? trim($data[7]) : '';

    // Determine type and amount.
    $type   = '';
    $amount = 0;
    if ($debit !== '' && $debit !== '0') {
      $type   = 'debit';
      $amount = (float) str_replace(',', '', $debit);
    }
    elseif ($credit !== '' && $credit !== '0') {
      $type   = 'credit';
      $amount = (float) str_replace(',', '', $credit);
    }
    else {
      continue; // Skip rows with no amount.
    }

    // Parse date.
    $date = strtotime($post_date);
    if ($date === FALSE) {
      continue;
    }

    // Compute hash for duplicate detection.
    // Append a counter for rows that produce the same base hash (e.g. two
    // identical transactions on the same day).
    $base_hash = $post_date . '|' . $description . '|' . $amount . '|' . $type . '|' . $check;
    if (!isset($hash_counts[$base_hash])) {
      $hash_counts[$base_hash] = 0;
    }
    $hash_counts[$base_hash]++;
    $hash = hash('sha256', $base_hash . '|' . $hash_counts[$base_hash]);

    $rows[] = array(
      'hash'         => $hash,
      'date'         => $date,
      'description'  => $description,
      'amount'       => $amount,
      'type'         => $type,
      'check_number' => $check,
      'balance'      => $balance_val !== '' ? (float) str_replace(',', '', $balance_val) : NULL,
      'vendor_name'  => '',
      'vid'          => NULL,
      'category'     => '',
      'is_personal'  => 0,
    );
  }

  fclose($handle);
  return $rows;
}

/**
 * Check which hashes already exist in the database.
 *
 * @param array $hashes
 *   Array of hash strings.
 *
 * @return array
 *   Array of hashes that already exist.
 */
function _dropfinance_check_duplicates($hashes) {
  if (empty($hashes)) {
    return array();
  }

  return db_select('drop_transaction', 't')
    ->fields('t', array('hash'))
    ->condition('t.hash', $hashes, 'IN')
    ->execute()
    ->fetchCol();
}
