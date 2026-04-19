<?php
/**
 * @file
 * Setup wizard page callbacks, step implementations, and shared rendering.
 */

// ---------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------

/**
 * Page callback: drop/start/step/{key}.
 *
 * Accepts either the short step key ('welcome') or the namespaced full key
 * ('drop_start:welcome'). The short form is what users type and bookmark;
 * the namespaced form is what the registry uses internally.
 */
function drop_start_wizard_step_page($key) {
  $full_key = _drop_start_resolve_step_key($key);
  $steps    = drop_start_get_steps();

  if (!$full_key || !isset($steps[$full_key])) {
    backdrop_not_found();
    return;
  }

  // Remember where we are so /drop/start resumes here.
  $state = drop_start_wizard_state_get();
  if ($state['current'] !== $full_key) {
    $state['current'] = $full_key;
    drop_start_wizard_state_set($state);
  }

  $step = $steps[$full_key];
  backdrop_set_title(t('Setup wizard: !title', array('!title' => $step['title'])));

  return backdrop_get_form('drop_start_wizard_step_form', $full_key);
}

/**
 * Page callback: drop/start/step/{key}/skip.
 *
 * Marks the step as skipped and advances. Non-required steps only —
 * required steps 404.
 */
function drop_start_wizard_step_skip($key) {
  $full_key = _drop_start_resolve_step_key($key);
  $steps    = drop_start_get_steps();

  if (!$full_key || !isset($steps[$full_key])) {
    backdrop_not_found();
    return;
  }
  if (!empty($steps[$full_key]['required'])) {
    backdrop_set_message(t('This step is required and cannot be skipped.'), 'warning');
    backdrop_goto('drop/start/step/' . $steps[$full_key]['_key']);
    return;
  }

  $next = drop_start_wizard_advance($full_key, TRUE);
  backdrop_set_message(t('Skipped %step.', array('%step' => $steps[$full_key]['title'])));
  backdrop_goto($next ? _drop_start_step_path($next) : 'drop/start');
}

/**
 * Page callback: drop/start/restart.
 *
 * Clears wizard state and returns to the overview.
 */
function drop_start_wizard_restart() {
  state_del('drop_start_wizard');
  backdrop_static_reset('drop_start_get_steps');
  backdrop_set_message(t('Setup wizard reset. Starting over.'));
  backdrop_goto('drop/start');
}

/**
 * Resolves a URL step key (e.g. 'welcome' or 'drop_start:welcome') to a
 * full namespaced key. Accepts either form; returns NULL on no match.
 *
 * Short keys match the first step (by weight) whose '_key' matches. This
 * means two modules declaring the same short key compete, but the earlier-
 * weighted one wins — acceptable for MVP.
 */
function _drop_start_resolve_step_key($key) {
  if (strpos($key, ':') !== FALSE) {
    $steps = drop_start_get_steps();
    return isset($steps[$key]) ? $key : NULL;
  }
  foreach (drop_start_get_steps() as $full_key => $step) {
    if ($step['_key'] === $key) {
      return $full_key;
    }
  }
  return NULL;
}

/**
 * Returns the canonical URL path for a step key.
 */
function _drop_start_step_path($full_key) {
  $steps = drop_start_get_steps();
  if (!isset($steps[$full_key])) {
    return 'drop/start';
  }
  return 'drop/start/step/' . $steps[$full_key]['_key'];
}

// ---------------------------------------------------------------------------
// Wizard form — adds step body + universal Next/Skip navigation
// ---------------------------------------------------------------------------

/**
 * Form builder: wraps a step's body callback with wizard navigation.
 */
function drop_start_wizard_step_form($form, &$form_state, $full_key) {
  $steps = drop_start_get_steps();
  $step  = $steps[$full_key];

  $form['#drop_start_full_key'] = $full_key;

  // Progress indicator.
  $form['progress'] = array(
    '#markup' => _drop_start_wizard_progress_bar($full_key),
    '#weight' => -100,
  );

  // Step body — the step's callback returns a subform.
  $body = array();
  if (!empty($step['callback']) && function_exists($step['callback'])) {
    $body = call_user_func_array($step['callback'], array(array(), &$form_state));
  }
  if (!is_array($body)) {
    $body = array('#markup' => (string) $body);
  }
  $form['body'] = $body + array('#weight' => 0);

  // If the step has a per-step submit handler ({callback}_submit), wire it
  // in before the main wizard submit. The step handler can abort advancement
  // by setting $form_state['drop_start_no_advance'] = TRUE.
  $handlers = array();
  if (!empty($step['callback']) && function_exists($step['callback'] . '_submit')) {
    $handlers[] = $step['callback'] . '_submit';
  }
  $handlers[] = 'drop_start_wizard_step_form_submit';
  $form['#submit'] = $handlers;

  // Universal navigation actions.
  $form['actions'] = array('#type' => 'actions', '#weight' => 100);
  $form['actions']['next'] = array(
    '#type'  => 'submit',
    '#value' => _drop_start_wizard_next_label($full_key),
  );
  if (empty($step['required'])) {
    $form['actions']['skip'] = array(
      '#type'  => 'markup',
      '#markup' => ' ' . l(t('Skip this step'), 'drop/start/step/' . $step['_key'] . '/skip', array(
        'attributes' => array('class' => array('drop-start-wizard-skip')),
      )),
    );
  }
  $form['actions']['exit'] = array(
    '#markup' => ' ' . l(t('Exit wizard'), 'drop/start', array(
      'attributes' => array('class' => array('drop-start-wizard-exit')),
    )),
  );

  return $form;
}

