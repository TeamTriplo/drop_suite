<?php
/**
 * @file
 * Admin forms for Drop Finance.
 */

/**
 * Settings form for Drop Finance.
 */
function dropfinance_settings_form($form, &$form_state) {
  $config = config('dropfinance.settings');

  $form['balance'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Current balance'),
    '#description' => t('Your current account balance, used as the starting point for projections.'),
    '#collapsible' => TRUE,
    '#collapsed'   => TRUE,
    '#weight'      => 0,
  );
  $form['balance']['balance_amount'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Balance ($)'),
    '#default_value' => $config->get('balance'),
    '#size'          => 15,
  );
  $form['balance']['balance_date'] = array(
    '#type'          => 'textfield',
    '#title'         => t('As of date'),
    '#description'   => t('When this balance was last verified (e.g. "April 10, 2026").'),
    '#default_value' => $config->get('balance_date'),
    '#size'          => 20,
  );

  $form['source'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Data source'),
    '#description' => t('Configure the remote Drop Suite site to import client and project data from.'),
    '#collapsible' => TRUE,
    '#collapsed'   => TRUE,
    '#weight'      => 10,
  );
  $form['source']['source_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Source site URL'),
    '#description'   => t('Base URL of the remote Drop Suite site, e.g. <code>https://support.simplo.site</code>'),
    '#default_value' => $config->get('source_url'),
    '#size'          => 60,
  );
  $form['source']['api_key'] = array(
    '#type'          => 'textfield',
    '#title'         => t('API key'),
    '#description'   => t('The shared secret configured on the source site.'),
    '#default_value' => $config->get('api_key'),
    '#size'          => 40,
  );

  // Matching rules summary.
  $rules = config('dropfinance.matching_rules')->get('rules');
  $rule_count = is_array($rules) ? count($rules) : 0;
  $form['matching'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Matching rules'),
    '#description' => t('Rules for auto-categorizing bank transactions during CSV import.'),
    '#collapsible' => TRUE,
    '#collapsed'   => TRUE,
    '#weight'      => 15,
  );
  $form['matching']['info'] = array(
    '#markup' => '<p>' . t('@count rules configured. ', array('@count' => $rule_count))
      . l(t('Manage rules'), 'drop/finance/settings/rules') . '</p>',
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save settings'),
  );

  return $form;
}

/**
 * Submit callback for dropfinance_settings_form.
 */
function dropfinance_settings_form_submit($form, &$form_state) {
  $config = config('dropfinance.settings');
  $config->set('source_url', trim($form_state['values']['source_url']));
  $config->set('api_key', trim($form_state['values']['api_key']));
  $config->set('balance', trim($form_state['values']['balance_amount']));
  $config->set('balance_date', trim($form_state['values']['balance_date']));
  $config->save();
  backdrop_set_message(t('Settings saved.'));
}

// ---------------------------------------------------------------------------
// Matching Rules
// ---------------------------------------------------------------------------

/**
 * Page callback: list all matching rules.
 *
 * @return array
 *   A render array.
 */
function dropfinance_rules_list() {
  backdrop_set_title(t('Matching Rules'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Settings'), 'drop/finance/settings'),
  ));

  $rules = config('dropfinance.matching_rules')->get('rules');
  if (!is_array($rules)) {
    $rules = array();
  }

  $rows = array();
  foreach ($rules as $index => $rule) {
    $personal = !empty($rule['is_personal']) ? t('Yes') : '';
    $actions = l(t('Edit'), 'drop/finance/settings/rules/' . $index . '/edit')
      . ' &nbsp;·&nbsp; '
      . l(t('Delete'), 'drop/finance/settings/rules/' . $index . '/delete');

    $rows[] = array(
      '<code>' . check_plain($rule['pattern']) . '</code>',
      check_plain($rule['vendor_name']),
      check_plain($rule['category']),
      $personal,
      $actions,
    );
  }

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  // Count uncategorized transactions.
  $uncat_count = db_select('drop_transaction', 't')
    ->condition('t.category', '')
    ->countQuery()
    ->execute()
    ->fetchField();

  $output['actions'] = array(
    '#markup' => '<p>' . l(t('+ Add rule'), 'drop/finance/settings/rules/add')
      . ' &nbsp;·&nbsp; '
      . l(t('Re-apply to uncategorized (@count)', array('@count' => $uncat_count)), 'drop/finance/settings/rules/reapply')
      . '</p>',
    '#weight' => -10,
  );

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Pattern'), t('Vendor Name'), t('Category'), t('Personal'), t('Actions')),
    '#rows'   => $rows,
    '#empty'  => t('No matching rules configured.'),
  );

  return $output;
}

