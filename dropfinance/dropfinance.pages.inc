<?php
/**
 * @file
 * Page callbacks for Drop Finance.
 */

/**
 * Page callback: Finance dashboard with 6-month projection.
 */
function dropfinance_dashboard() {
  backdrop_set_title(t('Finance'));

  $config = config('dropfinance.settings');
  $balance = (float) $config->get('balance');
  $balance_date = $config->get('balance_date');
  $module_path = backdrop_get_path('module', 'dropfinance');

  $output = array(
    '#attached' => array(
      'css' => array($module_path . '/dropfinance.css'),
      'js'  => array(
        $module_path . '/js/chart.umd.min.js' => array('weight' => -10),
      ),
    ),
  );

  // Current balance.
  $balance_display = '$' . number_format($balance, 2);
  $date_note = '';
  if (!empty($balance_date)) {
    $date_note = ' <span class="dropfinance-balance-date">' . t('as of @date', array('@date' => $balance_date)) . '</span>';
  }
  $output['balance'] = array(
    '#markup' => '<div class="dropfinance-balance">'
      . '<strong>' . t('Current balance:') . '</strong> '
      . '<span class="dropfinance-balance-amount">' . $balance_display . '</span>'
      . $date_note
      . ' ' . l(t('(update)'), 'drop/finance/settings', array('attributes' => array('class' => array('dropfinance-balance-edit'))))
      . '</div>',
    '#weight' => -20,
  );

  // Projection range toggle (6 or 12 months).
  $projection_months = isset($_GET['months']) && $_GET['months'] == '12' ? 12 : 6;
  $avg_personal = _dropfinance_avg_monthly_personal();
  $projection = _dropfinance_build_projection($balance, $projection_months, $avg_personal);

  // Projection table.
  $rows = array();
  foreach ($projection as $month) {
    $net_class = $month['net'] >= 0 ? 'dropfinance-positive' : 'dropfinance-negative';
    $rows[] = array(
      $month['label'],
      '$' . number_format($month['income'], 2),
      '$' . number_format($month['expenses'], 2),
      '$' . number_format($month['personal'], 2),
      array('data' => ($month['net'] >= 0 ? '+$' : '-$') . number_format(abs($month['net']), 2), 'class' => array($net_class)),
      array('data' => '$' . number_format($month['balance'], 2), 'class' => array($month['balance'] >= 0 ? 'dropfinance-positive' : 'dropfinance-negative')),
    );
  }

  // Title with toggle links.
  $toggle_6 = $projection_months == 6
    ? '<strong>' . t('6 months') . '</strong>'
    : l(t('6 months'), 'drop/finance', array('query' => array('months' => '6')));
  $toggle_12 = $projection_months == 12
    ? '<strong>' . t('12 months') . '</strong>'
    : l(t('12 months'), 'drop/finance', array('query' => array('months' => '12')));

  $output['projection_title'] = array(
    '#markup' => '<h2>' . t('Cash Flow Projection') . ' <span class="dropfinance-toggle">' . $toggle_6 . ' · ' . $toggle_12 . '</span></h2>',
    '#weight' => -10,
  );

  $output['projection'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Month'), t('Income'), t('Expenses'), t('Personal'), t('Net'), t('Balance')),
    '#rows'   => $rows,
    '#empty'  => t('No data to project.'),
    '#attributes' => array('class' => array('dropfinance-projection-table')),
    '#weight' => -9,
  );

  $output['projection_help'] = array(
    '#markup' => '<div class="dropfinance-help-text">'
      . t('Projected from active revenue and expense lines. Personal spending is the monthly average from bank transactions. Net = income minus expenses minus personal.')
      . '</div>',
    '#weight' => -8.9,
  );

  // Chart canvases.
  $output['charts'] = array(
    '#markup' => '<div class="dropfinance-charts">'
      . '<div class="dropfinance-chart-wrapper">'
      . '<canvas id="dropfinance-line-chart" height="300"></canvas>'
      . '</div>'
      . '<div class="dropfinance-chart-wrapper">'
      . '<canvas id="dropfinance-bar-chart" height="300"></canvas>'
      . '</div>'
      . '</div>',
    '#weight' => -8,
  );

  // Revenue projection stacked chart.
  $output['revenue_stack_chart'] = array(
    '#markup' => '<div class="dropfinance-charts">'
      . '<div class="dropfinance-chart-wrapper" style="grid-column: 1 / -1;">'
      . '<canvas id="dropfinance-revenue-stack" height="300"></canvas>'
      . '</div>'
      . '</div>',
    '#weight' => -7.95,
  );

  // Pass projection data to JS.
  $js_data = array(
    'labels'           => array(),
    'income'           => array(),
    'recurring_income'  => array(),
    'pipeline_income'   => array(),
    'pipeline_details'  => array(),
    'expenses'         => array(),
    'personal'         => array(),
    'balance'          => array(),
    'net'              => array(),
  );
  foreach ($projection as $month) {
    $js_data['labels'][]           = $month['label'];
    $js_data['income'][]           = round($month['income'], 2);
    $js_data['recurring_income'][]  = round($month['recurring_income'], 2);
    $js_data['pipeline_income'][]   = round($month['pipeline_income'], 2);
    $js_data['pipeline_details'][]  = $month['pipeline_details'];
    $js_data['expenses'][]         = round($month['expenses'], 2);
    $js_data['personal'][]         = round($month['personal'], 2);
    $js_data['balance'][]          = round($month['balance'], 2);
    $js_data['net'][]              = round($month['net'], 2);
  }
  // Historical profitability from bank transactions.
  $hist_data = _dropfinance_monthly_profitability();
  if (!empty($hist_data)) {
    $output['hist_title'] = array(
      '#markup' => '<h2>' . t('Monthly Profitability (Actual)') . '</h2>',
      '#weight' => -7.5,
    );

    // Chart.
    $output['hist_chart'] = array(
      '#markup' => '<div class="dropfinance-charts">'
        . '<div class="dropfinance-chart-wrapper" style="grid-column: 1 / -1;">'
        . '<canvas id="dropfinance-profitability-chart" height="300"></canvas>'
        . '</div>'
        . '</div>',
      '#weight' => -7.4,
    );

    // Table.
    $hist_rows = array();
    $yearly = array();
    foreach ($hist_data as $m) {
      $year = substr($m['label'], -4);
      if (!isset($yearly[$year])) {
        $yearly[$year] = array('income' => 0, 'expenses' => 0, 'personal' => 0, 'net' => 0);
      }
      $yearly[$year]['income'] += $m['income'];
      $yearly[$year]['expenses'] += $m['expenses'];
      $yearly[$year]['personal'] += $m['personal'];
      $yearly[$year]['net'] += $m['net'];

      $net_class = $m['net'] >= 0 ? 'dropfinance-positive' : 'dropfinance-negative';
      $hist_rows[] = array(
        $m['label'],
        '$' . number_format($m['income'], 2),
        '$' . number_format($m['expenses'], 2),
        '$' . number_format($m['personal'], 2),
        array('data' => ($m['net'] >= 0 ? '+$' : '-$') . number_format(abs($m['net']), 2), 'class' => array($net_class)),
      );
    }

    // Add yearly summary rows.
    $year_rows = array();
    foreach ($yearly as $year => $totals) {
      $net_class = $totals['net'] >= 0 ? 'dropfinance-positive' : 'dropfinance-negative';
      $year_rows[] = array(
        'data' => array(
          array('data' => '<strong>' . $year . ' Total</strong>'),
          array('data' => '<strong>$' . number_format($totals['income'], 2) . '</strong>'),
          array('data' => '<strong>$' . number_format($totals['expenses'], 2) . '</strong>'),
          array('data' => '<strong>$' . number_format($totals['personal'], 2) . '</strong>'),
          array('data' => '<strong>' . ($totals['net'] >= 0 ? '+$' : '-$') . number_format(abs($totals['net']), 2) . '</strong>', 'class' => array($net_class)),
        ),
        'class' => array('dropfinance-totals-row'),
      );
    }

    $output['hist_yearly_title'] = array(
      '#markup' => '<h3>' . t('Annual Summary') . '</h3>',
      '#weight' => -7.2,
    );
    $output['hist_yearly'] = array(
      '#theme'  => 'table',
      '#header' => array(t('Year'), t('Income'), t('Business Expenses'), t('Personal'), t('Net')),
      '#rows'   => $year_rows,
      '#attributes' => array('class' => array('dropfinance-projection-table')),
      '#weight' => -7.1,
    );

    $output['hist_help'] = array(
      '#markup' => '<div class="dropfinance-help-text">'
        . t('Calculated from imported bank transactions. Income = all credits. Business expenses = non-personal debits. Personal = debits marked personal. Net = income minus all spending.')
        . '</div>',
      '#weight' => -7.05,
    );

    // Collapsible monthly detail.
    $output['hist_detail'] = array(
      '#type' => 'fieldset',
      '#title' => t('Monthly detail'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#weight' => -7.0,
    );
    $output['hist_detail']['table'] = array(
      '#theme'  => 'table',
      '#header' => array(t('Month'), t('Income'), t('Business Expenses'), t('Personal'), t('Net')),
      '#rows'   => array_reverse($hist_rows),
      '#attributes' => array('class' => array('dropfinance-projection-table')),
    );

    // Pass profitability data to JS.
    $js_data['profitability'] = array(
      'labels'   => array(),
      'income'   => array(),
      'expenses' => array(),
      'personal' => array(),
      'net'      => array(),
    );
    foreach ($hist_data as $m) {
      $js_data['profitability']['labels'][]   = $m['label'];
      $js_data['profitability']['income'][]   = round($m['income'], 2);
      $js_data['profitability']['expenses'][] = round($m['expenses'], 2);
      $js_data['profitability']['personal'][] = round($m['personal'], 2);
      $js_data['profitability']['net'][]      = round($m['net'], 2);
    }
  }

  // Transaction category donut charts (last 12 months).
  $twelve_months_ago = strtotime('-12 months', REQUEST_TIME);
  $has_transactions = (bool) db_select('drop_transaction', 't')
    ->condition('t.date', $twelve_months_ago, '>=')
    ->countQuery()
    ->execute()
    ->fetchField();

  if ($has_transactions) {
    // Where money goes: business debits by category.
    $expense_cats = db_query("
      SELECT category, SUM(amount) AS total
      FROM {drop_transaction}
      WHERE type = 'debit' AND is_personal = 0 AND date >= :since AND category != ''
      GROUP BY category
      ORDER BY total DESC
    ", array(':since' => $twelve_months_ago))->fetchAllKeyed();

    // Where money comes from: non-personal credits in income categories.
    $income_cats = db_query("
      SELECT category, SUM(amount) AS total
      FROM {drop_transaction}
      WHERE type = 'credit' AND is_personal = 0 AND category LIKE 'Client Payments%' AND date >= :since
      GROUP BY category
      ORDER BY total DESC
    ", array(':since' => $twelve_months_ago))->fetchAllKeyed();

    $output['donut_title'] = array(
      '#markup' => '<h2>' . t('Transaction Breakdown (Last 12 Months)') . '</h2>',
      '#weight' => -7,
    );

    $output['donuts'] = array(
      '#markup' => '<div class="dropfinance-charts dropfinance-donuts">'
        . '<div class="dropfinance-chart-wrapper">'
        . '<canvas id="dropfinance-expense-donut" height="300"></canvas>'
        . '</div>'
        . '<div class="dropfinance-chart-wrapper">'
        . '<canvas id="dropfinance-income-donut" height="300"></canvas>'
        . '</div>'
        . '</div>',
      '#weight' => -6,
    );

    $output['donut_help'] = array(
      '#markup' => '<div class="dropfinance-help-text">'
        . t('Based on categorized bank transactions from the last 12 months. Business expenses exclude personal transactions. Categories are assigned by matching rules during CSV import.')
        . '</div>',
      '#weight' => -5.9,
    );

    // All expenses by category (business + personal) for bar chart.
    $all_expense_cats = db_query("
      SELECT CASE WHEN is_personal = 1 THEN CONCAT(category, ' (personal)') ELSE category END AS cat_label,
             is_personal,
             SUM(amount) AS total
      FROM {drop_transaction}
      WHERE type = 'debit' AND date >= :since AND category != ''
      GROUP BY cat_label, is_personal
      ORDER BY total DESC
    ", array(':since' => $twelve_months_ago))->fetchAll();

    $cat_labels = array();
    $cat_values = array();
    $cat_colors = array();
    foreach ($all_expense_cats as $row) {
      $cat_labels[] = $row->cat_label;
      $cat_values[] = (float) $row->total;
      // Orange for personal, blue for business.
      $cat_colors[] = $row->is_personal ? 'rgba(239, 108, 0, 0.7)' : 'rgba(21, 101, 192, 0.7)';
    }

    $output['expense_bar_title'] = array(
      '#markup' => '<h2>' . t('Expenses by Category (Last 12 Months)') . '</h2>',
      '#weight' => -5.8,
    );

    $canvas_height = max(300, count($cat_labels) * 28);
    $output['expense_bar'] = array(
      '#markup' => '<div class="dropfinance-charts">'
        . '<div class="dropfinance-chart-wrapper" style="grid-column: 1 / -1; min-height: ' . $canvas_height . 'px;">'
        . '<canvas id="dropfinance-expense-bar" height="' . $canvas_height . '"></canvas>'
        . '</div>'
        . '</div>',
      '#weight' => -5.7,
    );

    $js_data['expenseBar'] = array(
      'labels' => $cat_labels,
      'values' => $cat_values,
      'colors' => $cat_colors,
    );

    // Pass donut data to JS.
    $js_data['expenseDonut'] = array(
      'labels' => array_keys($expense_cats),
      'values' => array_values(array_map('floatval', $expense_cats)),
    );
    $js_data['incomeDonut'] = array(
      'labels' => array_keys($income_cats),
      'values' => array_values(array_map('floatval', $income_cats)),
    );
  }

  // Revenue breakdown by client.
  $revenue_breakdown = _dropfinance_revenue_breakdown();
  if (!empty($revenue_breakdown)) {
    $rev_rows = array();
    foreach ($revenue_breakdown as $item) {
      $rev_rows[] = array(
        check_plain($item['client']),
        check_plain($item['label']),
        '$' . number_format($item['amount'], 2),
        $item['frequency'],
        '$' . number_format($item['monthly'], 2),
      );
    }
    $output['rev_title'] = array(
      '#markup' => '<h2>' . t('Revenue Breakdown') . '</h2>',
      '#weight' => 0,
    );
    $output['rev_table'] = array(
      '#theme'  => 'table',
      '#header' => array(t('Client'), t('Label'), t('Amount'), t('Frequency'), t('Monthly Equiv.')),
      '#rows'   => $rev_rows,
      '#attributes' => array('class' => array('dropfinance-breakdown-table')),
      '#weight' => 1,
    );
    $output['rev_help'] = array(
      '#markup' => '<div class="dropfinance-help-text">'
        . t('Active revenue lines used in projections. Monthly equivalent converts quarterly (÷3) and annual (÷12) amounts. Manage on the !link page.', array('!link' => l(t('Revenue'), 'drop/finance/revenue')))
        . '</div>',
      '#weight' => 1.5,
    );
  }

  // Expense breakdown grouped by category.
  $expense_breakdown = _dropfinance_expense_breakdown();
  if (!empty($expense_breakdown)) {
    // Group by category.
    $by_category = array();
    foreach ($expense_breakdown as $item) {
      $cat = !empty($item['category']) ? $item['category'] : $item['source'];
      if (!isset($by_category[$cat])) {
        $by_category[$cat] = array('monthly' => 0, 'items' => array());
      }
      $by_category[$cat]['monthly'] += $item['monthly'];
      $by_category[$cat]['items'][] = $item['label'];
    }
    // Sort by monthly total descending.
    uasort($by_category, function ($a, $b) {
      return $b['monthly'] <=> $a['monthly'];
    });

    $exp_rows = array();
    $exp_grand_total = 0;
    foreach ($by_category as $cat => $data) {
      $exp_grand_total += $data['monthly'];
      $exp_rows[] = array(
        check_plain($cat),
        check_plain(implode(', ', $data['items'])),
        '$' . number_format($data['monthly'], 2),
      );
    }
    // Totals row.
    $exp_rows[] = array(
      'data' => array(
        array('data' => '<strong>' . t('Total') . '</strong>'),
        '',
        array('data' => '<strong>$' . number_format($exp_grand_total, 2) . '</strong>'),
      ),
      'class' => array('dropfinance-totals-row'),
    );

    $output['exp_title'] = array(
      '#markup' => '<h2>' . t('Expense Breakdown') . '</h2>',
      '#weight' => 5,
    );
    $output['exp_table'] = array(
      '#theme'  => 'table',
      '#header' => array(t('Category'), t('Items'), t('Monthly Total')),
      '#rows'   => $exp_rows,
      '#attributes' => array('class' => array('dropfinance-breakdown-table')),
      '#weight' => 6,
    );
    $output['exp_help'] = array(
      '#markup' => '<div class="dropfinance-help-text">'
        . t('Active expense lines grouped by category, sorted by monthly cost. Manage individual items on the !link page.', array('!link' => l(t('Expenses'), 'drop/finance/expenses')))
        . '</div>',
      '#weight' => 6.5,
    );
  }

  // Monthly totals summary.
  $monthly_income = _dropfinance_total_monthly_income();
  $monthly_expenses = _dropfinance_total_monthly_expenses();
  $monthly_net = $monthly_income - $monthly_expenses - $avg_personal;
  $net_class = $monthly_net >= 0 ? 'dropfinance-positive' : 'dropfinance-negative';

  $output['summary'] = array(
    '#markup' => '<div class="dropfinance-summary">'
      . '<span class="dropfinance-summary-item">' . t('Monthly income: <strong>$@amount</strong>', array('@amount' => number_format($monthly_income, 2))) . '</span>'
      . '<span class="dropfinance-summary-item">' . t('Monthly expenses: <strong>$@amount</strong>', array('@amount' => number_format($monthly_expenses, 2))) . '</span>'
      . '<span class="dropfinance-summary-item">' . t('Personal: <strong>$@amount</strong>', array('@amount' => number_format($avg_personal, 2))) . '</span>'
      . '<span class="dropfinance-summary-item ' . $net_class . '">' . t('Monthly net: <strong>@sign$@amount</strong>', array('@sign' => $monthly_net >= 0 ? '+' : '-', '@amount' => number_format(abs($monthly_net), 2))) . '</span>'
      . '</div>',
    '#weight' => -15,
  );

  // Attach all chart data to JS settings.
  $output['#attached']['js'][] = array(
    'data' => array('dropfinance' => $js_data),
    'type' => 'setting',
  );
  $output['#attached']['js'][] = $module_path . '/js/dropfinance.charts.js';

  return $output;
}

/**
 * Build projection data.
 *
 * @param float $starting_balance
 *   The current balance.
 * @param int $months
 *   Number of months to project.
 * @param float $personal
 *   Average monthly personal spending.
 *
 * @return array
 *   Array of month data with keys: label, income, expenses, personal, net, balance.
 */
function _dropfinance_build_projection($starting_balance, $months = 6, $personal = 0) {
  $projection = array();
  $balance = $starting_balance;

  // Load active revenue and expense lines.
  $revenues = db_select('drop_revenue', 'r')
    ->fields('r')
    ->condition('r.active', 1)
    ->execute()
    ->fetchAll();

  $expenses = db_select('drop_expense', 'e')
    ->fields('e')
    ->condition('e.active', 1)
    ->execute()
    ->fetchAll();

  // Load pipeline items keyed by month.
  $pipeline_by_month = array();
  $pipeline_details_by_month = array();
  if (db_table_exists('drop_revenue_pipeline')) {
    $pipeline_items = db_query("
      SELECT p.month, p.amount, p.description, r.label AS client
      FROM {drop_revenue_pipeline} p
      INNER JOIN {drop_revenue} r ON r.rid = p.rid
      ORDER BY p.month, r.label
    ")->fetchAll();
    foreach ($pipeline_items as $item) {
      if (!isset($pipeline_by_month[$item->month])) {
        $pipeline_by_month[$item->month] = 0;
        $pipeline_details_by_month[$item->month] = array();
      }
      $pipeline_by_month[$item->month] += (float) $item->amount;
      $detail = $item->client;
      if (!empty($item->description)) {
        $detail .= ': ' . $item->description;
      }
      $detail .= ' ($' . number_format((float) $item->amount, 2) . ')';
      $pipeline_details_by_month[$item->month][] = $detail;
    }
  }

  for ($i = 0; $i < $months; $i++) {
    $month_ts = strtotime("+{$i} months", REQUEST_TIME);
    $month_start = strtotime(date('Y-m-01', $month_ts));
    $month_end = strtotime(date('Y-m-t', $month_ts) . ' 23:59:59');
    $label = date('M Y', $month_ts);
    $month_key = date('Y-m', $month_ts);

    // Sum recurring income for this month.
    $recurring_income = 0;
    foreach ($revenues as $rev) {
      $recurring_income += _dropfinance_line_amount_for_month($rev, $month_start, $month_end);
    }

    // Pipeline project revenue for this month.
    $pipeline_income = 0;
    $pipeline_details = array();
    if (isset($pipeline_by_month[$month_key])) {
      $pipeline_income = $pipeline_by_month[$month_key];
      $pipeline_details = $pipeline_details_by_month[$month_key];
    }

    $income = $recurring_income + $pipeline_income;

    // Sum expenses for this month.
    $exp_total = 0;
    foreach ($expenses as $exp) {
      $exp_total += _dropfinance_line_amount_for_month($exp, $month_start, $month_end);
    }

    $net = $income - $exp_total - $personal;
    $balance += $net;

    $projection[] = array(
      'label'            => $label,
      'income'           => $income,
      'recurring_income'  => $recurring_income,
      'pipeline_income'   => $pipeline_income,
      'pipeline_details'  => $pipeline_details,
      'expenses'         => $exp_total,
      'personal'         => $personal,
      'net'              => $net,
      'balance'          => $balance,
    );
  }

  return $projection;
}

/**
 * Calculate a revenue/expense line's contribution to a given month.
 *
 * @param object $line
 *   A revenue or expense line object.
 * @param int $month_start
 *   Unix timestamp of the first day of the month.
 * @param int $month_end
 *   Unix timestamp of the last moment of the month.
 *
 * @return float
 *   Dollar amount this line contributes to this month.
 */
function _dropfinance_line_amount_for_month($line, $month_start, $month_end) {
  // Check if this line is active during this month.
  if ($line->start_date > $month_end) {
    return 0; // Hasn't started yet.
  }
  if (!empty($line->end_date) && $line->end_date < $month_start) {
    return 0; // Already ended.
  }

  $amount = (float) $line->amount;
  $frequency = $line->frequency;

  switch ($frequency) {
    case 'once':
      // One-time items only count in their start month.
      $line_month = date('Y-m', $line->start_date);
      $this_month = date('Y-m', $month_start);
      return ($line_month === $this_month) ? $amount : 0;

    case 'quarterly':
      // Quarterly: full amount in quarter-start months (Jan, Apr, Jul, Oct).
      $month_num = (int) date('n', $month_start);
      return in_array($month_num, array(1, 4, 7, 10)) ? $amount : 0;

    case 'annual':
      // Annual: contributes 1/12 each month.
      return $amount / 12.0;

    case 'monthly':
    default:
      return $amount;
  }
}

/**
 * Returns revenue breakdown for the dashboard.
 *
 * @return array
 *   Array of items with client, label, amount, frequency, monthly keys.
 */
function _dropfinance_revenue_breakdown() {
  $items = array();

  $revenues = db_select('drop_revenue', 'r')
    ->fields('r')
    ->condition('r.active', 1)
    ->orderBy('r.label')
    ->execute()
    ->fetchAll();

  // Load client names.
  $client_names = array();
  if (db_table_exists('crm_contact')) {
    $client_names = db_query("SELECT cid, name FROM {crm_contact} WHERE type = 'crm_organization'")
      ->fetchAllKeyed();
  }

  $freq_options = dropfinance_frequency_options();

  foreach ($revenues as $rev) {
    $client = !empty($rev->cid) && isset($client_names[$rev->cid])
      ? $client_names[$rev->cid] : '—';
    $monthly = (float) $rev->amount * _dropfinance_monthly_multiplier($rev->frequency);

    $items[] = array(
      'client'    => $client,
      'label'     => $rev->label,
      'amount'    => (float) $rev->amount,
      'frequency' => isset($freq_options[$rev->frequency]) ? $freq_options[$rev->frequency] : $rev->frequency,
      'monthly'   => $monthly,
    );
  }

  return $items;
}

/**
 * Returns expense breakdown for the dashboard.
 *
 * @return array
 *   Array of items with source, label, amount, frequency, monthly keys.
 */
function _dropfinance_expense_breakdown() {
  $items = array();

  $expenses = db_select('drop_expense', 'e')
    ->fields('e')
    ->condition('e.active', 1)
    ->orderBy('e.label')
    ->execute()
    ->fetchAll();

  // Load vendor names.
  $vendor_names = db_query("SELECT vid, name FROM {drop_vendor}")->fetchAllKeyed();
  $freq_options = dropfinance_frequency_options();

  foreach ($expenses as $exp) {
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

    $monthly = (float) $exp->amount * _dropfinance_monthly_multiplier($exp->frequency);

    $items[] = array(
      'source'    => $source,
      'category'  => !empty($exp->category) ? $exp->category : $source,
      'label'     => $exp->label,
      'amount'    => (float) $exp->amount,
      'frequency' => isset($freq_options[$exp->frequency]) ? $freq_options[$exp->frequency] : $exp->frequency,
      'monthly'   => $monthly,
    );
  }

  return $items;
}

/**
 * Calculate total monthly income from active revenue lines.
 *
 * @return float
 */
function _dropfinance_total_monthly_income() {
  $total = 0;
  $revenues = db_select('drop_revenue', 'r')
    ->fields('r', array('amount', 'frequency'))
    ->condition('r.active', 1)
    ->execute();
  foreach ($revenues as $rev) {
    $total += (float) $rev->amount * _dropfinance_monthly_multiplier($rev->frequency);
  }
  return $total;
}

/**
 * Calculate total monthly expenses from active expense lines.
 *
 * @return float
 */
function _dropfinance_total_monthly_expenses() {
  $total = 0;
  $expenses = db_select('drop_expense', 'e')
    ->fields('e', array('amount', 'frequency'))
    ->condition('e.active', 1)
    ->execute();
  foreach ($expenses as $exp) {
    $total += (float) $exp->amount * _dropfinance_monthly_multiplier($exp->frequency);
  }
  return $total;
}

/**
 * Calculate monthly profitability from bank transactions.
 *
 * Returns income (credits) vs business expenses (debits, non-personal)
 * vs personal spending grouped by month, ordered chronologically.
 *
 * @return array
 *   Array of month data with keys: label, income, expenses, personal, net.
 */
function _dropfinance_monthly_profitability() {
  $months = array();

  // Income by month (non-personal credits in income categories).
  $income = db_query("
    SELECT DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m') AS ym,
           SUM(amount) AS total
    FROM {drop_transaction}
    WHERE type = 'credit' AND is_personal = 0 AND category LIKE 'Client Payments%'
    GROUP BY ym
    ORDER BY ym
  ");
  foreach ($income as $row) {
    $months[$row->ym]['income'] = (float) $row->total;
  }

  // Business expenses by month (debits minus refund credits, non-personal).
  $expenses = db_query("
    SELECT DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m') AS ym,
           SUM(CASE WHEN type = 'debit' THEN amount ELSE -amount END) AS total
    FROM {drop_transaction}
    WHERE is_personal = 0 AND category NOT LIKE 'Loan%' AND (type = 'debit' OR (type = 'credit' AND category NOT LIKE 'Client Payments%'))
    GROUP BY ym
    ORDER BY ym
  ");
  foreach ($expenses as $row) {
    $months[$row->ym]['expenses'] = (float) $row->total;
  }

  // Personal spending by month (debits minus credits, personal).
  $personal_debits = db_query("
    SELECT DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m') AS ym,
           SUM(amount) AS total
    FROM {drop_transaction}
    WHERE type = 'debit' AND is_personal = 1
    GROUP BY ym
    ORDER BY ym
  ");
  foreach ($personal_debits as $row) {
    $months[$row->ym]['personal'] = (float) $row->total;
  }

  // Personal refunds reduce personal spending.
  $personal_credits = db_query("
    SELECT DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m') AS ym,
           SUM(amount) AS total
    FROM {drop_transaction}
    WHERE type = 'credit' AND is_personal = 1
    GROUP BY ym
    ORDER BY ym
  ");
  foreach ($personal_credits as $row) {
    if (!isset($months[$row->ym]['personal'])) {
      $months[$row->ym]['personal'] = 0;
    }
    $months[$row->ym]['personal'] -= (float) $row->total;
  }

  // Build result array.
  ksort($months);
  $result = array();
  foreach ($months as $ym => $data) {
    $income_val = isset($data['income']) ? $data['income'] : 0;
    $expense_val = isset($data['expenses']) ? $data['expenses'] : 0;
    $personal_val = isset($data['personal']) ? $data['personal'] : 0;
    $result[] = array(
      'label'    => date('M Y', strtotime($ym . '-01')),
      'income'   => $income_val,
      'expenses' => $expense_val,
      'personal' => $personal_val,
      'net'      => $income_val - $expense_val - $personal_val,
    );
  }

  return $result;
}

/**
 * Calculate average monthly personal spending from bank transactions.
 *
 * @return float
 *   Average monthly personal spending.
 */
function _dropfinance_avg_monthly_personal() {
  $result = db_query("
    SELECT COUNT(DISTINCT DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m')) AS month_count,
           SUM(CASE WHEN type = 'debit' THEN amount ELSE -amount END) AS total
    FROM {drop_transaction}
    WHERE is_personal = 1
  ")->fetchObject();

  if (empty($result->month_count) || $result->month_count == 0) {
    return 0;
  }
  return max(0, (float) $result->total / (int) $result->month_count);
}

// ---------------------------------------------------------------------------
// How It Works (documentation)
// ---------------------------------------------------------------------------

/**
 * Page callback: documentation page explaining how Drop Finance works.
 */
function dropfinance_help_page() {
  backdrop_set_title(t('How It Works'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
  ));

  $output = array();
  $output['#attached']['css'][] = backdrop_get_path('module', 'dropfinance') . '/dropfinance.css';

  $sections = array();

  // Data sources.
  $sections[] = '<h2>' . t('Data Sources') . '</h2>'
    . '<dl class="dropfinance-help-dl">'
    . '<dt>' . t('Bank Transactions') . '</dt>'
    . '<dd>' . t('Imported from bank CSV exports. Each transaction is categorized by matching rules and marked as business or personal. This is the source of truth for historical profitability and the P&L report.') . '</dd>'
    . '<dt>' . t('Revenue Lines') . '</dt>'
    . '<dd>' . t('Expected recurring income. Initially seeded from 12-month averages of paid FreshBooks invoices per client. Can be edited manually. Used only for the forward-looking projection.') . '</dd>'
    . '<dt>' . t('Expense Lines') . '</dt>'
    . '<dd>' . t('Expected recurring costs. Initially seeded from 6-month averages of bank transaction patterns per vendor. Can be edited manually. Used only for the forward-looking projection.') . '</dd>'
    . '<dt>' . t('FreshBooks Invoices') . '</dt>'
    . '<dd>' . t('Imported invoice history. Powers the Client Revenue Report and was used to generate initial revenue line amounts.') . '</dd>'
    . '</dl>';

  // Transaction classification.
  $sections[] = '<h2>' . t('Transaction Classification') . '</h2>'
    . '<dl class="dropfinance-help-dl">'
    . '<dt>' . t('Matching Rules') . '</dt>'
    . '<dd>' . t('When bank transactions are imported, each description is matched against a list of patterns (configured in Settings). The first matching rule assigns a vendor name, category, and business/personal flag. Rules are evaluated top-down.') . '</dd>'
    . '<dt>' . t('Business vs. Personal') . '</dt>'
    . '<dd>' . t('Transactions are flagged as business or personal. Business debits appear as expenses. Personal debits appear under "Personal" in reports. This separation drives the profitability calculations.') . '</dd>'
    . '</dl>';

  // How refunds work.
  $sections[] = '<h2>' . t('Refunds & Credits') . '</h2>'
    . '<p>' . t('Not all bank credits are income. Credits are classified as follows:') . '</p>'
    . '<dl class="dropfinance-help-dl">'
    . '<dt>' . t('Income') . '</dt>'
    . '<dd>' . t('Credits in categories starting with "Client Payments" (e.g., Client Payments, Client Payments - FreshBooks, Client Payments - Other). These are real revenue.') . '</dd>'
    . '<dt>' . t('Business refunds') . '</dt>'
    . '<dd>' . t('Non-personal credits in any other category (e.g., a $20 OpenAI credit in "Software"). These are treated as negative business expenses — they reduce the expense total in their category rather than showing as income.') . '</dd>'
    . '<dt>' . t('Personal refunds') . '</dt>'
    . '<dd>' . t('Credits marked as personal (e.g., a Prime Video refund). These reduce the personal spending total in their category.') . '</dd>'
    . '<dt>' . t('Loan transactions') . '</dt>'
    . '<dd>' . t('Categories starting with "Loan" (Loan Proceeds, Loan Repayment) are excluded from P&L calculations entirely. These are balance sheet movements, not income or expenses. Stripe Capital financing fees are tracked separately as "Financing Fees" business expenses.') . '</dd>'
    . '</dl>';

  // Dashboard.
  $sections[] = '<h2>' . t('Dashboard Sections') . '</h2>'
    . '<dl class="dropfinance-help-dl">'
    . '<dt>' . t('Cash Flow Projection') . '</dt>'
    . '<dd>' . t('Forward-looking estimate based on active revenue lines, expense lines, and average monthly personal spending. Personal spending is calculated as the monthly average from all imported bank transactions. Quarterly items land in their calendar quarter months (Jan, Apr, Jul, Oct).') . '</dd>'
    . '<dt>' . t('Monthly Profitability') . '</dt>'
    . '<dd>' . t('Historical view from actual bank transactions. Shows income (non-personal credits), business expenses (non-personal debits), personal spending (personal debits minus personal credits), and net.') . '</dd>'
    . '<dt>' . t('Transaction Breakdown') . '</dt>'
    . '<dd>' . t('Donut charts showing the last 12 months of categorized bank transactions. "Where Money Goes" shows business expense categories. "Where Money Comes From" shows income categories.') . '</dd>'
    . '<dt>' . t('Revenue & Expense Breakdowns') . '</dt>'
    . '<dd>' . t('Tables showing active revenue and expense lines used in the projection. These are editable estimates, not historical data.') . '</dd>'
    . '</dl>';

  // P&L.
  $sections[] = '<h2>' . t('Profit & Loss Report') . '</h2>'
    . '<p>' . t('A month-by-month breakdown of actual income and expenses from bank transactions, organized by category. Shows three sections: Income (non-personal credits), Business Expenses (non-personal debits), and Personal (personal debits net of personal credits/refunds). Categories are sorted by total amount within each section.') . '</p>';

  // Frequencies.
  $sections[] = '<h2>' . t('Frequency Handling') . '</h2>'
    . '<dl class="dropfinance-help-dl">'
    . '<dt>' . t('Monthly') . '</dt>'
    . '<dd>' . t('Full amount applied every month.') . '</dd>'
    . '<dt>' . t('Quarterly') . '</dt>'
    . '<dd>' . t('Full amount applied in calendar quarter start months: January, April, July, October. Shows as zero in other months. Monthly equivalent in breakdowns divides by 3.') . '</dd>'
    . '<dt>' . t('Annual') . '</dt>'
    . '<dd>' . t('Spread evenly across all months (amount ÷ 12 per month).') . '</dd>'
    . '<dt>' . t('One-time') . '</dt>'
    . '<dd>' . t('Applied only in the start date month.') . '</dd>'
    . '</dl>';

  // Duplicate detection.
  $sections[] = '<h2>' . t('CSV Import & Duplicate Detection') . '</h2>'
    . '<p>' . t('Bank CSV imports use a SHA-256 hash of each row (date, description, amount, type, check number) to detect duplicates. If two transactions are identical on the same day, a counter suffix is added to the hash. Re-importing the same CSV will skip already-imported rows.') . '</p>';

  $output['content'] = array(
    '#markup' => '<div class="dropfinance-help-page">' . implode('', $sections) . '</div>',
  );

  return $output;
}

// ---------------------------------------------------------------------------
// Profit & Loss
// ---------------------------------------------------------------------------

/**
 * Page callback: Profit & Loss statement from bank transactions.
 *
 * Shows income and expenses by category by month, most recent first.
 */
function dropfinance_pnl_page() {
  backdrop_set_title(t('Profit & Loss'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
  ));

  $module_path = backdrop_get_path('module', 'dropfinance');
  $output = array();
  $output['#attached']['css'][] = $module_path . '/dropfinance.css';

  // Determine number of months to show (default 12).
  $num_months = isset($_GET['months']) && in_array($_GET['months'], array('6', '12', '24')) ? (int) $_GET['months'] : 12;

  // Toggle links.
  $toggles = array();
  foreach (array(6, 12, 24) as $opt) {
    $toggles[] = $num_months == $opt
      ? '<strong>' . t('@n months', array('@n' => $opt)) . '</strong>'
      : l(t('@n months', array('@n' => $opt)), 'drop/finance/pnl', array('query' => array('months' => $opt)));
  }

  $output['title_bar'] = array(
    '#markup' => '<div class="dropfinance-pnl-toggle">' . implode(' · ', $toggles) . '</div>',
    '#weight' => -10,
  );

  // Build month columns (oldest first, left to right).
  $month_keys = array();
  $month_labels = array();
  for ($i = $num_months - 1; $i >= 0; $i--) {
    $ts = strtotime("-{$i} months", REQUEST_TIME);
    $key = date('Y-m', $ts);
    $month_keys[] = $key;
    $month_labels[$key] = date('M Y', $ts);
  }

  // Query all transactions within range.
  $earliest = strtotime(reset($month_keys) . '-01');
  $latest = strtotime(date('Y-m-t', REQUEST_TIME) . ' 23:59:59');

  // Income by category and month (non-personal credits in income categories).
  $income_data = db_query("
    SELECT COALESCE(NULLIF(category, ''), 'Uncategorized') AS category,
           DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m') AS ym,
           SUM(amount) AS total
    FROM {drop_transaction}
    WHERE type = 'credit' AND is_personal = 0 AND category LIKE 'Client Payments%'
      AND date >= :earliest AND date <= :latest
    GROUP BY category, ym
  ", array(':earliest' => $earliest, ':latest' => $latest))->fetchAll();

  $income_cats = array();
  $income_totals = array(); // per-category grand total
  foreach ($income_data as $row) {
    $income_cats[$row->category][$row->ym] = (float) $row->total;
    if (!isset($income_totals[$row->category])) {
      $income_totals[$row->category] = 0;
    }
    $income_totals[$row->category] += (float) $row->total;
  }
  arsort($income_totals);

  // Revenue adjustment for Stripe loan repayments.
  // Client revenue withheld by Stripe to repay loans never appears as Client
  // Payments in the bank. Add it back as an income adjustment.
  $loan_adj_cat = t('Revenue Adjustment (for Stripe Loan)');
  $loan_repay_data = db_query("
    SELECT DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m') AS ym,
           SUM(amount) AS total
    FROM {drop_transaction}
    WHERE category = 'Loan Repayment' AND type = 'debit'
      AND date >= :earliest AND date <= :latest
    GROUP BY ym
  ", array(':earliest' => $earliest, ':latest' => $latest))->fetchAll();
  if (!empty($loan_repay_data)) {
    $income_cats[$loan_adj_cat] = array();
    $income_totals[$loan_adj_cat] = 0;
    foreach ($loan_repay_data as $row) {
      $income_cats[$loan_adj_cat][$row->ym] = (float) $row->total;
      $income_totals[$loan_adj_cat] += (float) $row->total;
    }
  }

  // Business expenses by category and month (debits minus refund credits).
  $biz_data = db_query("
    SELECT COALESCE(NULLIF(category, ''), 'Uncategorized') AS category,
           DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m') AS ym,
           SUM(CASE WHEN type = 'debit' THEN amount ELSE -amount END) AS total
    FROM {drop_transaction}
    WHERE is_personal = 0 AND category NOT LIKE 'Loan%'
      AND (type = 'debit' OR (type = 'credit' AND category NOT LIKE 'Client Payments%'))
      AND date >= :earliest AND date <= :latest
    GROUP BY category, ym
  ", array(':earliest' => $earliest, ':latest' => $latest))->fetchAll();

  $biz_cats = array();
  $biz_totals = array();
  foreach ($biz_data as $row) {
    $biz_cats[$row->category][$row->ym] = (float) $row->total;
    if (!isset($biz_totals[$row->category])) {
      $biz_totals[$row->category] = 0;
    }
    $biz_totals[$row->category] += (float) $row->total;
  }
  arsort($biz_totals);

  // Personal expenses by category and month (debits minus credits).
  $personal_data = db_query("
    SELECT COALESCE(NULLIF(category, ''), 'Uncategorized') AS category,
           DATE_FORMAT(FROM_UNIXTIME(date), '%Y-%m') AS ym,
           SUM(CASE WHEN type = 'debit' THEN amount ELSE -amount END) AS total
    FROM {drop_transaction}
    WHERE is_personal = 1 AND date >= :earliest AND date <= :latest
    GROUP BY category, ym
  ", array(':earliest' => $earliest, ':latest' => $latest))->fetchAll();

  $personal_cats = array();
  $personal_totals = array();
  foreach ($personal_data as $row) {
    $personal_cats[$row->category][$row->ym] = (float) $row->total;
    if (!isset($personal_totals[$row->category])) {
      $personal_totals[$row->category] = 0;
    }
    $personal_totals[$row->category] += (float) $row->total;
  }
  arsort($personal_totals);

  // Build the table header.
  $header = array(t('Category'));
  foreach ($month_keys as $key) {
    $header[] = $month_labels[$key];
  }
  $header[] = t('Total');

  // Helper to build a data row.
  $build_row = function ($cat, $cat_data, $class = '', $link_category = NULL) use ($month_keys) {
    $filter_cat = $link_category !== NULL ? $link_category : $cat;
    $cat_link = l($cat, 'drop/finance/transactions', array('query' => array('category' => $filter_cat)));
    $row_data = array(array('data' => $cat_link));
    $row_total = 0;
    foreach ($month_keys as $key) {
      $val = isset($cat_data[$key]) ? $cat_data[$key] : 0;
      $row_total += $val;
      $row_data[] = $val > 0 ? '$' . number_format($val, 0) : '—';
    }
    $row_data[] = '$' . number_format($row_total, 0);
    $row = array('data' => $row_data);
    if ($class) {
      $row['class'] = array($class);
    }
    return $row;
  };

  // Helper to build a totals row.
  $build_totals_row = function ($label, $all_cats, $class = '') use ($month_keys) {
    $row_data = array(array('data' => '<strong>' . $label . '</strong>'));
    $grand = 0;
    foreach ($month_keys as $key) {
      $col_total = 0;
      foreach ($all_cats as $cat_data) {
        $col_total += isset($cat_data[$key]) ? $cat_data[$key] : 0;
      }
      $grand += $col_total;
      $row_data[] = array('data' => '<strong>$' . number_format($col_total, 0) . '</strong>');
    }
    $row_data[] = array('data' => '<strong>$' . number_format($grand, 0) . '</strong>');
    $row = array('data' => $row_data, 'class' => array('dropfinance-totals-row'));
    if ($class) {
      $row['class'][] = $class;
    }
    return $row;
  };

  $rows = array();

  // --- Income section ---
  $rows[] = array(
    'data' => array(array('data' => '<strong>' . t('Income') . '</strong>', 'colspan' => count($header))),
    'class' => array('dropfinance-pnl-section'),
  );
  foreach (array_keys($income_totals) as $cat) {
    $link_cat = ($cat === $loan_adj_cat) ? 'Loan Repayment' : NULL;
    $rows[] = $build_row($cat, $income_cats[$cat], '', $link_cat);
  }
  $rows[] = $build_totals_row(t('Total Income'), $income_cats);

  // --- Business Expenses section ---
  $rows[] = array(
    'data' => array(array('data' => '<strong>' . t('Business Expenses') . '</strong>', 'colspan' => count($header))),
    'class' => array('dropfinance-pnl-section'),
  );
  foreach (array_keys($biz_totals) as $cat) {
    $rows[] = $build_row($cat, $biz_cats[$cat]);
  }
  $rows[] = $build_totals_row(t('Total Business Expenses'), $biz_cats);

  // --- Personal section ---
  if (!empty($personal_cats)) {
    $rows[] = array(
      'data' => array(array('data' => '<strong>' . t('Personal') . '</strong>', 'colspan' => count($header))),
      'class' => array('dropfinance-pnl-section'),
    );
    foreach (array_keys($personal_totals) as $cat) {
      $rows[] = $build_row($cat, $personal_cats[$cat]);
    }
    $rows[] = $build_totals_row(t('Total Personal'), $personal_cats);
  }

  // --- Net row ---
  $net_data = array(array('data' => '<strong>' . t('Net Income') . '</strong>'));
  $net_grand = 0;
  foreach ($month_keys as $key) {
    $inc = 0;
    foreach ($income_cats as $cat_data) {
      $inc += isset($cat_data[$key]) ? $cat_data[$key] : 0;
    }
    $biz = 0;
    foreach ($biz_cats as $cat_data) {
      $biz += isset($cat_data[$key]) ? $cat_data[$key] : 0;
    }
    $pers = 0;
    foreach ($personal_cats as $cat_data) {
      $pers += isset($cat_data[$key]) ? $cat_data[$key] : 0;
    }
    $net = $inc - $biz - $pers;
    $net_grand += $net;
    $net_class = $net >= 0 ? 'dropfinance-positive' : 'dropfinance-negative';
    $sign = $net >= 0 ? '$' : '-$';
    $net_data[] = array('data' => '<strong>' . $sign . number_format(abs($net), 0) . '</strong>', 'class' => array($net_class));
  }
  $net_class = $net_grand >= 0 ? 'dropfinance-positive' : 'dropfinance-negative';
  $sign = $net_grand >= 0 ? '$' : '-$';
  $net_data[] = array('data' => '<strong>' . $sign . number_format(abs($net_grand), 0) . '</strong>', 'class' => array($net_class));
  $rows[] = array('data' => $net_data, 'class' => array('dropfinance-totals-row', 'dropfinance-pnl-net'));

  $output['table'] = array(
    '#theme'  => 'table',
    '#header' => $header,
    '#rows'   => $rows,
    '#attributes' => array('class' => array('dropfinance-pnl-table')),
    '#weight' => 0,
  );

  $output['help'] = array(
    '#markup' => '<div class="dropfinance-help-text">'
      . t('All amounts from imported bank transactions. Income = credits. Business expenses = non-personal debits. Personal = debits marked personal. Sorted by total within each section.')
      . '</div>',
    '#weight' => 1,
  );

  return $output;
}

// ---------------------------------------------------------------------------
// Import (existing functionality)
// ---------------------------------------------------------------------------

/**
 * Page callback: import landing page with links to import tools.
 */
function dropfinance_import_page() {
  backdrop_set_title(t('Import'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Finance'), 'drop/finance'),
  ));

  $items = array(
    array(
      'data' => l(t('Bank CSV'), 'drop/finance/import/csv'),
      'children' => array(t('Import bank transactions from a CSV export. Auto-categorizes using matching rules.')),
    ),
    array(
      'data' => l(t('FreshBooks Invoices'), 'drop/finance/import/freshbooks'),
      'children' => array(t('Import invoice history from a FreshBooks CSV export. Creates revenue lines and enables client revenue reporting.')),
    ),
    array(
      'data' => l(t('Support Site (CRM)'), 'drop/finance/import/crm'),
      'children' => array(t('Pull organization and project data from the remote support site API.')),
    ),
  );

  return array(
    '#theme' => 'item_list',
    '#items' => $items,
    '#attributes' => array('class' => array('dropfinance-import-list')),
  );
}

/**
 * Form: Import data from the remote support site.
 */
function dropfinance_import_form($form, &$form_state) {
  $config = config('dropfinance.settings');
  $source_url = $config->get('source_url');
  $api_key = $config->get('api_key');

  if (empty($source_url) || empty($api_key)) {
    $form['notice'] = array(
      '#markup' => '<p>' . t('Please <a href="@url">configure the source URL and API key</a> before importing.', array('@url' => url('drop/finance/settings'))) . '</p>',
    );
    return $form;
  }

  $form['info'] = array(
    '#markup' => '<p>' . t('Pull organization and project data from <code>@url</code>. Existing records will be updated; new records will be created.', array('@url' => $source_url)) . '</p>',
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Pull from support site'),
  );

  return $form;
}

/**
 * Submit callback: fetch and import data from the remote API.
 */
function dropfinance_import_form_submit($form, &$form_state) {
  $config = config('dropfinance.settings');
  $base_url = rtrim($config->get('source_url'), '/');
  $key = $config->get('api_key');

  $request_url = $base_url . '/drop/api/crm?key=' . urlencode($key);
  $response = backdrop_http_request($request_url);

  if ($response->code != 200) {
    backdrop_set_message(t('Failed to fetch data from the source site. HTTP @code.', array('@code' => $response->code)), 'error');
    return;
  }

  $data = json_decode($response->data, TRUE);
  if (empty($data)) {
    backdrop_set_message(t('Received empty or invalid response from the source site.'), 'error');
    return;
  }

  $stats = array(
    'org_new' => 0,
    'org_updated' => 0,
    'project_new' => 0,
    'project_updated' => 0,
  );

  // Import organizations.
  if (!empty($data['organizations'])) {
    foreach ($data['organizations'] as $org) {
      $cid = (int) $org['cid'];
      $existing = db_query("SELECT cid, changed FROM {crm_contact} WHERE cid = :cid", array(':cid' => $cid))->fetchObject();

      if ($existing) {
        // Update if source is newer.
        if ((int) $org['changed'] > (int) $existing->changed) {
          db_update('crm_contact')
            ->fields(array(
              'name'    => $org['name'],
              'status'  => (int) $org['status'],
              'changed' => (int) $org['changed'],
            ))
            ->condition('cid', $cid)
            ->execute();
          $stats['org_updated']++;
        }
      }
      else {
        db_insert('crm_contact')
          ->fields(array(
            'cid'     => $cid,
            'type'    => 'crm_organization',
            'name'    => $org['name'],
            'status'  => (int) $org['status'],
            'uid'     => 1,
            'created' => REQUEST_TIME,
            'changed' => (int) $org['changed'],
          ))
          ->execute();
        $stats['org_new']++;
      }
    }
    // Reset the auto-increment to avoid conflicts with future imports.
    $max_cid = (int) db_query("SELECT MAX(cid) FROM {crm_contact}")->fetchField();
    if ($max_cid) {
      db_query("ALTER TABLE {crm_contact} AUTO_INCREMENT = " . ($max_cid + 1));
    }
  }

  // Import projects.
  if (!empty($data['projects'])) {
    foreach ($data['projects'] as $project) {
      $pid = (int) $project['pid'];
      $existing = db_query("SELECT pid, changed FROM {drop_project} WHERE pid = :pid", array(':pid' => $pid))->fetchObject();

      if ($existing) {
        if ((int) $project['changed'] > (int) $existing->changed) {
          db_update('drop_project')
            ->fields(array(
              'name'     => $project['name'],
              'active'   => (int) $project['active'],
              'cid'      => !empty($project['cid']) ? (int) $project['cid'] : NULL,
              'prefix'   => $project['prefix'],
              'site_url' => $project['site_url'],
              'changed'  => (int) $project['changed'],
            ))
            ->condition('pid', $pid)
            ->execute();
          $stats['project_updated']++;
        }
      }
      else {
        db_insert('drop_project')
          ->fields(array(
            'pid'      => $pid,
            'name'     => $project['name'],
            'active'   => (int) $project['active'],
            'cid'      => !empty($project['cid']) ? (int) $project['cid'] : NULL,
            'prefix'   => $project['prefix'],
            'site_url' => $project['site_url'],
            'uid'      => 1,
            'created'  => REQUEST_TIME,
            'changed'  => (int) $project['changed'],
          ))
          ->execute();
        $stats['project_new']++;
      }
    }
    $max_pid = (int) db_query("SELECT MAX(pid) FROM {drop_project}")->fetchField();
    if ($max_pid) {
      db_query("ALTER TABLE {drop_project} AUTO_INCREMENT = " . ($max_pid + 1));
    }
  }

  backdrop_set_message(t('Import complete. Organizations: @new new, @updated updated. Projects: @pnew new, @pupdated updated.', array(
    '@new'      => $stats['org_new'],
    '@updated'  => $stats['org_updated'],
    '@pnew'     => $stats['project_new'],
    '@pupdated' => $stats['project_updated'],
  )));

  // Clear entity caches so imported data is visible immediately.
  entity_get_controller('crm_contact')->resetCache();
  entity_get_controller('drop_project')->resetCache();
}