/**
 * Submit handler: advances state and redirects to the next step.
 */
function drop_start_wizard_step_form_submit($form, &$form_state) {
  $full_key = $form['#drop_start_full_key'];

  // Steps with a custom submit handler can set
  // $form_state['drop_start_no_advance'] = TRUE to stay on the same step
  // (e.g. after a sub-action that needs confirmation).
  if (!empty($form_state['drop_start_no_advance'])) {
    return;
  }

  $next = drop_start_wizard_advance($full_key, FALSE);
  $form_state['redirect'] = $next ? _drop_start_step_path($next) : 'drop/start';
}

/**
 * Returns the label for the Next button: "Continue" for most steps,
 * "Finish" if this is the last step.
 */
function _drop_start_wizard_next_label($full_key) {
  $keys = array_keys(drop_start_get_steps());
  $pos  = array_search($full_key, $keys, TRUE);
  if ($pos === count($keys) - 1) {
    return t('Finish');
  }
  return t('Continue');
}

/**
 * Returns a simple progress indicator for the current step.
 */
function _drop_start_wizard_progress_bar($full_key) {
  $steps = drop_start_get_steps();
  $keys  = array_keys($steps);
  $total = count($keys);
  $pos   = array_search($full_key, $keys, TRUE);
  if ($pos === FALSE) {
    return '';
  }
  $pct = $total > 1 ? round((($pos + 1) / $total) * 100) : 100;

  $items = array();
  foreach ($keys as $i => $fk) {
    $label = $steps[$fk]['title'];
    $class = array('drop-start-step-pip');
    if ($i === $pos) {
      $class[] = 'drop-start-step-pip--current';
    }
    elseif ($i < $pos) {
      $class[] = 'drop-start-step-pip--done';
    }
    $items[] = '<li class="' . implode(' ', $class) . '">'
      . l($label, 'drop/start/step/' . $steps[$fk]['_key'])
      . '</li>';
  }

  return '<div class="drop-start-wizard-progress">'
    . '<p class="drop-start-wizard-progress-label">' . t('Step @n of @total', array('@n' => $pos + 1, '@total' => $total)) . '</p>'
    . '<ul class="drop-start-wizard-pips">' . implode('', $items) . '</ul>'
    . '</div>';
}

// ---------------------------------------------------------------------------
// Step: Welcome
// ---------------------------------------------------------------------------

function drop_start_step_welcome($form, &$form_state) {
  $subform = array();
  $subform['intro'] = array(
    '#markup' => '<h2>' . t('Welcome to Drop Suite') . '</h2>'
      . '<p>' . t('Drop Suite is a bundle of modules that turns your Backdrop site into a productivity hub — tickets, CRM, tasks, goals, time tracking, and more.') . '</p>'
      . '<p>' . t('This wizard walks you through the essentials. Each step is optional except for Welcome and Done. You can skip anything you want to handle later, and you can re-run the wizard at any time from Settings.') . '</p>'
      . '<p>' . t('Click %continue to begin.', array('%continue' => t('Continue'))) . '</p>',
  );
  return $subform;
}

function drop_start_step_welcome_status() {
  $state = drop_start_wizard_state_get();
  return in_array('drop_start:welcome', $state['completed'], TRUE) ? 'configured' : 'not_started';
}

// ---------------------------------------------------------------------------
// Step: Theme
// ---------------------------------------------------------------------------