/**
 * Form: add or edit a matching rule.
 *
 * @param array $form
 * @param array $form_state
 * @param int|null $index
 *   Rule index for editing, or NULL for add.
 *
 * @return array
 */
function dropfinance_rule_form($form, &$form_state, $index = NULL) {
  $rules = config('dropfinance.matching_rules')->get('rules');
  if (!is_array($rules)) {
    $rules = array();
  }

  $rule = NULL;
  if ($index !== NULL && isset($rules[$index])) {
    $rule = $rules[$index];
  }
  $is_edit = ($rule !== NULL);

  $form['#rule_index'] = $index;

  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Settings'), 'drop/finance/settings'),
    l(t('Rules'), 'drop/finance/settings/rules'),
  ));

  $form['pattern'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Pattern'),
    '#description'   => t('Substring(s) to match in bank descriptions. Use <code>|</code> to separate alternatives (e.g. "LINODE|AKAMAI"). Case-insensitive.'),
    '#default_value' => $is_edit ? $rule['pattern'] : '',
    '#required'      => TRUE,
    '#maxlength'     => 255,
    '#weight'        => -20,
  );

  $form['vendor_name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Vendor name'),
    '#description'   => t('Normalized vendor name to assign. Leave blank to use the raw description.'),
    '#default_value' => $is_edit ? $rule['vendor_name'] : '',
    '#maxlength'     => 255,
    '#weight'        => -15,
  );

  $form['category'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Category'),
    '#description'   => t('Category to assign (e.g. "Hosting", "Software", "Groceries").'),
    '#default_value' => $is_edit ? $rule['category'] : '',
    '#required'      => TRUE,
    '#maxlength'     => 100,
    '#weight'        => -10,
  );

  $form['is_personal'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Personal expense (not business)'),
    '#default_value' => $is_edit ? $rule['is_personal'] : 0,
    '#weight'        => 0,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Save') : t('Add rule'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), 'drop/finance/settings/rules'),
  );

  return $form;
}

/**
 * Submit handler for dropfinance_rule_form().
 */
function dropfinance_rule_form_submit($form, &$form_state) {
  $config = config('dropfinance.matching_rules');
  $rules = $config->get('rules');
  if (!is_array($rules)) {
    $rules = array();
  }

  $rule = array(
    'pattern'     => trim($form_state['values']['pattern']),
    'vendor_name' => trim($form_state['values']['vendor_name']),
    'vid'         => NULL,
    'category'    => trim($form_state['values']['category']),
    'is_personal' => (int) $form_state['values']['is_personal'],
  );

  $index = $form['#rule_index'];
  if ($index !== NULL && isset($rules[$index])) {
    $rules[$index] = $rule;
  }
  else {
    $rules[] = $rule;
  }

  $config->set('rules', $rules);
  $config->save();

  backdrop_set_message(t('Matching rule saved.'));
  $form_state['redirect'] = 'drop/finance/settings/rules';
}

/**
 * Form: confirm rule deletion.
 *
 * @param array $form
 * @param array $form_state
 * @param int $index
 *   Rule index to delete.
 *
 * @return array
 */