function drop_start_step_theme($form, &$form_state) {
  $subform = array();

  $themes   = list_themes();
  $current  = config_get('system.core', 'theme_default');
  $opera    = isset($themes['opera']) ? $themes['opera'] : NULL;
  $enabled  = $opera && !empty($opera->status);

  $subform['intro'] = array(
    '#markup' => '<h2>' . t('Choose a theme') . '</h2>'
      . '<p>' . t('Backdrop\'s default %basis theme is functional but dated. We recommend the %opera theme for a cleaner look on Drop Suite admin pages.', array(
        '%basis' => 'Basis',
        '%opera' => 'Opera',
      )) . '</p>'
      . '<p>' . t('Currently active default theme: %current', array('%current' => $current)) . '</p>',
  );

  if ($current === 'opera') {
    // State A: already using Opera.
    $subform['ok'] = array(
      '#markup' => '<p><strong>' . t('✓ Your site is already using Opera.') . '</strong> ' . t('Click Continue to proceed.') . '</p>',
    );
  }
  elseif ($opera) {
    // State B: Opera installed but not active. Offer to switch.
    $subform['switch'] = array(
      '#markup' => '<p>' . t('Opera is installed on this site.') . '</p>',
    );
    $subform['do_switch'] = array(
      '#type' => 'checkbox',
      '#title' => t('Switch this site to the Opera theme'),
      '#description' => $enabled
        ? t('Opera is already enabled; this will set it as the default theme.')
        : t('This will enable the Opera theme and set it as the default.'),
      '#default_value' => TRUE,
    );
  }
  else {
    // State C: Opera not installed. Provide instructions.
    $subform['missing'] = array(
      '#markup' => '<p>' . t('The Opera theme is not installed on this site yet. To install it:') . '</p>'
        . '<ol>'
        . '<li>' . t('Visit the !install_link page on this site.', array(
          '!install_link' => l(t('Install new themes'), 'admin/appearance/install'),
        )) . '</li>'
        . '<li>' . t('Paste the URL or upload the Opera theme archive.') . '</li>'
        . '<li>' . t('Return here and re-open this step to switch to it.') . '</li>'
        . '</ol>'
        . '<p>' . t('You can also skip this step and change themes later from %path.', array(
          '%path' => 'admin/appearance',
        )) . '</p>',
    );
  }

  return $subform;
}

function drop_start_step_theme_status() {
  $current = config_get('system.core', 'theme_default');
  if ($current === 'opera') {
    return 'configured';
  }
  return 'not_started';
}

/**
 * Custom submit handler: perform the theme switch if requested, then
 * advance normally.
 */
function drop_start_step_theme_submit($form, &$form_state) {
  if (!empty($form_state['values']['do_switch'])) {
    $themes = list_themes();
    if (isset($themes['opera'])) {
      if (empty($themes['opera']->status)) {
        theme_enable(array('opera'));
      }
      config_set('system.core', 'theme_default', 'opera');
      backdrop_set_message(t('Switched the default theme to Opera.'));
    }
    else {
      backdrop_set_message(t('Opera theme is not available. Install it first.'), 'warning');
      $form_state['drop_start_no_advance'] = TRUE;
    }
  }
}

// ---------------------------------------------------------------------------
// Step: Done
// ---------------------------------------------------------------------------

function drop_start_step_done($form, &$form_state) {
  $subform = array();
  $state = drop_start_wizard_state_get();
  $steps = drop_start_get_steps();

  $done_items = array();
  foreach ($steps as $full_key => $step) {
    if ($full_key === 'drop_start:done' || $full_key === 'drop_start:welcome') {
      continue;
    }
    $status = drop_start_step_status($full_key);
    $badge = array(
      'configured'  => '<span class="drop-start-badge drop-start-badge--done">' . t('Done') . '</span>',
      'partial'     => '<span class="drop-start-badge drop-start-badge--partial">' . t('Partial') . '</span>',
      'not_started' => '<span class="drop-start-badge drop-start-badge--skip">' . t('Skipped') . '</span>',
    );
    $done_items[] = '<li>' . $badge[$status] . ' ' . check_plain($step['title']) . '</li>';
  }

  $subform['summary'] = array(
    '#markup' => '<h2>' . t('All set') . '</h2>'
      . '<p>' . t('Here\'s what the wizard covered:') . '</p>'
      . '<ul class="drop-start-done-list">' . implode('', $done_items) . '</ul>',
  );

  // Re-run sample content prompt if it was skipped or not configured.
  if (!in_array('drop_start:sample_content', $state['completed'], TRUE)) {
    $subform['resample'] = array(
      '#markup' => '<p>' . t('You can still populate demo content any time from !link.', array(
        '!link' => l(t('Drop Start'), 'drop/start/generate'),
      )) . '</p>',
    );
  }

  $subform['next_steps'] = array(
    '#markup' => '<h3>' . t('Where to next?') . '</h3>'
      . '<ul>'
      . '<li>' . l(t('Dashboard'), 'drop') . '</li>'
      . '<li>' . l(t('Tickets'), 'drop/tickets') . '</li>'
      . '<li>' . l(t('Settings'), 'drop/settings') . '</li>'
      . '</ul>',
  );

  return $subform;
}

function drop_start_step_done_status() {
  $state = drop_start_wizard_state_get();
  return in_array('drop_start:done', $state['completed'], TRUE) ? 'configured' : 'not_started';
}