function dropfinance_rule_delete_form($form, &$form_state, $index) {
  $rules = config('dropfinance.matching_rules')->get('rules');
  if (!isset($rules[$index])) {
    backdrop_not_found();
    return array();
  }

  $form['#rule_index'] = $index;
  return confirm_form(
    $form,
    t('Are you sure you want to delete the rule matching %pattern?', array('%pattern' => $rules[$index]['pattern'])),
    'drop/finance/settings/rules',
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for dropfinance_rule_delete_form().
 */
function dropfinance_rule_delete_form_submit($form, &$form_state) {
  $config = config('dropfinance.matching_rules');
  $rules = $config->get('rules');
  $index = $form['#rule_index'];

  if (isset($rules[$index])) {
    $pattern = $rules[$index]['pattern'];
    array_splice($rules, $index, 1);
    $config->set('rules', $rules);
    $config->save();
    backdrop_set_message(t('Rule matching %pattern deleted.', array('%pattern' => $pattern)));
  }

  $form_state['redirect'] = 'drop/finance/settings/rules';
}

/**
 * Form: confirm re-applying matching rules to uncategorized transactions.
 */
function dropfinance_rules_reapply_form($form, &$form_state) {
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
    l(t('Settings'), 'drop/finance/settings'),
    l(t('Rules'), 'drop/finance/settings/rules'),
  ));

  $uncat_count = db_select('drop_transaction', 't')
    ->condition('t.category', '')
    ->countQuery()
    ->execute()
    ->fetchField();

  // Preview: show how many would match.
  $transactions = db_select('drop_transaction', 't')
    ->fields('t', array('tid', 'description', 'type'))
    ->condition('t.category', '')
    ->execute()
    ->fetchAll();

  $match_count = 0;
  foreach ($transactions as $txn) {
    $rule = _dropfinance_apply_matching_rules($txn->description, $txn->type);
    if ($rule) {
      $match_count++;
    }
  }

  $form['info'] = array(
    '#markup' => '<p>' . t('There are @uncat uncategorized transactions. Current rules would match @match of them.', array(
      '@uncat' => $uncat_count,
      '@match' => $match_count,
    )) . '</p>',
  );

  return confirm_form(
    $form,
    t('Re-apply matching rules to uncategorized transactions?'),
    'drop/finance/settings/rules',
    t('This will update vendor name, category, and personal flag on matched transactions. Transactions that do not match any rule will be left unchanged.'),
    t('Re-apply rules'),
    t('Cancel')
  );
}

/**
 * Submit handler for dropfinance_rules_reapply_form().
 */
function dropfinance_rules_reapply_form_submit($form, &$form_state) {
  $transactions = db_select('drop_transaction', 't')
    ->fields('t', array('tid', 'description', 'type'))
    ->condition('t.category', '')
    ->execute()
    ->fetchAll();

  $matched = 0;
  foreach ($transactions as $txn) {
    $rule = _dropfinance_apply_matching_rules($txn->description, $txn->type);
    if ($rule) {
      $fields = array(
        'category' => $rule['category'],
        'is_personal' => (int) $rule['is_personal'],
        'changed' => REQUEST_TIME,
      );
      if (!empty($rule['vendor_name'])) {
        $fields['vendor_name'] = $rule['vendor_name'];
      }
      db_update('drop_transaction')
        ->fields($fields)
        ->condition('tid', $txn->tid)
        ->execute();
      $matched++;
    }
  }

  backdrop_set_message(t('@count transactions categorized.', array('@count' => $matched)));
  $form_state['redirect'] = 'drop/finance/settings/rules';
}

/**
 * Apply matching rules to a bank description.
 *
 * @param string $description
 *   The raw bank description.
 *
 * @return array|null
 *   The matched rule array, or NULL if no match.
 */
function _dropfinance_apply_matching_rules($description, $type = NULL) {
  $rules = config('dropfinance.matching_rules')->get('rules');
  if (!is_array($rules)) {
    return NULL;
  }

  foreach ($rules as $rule) {
    // If rule specifies a type, only match that type.
    if (!empty($rule['type']) && $type !== NULL && $rule['type'] !== $type) {
      continue;
    }
    $patterns = explode('|', $rule['pattern']);
    foreach ($patterns as $p) {
      $p = trim($p);
      if ($p !== '' && stripos($description, $p) !== FALSE) {
        return $rule;
      }
    }
  }

  return NULL;
}
