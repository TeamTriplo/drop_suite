<?php
/**
 * @file
 * Setup wizard page callbacks, step implementations, and shared rendering.
 */

// ---------------------------------------------------------------------------
// Progress block (persistent reminder, rendered on every page until setup
// is complete).
// ---------------------------------------------------------------------------

/**
 * Returns the HTML body for the wizard-progress block, or empty when the
 * wizard should render nothing.
 *
 * @param string $overall
 *   'not_started' | 'in_progress' | 'complete'.
 */
function _drop_start_wizard_progress_block_html($overall) {
  // Attach the block's styles. Also loaded on wizard pages — no conflict,
  // backdrop_add_css() deduplicates.
  backdrop_add_css(backdrop_get_path('module', 'drop_start') . '/drop_start.wizard.css');

  $state = drop_start_wizard_state_get();
  $steps = drop_start_get_steps();
  if (empty($steps)) {
    return '';
  }

  // Pick the step to resume from: current if set, otherwise first.
  $resume_key = !empty($state['current']) && isset($steps[$state['current']])
    ? $state['current']
    : array_keys($steps)[0];

  $resume_step = $steps[$resume_key];
  $resume_path = _drop_start_step_path($resume_key);

  // Progress: X of Y.
  $all_keys = array_keys($steps);
  $total    = count($all_keys);
  $done     = count($state['completed']) + count($state['skipped']);

  $heading = $overall === 'not_started'
    ? t('Drop Suite setup wizard')
    : t('Drop Suite setup wizard — in progress');

  $status = $overall === 'not_started'
    ? t('Click Start to begin.')
    : t('Step @n of @total: %title', array(
        '@n'     => array_search($resume_key, $all_keys, TRUE) + 1,
        '@total' => $total,
        '%title' => $resume_step['title'],
      ));

  $resume_label = $overall === 'not_started' ? t('Start setup') : t('Return to wizard');

  $html  = '<div class="drop-start-progress-block">';
  $html .= '<div class="drop-start-progress-block-head">';
  $html .= '<strong>' . check_plain($heading) . '</strong>';
  $html .= ' — ' . $status;
  $html .= '</div>';
  $html .= '<div class="drop-start-progress-block-actions">';
  $html .= l($resume_label, $resume_path, array(
    'attributes' => array('class' => array('button', 'button-primary')),
  ));

  // Quick-jump: any remaining steps (not completed, not skipped).
  $remaining = array();
  foreach ($all_keys as $fk) {
    if (in_array($fk, $state['completed'], TRUE)) continue;
    if (in_array($fk, $state['skipped'], TRUE)) continue;
    if ($fk === $resume_key) continue;
    $remaining[$fk] = $steps[$fk]['title'];
  }
  if (!empty($remaining) && $overall === 'in_progress') {
    $links = array();
    foreach ($remaining as $fk => $title) {
      $links[] = l($title, _drop_start_step_path($fk));
    }
    $html .= ' <span class="drop-start-progress-jump">'
      . t('Jump to: ') . implode(' · ', $links)
      . '</span>';
  }

  $html .= '</div>';
  $html .= '</div>';
  return $html;
}

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
 * Page callback: drop/start/find-module/{name}.
 *
 * Pre-populates the installer's text search filter (session-based) and
 * redirects to /admin/modules/install. The admin lands on the installer
 * with the missing module's name already filled in.
 */
function drop_start_installer_search($name) {
  if (!module_exists('installer')) {
    backdrop_set_message(t('The project installer module is not enabled. Enable it at the modules page first.'), 'warning');
    backdrop_goto('drop/start/step/modules');
    return;
  }
  // Installer browser stores the module-tab search filter under this key.
  $_SESSION['installer_browser_text_filter_module'] = $name;
  backdrop_goto('admin/modules/install');
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
  $form['#attached']['css'][] = backdrop_get_path('module', 'drop_start') . '/drop_start.wizard.css';

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
// Step: Sample content
// ---------------------------------------------------------------------------

/**
 * Embeds the existing drop_start_generate_form as a wizard step. The step
 * body has a "Generate now" checkbox controlling whether the submit handler
 * actually runs the generator — otherwise Continue just advances.
 */
function drop_start_step_sample_content($form, &$form_state) {
  $subform = array();

  $subform['intro'] = array(
    '#markup' => '<h2>' . t('Sample content') . '</h2>'
      . '<p>' . t('Drop Start can populate your site with an Olympics event-planning demo. Gives you real tickets, contacts, projects, and more to explore while you set up the rest.') . '</p>',
  );

  $available = drop_start_available_modules();
  if (empty($available)) {
    $subform['empty'] = array(
      '#markup' => '<p><em>' . t('No Drop Suite modules are enabled yet. Come back here after the Modules step to populate demo content.') . '</em></p>',
    );
    return $subform;
  }

  $existing_total = drop_start_content_total();
  if ($existing_total > 0) {
    $subform['existing'] = array(
      '#markup' => '<div class="messages warning"><p>'
        . t('@count demo items already exist on this site. Generating again will add more on top.', array('@count' => $existing_total))
        . '</p></div>',
    );
  }

  $subform['do_generate'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Generate demo content now'),
    '#default_value' => ($existing_total === 0),
    '#description'   => t('Uncheck to skip generation on this step. You can always run it later from the Setup wizard card.'),
  );

  $options = array();
  foreach ($available as $module => $label) {
    $options[$module] = $label;
  }

  $subform['modules'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Generate content for'),
    '#options'       => $options,
    '#default_value' => array_keys($options),
    '#states'        => array(
      'visible' => array(':input[name="do_generate"]' => array('checked' => TRUE)),
    ),
  );

  return $subform;
}

function drop_start_step_sample_content_status() {
  return drop_start_content_total() > 0 ? 'configured' : 'not_started';
}

/**
 * Custom submit handler: run the generator if the user checked the box.
 */
function drop_start_step_sample_content_submit($form, &$form_state) {
  if (empty($form_state['values']['do_generate'])) {
    return;
  }
  $selected = array_filter($form_state['values']['modules']);
  if (empty($selected)) {
    backdrop_set_message(t('No modules selected for demo content. Nothing generated.'), 'warning');
    return;
  }
  module_load_include('inc', 'drop_start', 'drop_start.pages');
  // Reuse the existing generator. It reads $form_state['values']['modules'],
  // so build a minimal state to pass in.
  $sub_state = array('values' => array('modules' => $selected));
  drop_start_generate_form_submit($form, $sub_state);
}

// ---------------------------------------------------------------------------
// Step: Modules
// ---------------------------------------------------------------------------

/**
 * Shows a summary of currently-enabled Drop modules, offers to enable any
 * contrib dependencies already downloaded on disk, and links out to the
 * dedicated modules page for the main enable/disable.
 *
 * We intentionally don't embed the full modules form in-line: enabling
 * modules runs install hooks and rebuilds the menu, which doesn't play
 * nicely inside a wizard subform. What we DO embed is the one operation
 * that's safe and common in a fresh install — enabling already-downloaded
 * contrib dependencies so the scoped modules page can actually enable the
 * Drop modules that need them.
 */
function drop_start_step_modules($form, &$form_state) {
  $subform = array();

  $subform['intro'] = array(
    '#markup' => '<h2>' . t('Enable Drop Suite modules') . '</h2>'
      . '<p>' . t('Pick which Drop Suite modules you want on this site. You can always change your selection later from the same page.') . '</p>',
  );

  $enabled_list = _drop_start_step_modules_enabled();

  if (!empty($enabled_list)) {
    $items = array();
    foreach ($enabled_list as $machine => $info) {
      $items[] = '<strong>' . check_plain($info['name']) . '</strong> <small class="drop-start-module-machine">(' . check_plain($machine) . ')</small>';
    }
    $subform['enabled'] = array(
      '#markup' => '<h3>' . t('Currently enabled') . '</h3>'
        . theme('item_list', array('items' => $items)),
    );
  }
  else {
    $subform['none'] = array(
      '#markup' => '<p><em>' . t('No Drop Suite modules are enabled yet.') . '</em></p>',
    );
  }

  // Survey contrib dependencies of Drop Suite modules and narrate the
  // state so the admin understands what Color Field / Book / Entity Plus
  // / Entity Reference are, why they're listed, and whether they even
  // matter (only needed if the Drop module that depends on them is
  // actually wanted).
  $dep_survey = _drop_start_step_modules_contrib_dep_survey();

  if (!empty($dep_survey['disabled'])) {
    $options = array();
    foreach ($dep_survey['disabled'] as $name => $info) {
      $options[$name] = t('@label — needed if you want: @by', array(
        '@label' => $info['label'],
        '@by'    => implode(', ', array_unique($info['needed_by'])),
      ));
    }
    $subform['deps_on_disk'] = array(
      '#type'        => 'fieldset',
      '#title'       => t('Already downloaded — enable to use'),
      '#description' => t('These contrib modules are already on this site but are turned off. Each one is only needed if you plan to use the Drop Suite module that depends on it. Check the ones you want and click Continue to turn them on — the Drop modules that need them will then be enable-able on the modules page below.'),
    );
    $subform['deps_on_disk']['enable_deps'] = array(
      '#type'          => 'checkboxes',
      '#title'         => t('Enable these modules'),
      '#title_display' => 'invisible',
      '#options'       => $options,
      '#default_value' => array_keys($options),
    );
  }

  if (!empty($dep_survey['missing'])) {
    $installer_available = module_exists('installer');
    $items = array();
    foreach ($dep_survey['missing'] as $name => $info) {
      if ($installer_available) {
        $link = l(t('Find in the project installer'), 'drop/start/find-module/' . $name, array(
          'attributes' => array('target' => '_blank'),
        ));
      }
      else {
        $link = l(t('Find on backdropcms.org'), 'https://backdropcms.org/project/' . $name, array(
          'external'   => TRUE,
          'attributes' => array('target' => '_blank'),
        ));
      }
      $items[] = t('<strong>@label</strong> — needed if you want: @by. !link', array(
        '@label' => $info['label'],
        '@by'    => implode(', ', array_unique($info['needed_by'])),
        '!link'  => $link,
      ));
    }
    $subform['deps_missing'] = array(
      '#type'        => 'fieldset',
      '#title'       => t('Not on this site yet — download if needed'),
      '#description' => $installer_available
        ? t('Some Drop Suite modules depend on separate contrib projects that are not yet on this site. You only need these if you plan to use the dependent Drop module. Each link below opens the project installer with the module name pre-filled in a new tab — install from there, then come back to this step.')
        : t('Some Drop Suite modules depend on separate contrib projects that are not yet on this site. You only need these if you plan to use the dependent Drop module. Find and install each one, then come back to this step.'),
    );
    $subform['deps_missing']['list'] = array(
      '#markup' => theme('item_list', array('items' => $items)),
    );
  }

  if (!empty($dep_survey['enabled']) && empty($dep_survey['disabled']) && empty($dep_survey['missing'])) {
    $subform['deps_ok'] = array(
      '#markup' => '<p><strong>' . t('✓ All contrib dependencies for Drop Suite modules are downloaded and enabled.') . '</strong></p>',
    );
  }

  $subform['go'] = array(
    '#markup' => '<p>'
      . l(t('Open the modules page'), 'drop/settings/modules', array(
          'attributes' => array('class' => array('button', 'button-primary')),
          'query' => backdrop_get_destination(),
        ))
      . '</p>'
      . '<p class="description">' . t('The modules page opens in the same tab. After saving, return here with your browser\'s Back button (or revisit this wizard step) and click Continue.') . '</p>',
  );

  return $subform;
}

/**
 * Custom submit: enable any contrib deps the admin checked, and stay on
 * this step so the re-rendered page reflects the new state (the contrib
 * deps are gone from the "available" list, and the main modules link now
 * unblocks the Drop modules that needed them).
 */
function drop_start_step_modules_submit($form, &$form_state) {
  if (empty($form_state['values']['enable_deps'])) {
    return;
  }
  $to_enable = array_values(array_filter($form_state['values']['enable_deps']));
  if (empty($to_enable)) {
    return;
  }
  if (module_enable($to_enable)) {
    backdrop_set_message(format_plural(count($to_enable),
      'Enabled 1 contrib dependency: @list.',
      'Enabled @count contrib dependencies: @list.',
      array('@list' => implode(', ', $to_enable))
    ));
  }
  // Stay on this step so the admin sees the updated state.
  $form_state['drop_start_no_advance'] = TRUE;
  $form_state['redirect'] = _drop_start_step_path('drop_start:modules');
}

/**
 * Returns enabled Drop Suite modules keyed by machine name.
 */
function _drop_start_step_modules_enabled() {
  $drop_modules = drop_base_get_drop_module_names();
  $all = system_rebuild_module_data();
  $enabled = array();
  foreach (array_keys($drop_modules) as $module) {
    if (!empty($all[$module]) && !empty($all[$module]->status)) {
      $enabled[$module] = array(
        'name' => !empty($all[$module]->info['name']) ? $all[$module]->info['name'] : $module,
      );
    }
  }
  ksort($enabled);
  return $enabled;
}

/**
 * Surveys all non-Drop contrib deps referenced by Drop modules and buckets
 * them by current state:
 *   - enabled:  downloaded AND enabled on this site
 *   - disabled: downloaded but disabled (admin can flip here)
 *   - missing:  not downloaded (admin needs the project installer)
 *
 * Each bucket is keyed by module machine name, with {label, needed_by[]}.
 * 'needed_by' lists the Drop modules that depend on this contrib.
 *
 * Used by the Modules wizard step to narrate the dep situation clearly so
 * admins understand what each contrib is for and whether they even need it.
 */
function _drop_start_step_modules_contrib_dep_survey() {
  $drop_modules = drop_base_get_drop_module_names();
  $all = system_rebuild_module_data();
  $buckets = array('enabled' => array(), 'disabled' => array(), 'missing' => array());

  foreach (array_keys($drop_modules) as $drop_mod) {
    if (empty($all[$drop_mod]) || empty($all[$drop_mod]->requires)) {
      continue;
    }
    $drop_label = !empty($all[$drop_mod]->info['name']) ? $all[$drop_mod]->info['name'] : $drop_mod;
    foreach (array_keys($all[$drop_mod]->requires) as $dep) {
      // Skip inter-Drop dependencies — those aren't contrib concerns.
      if (isset($drop_modules[$dep])) {
        continue;
      }
      // Determine bucket.
      if (empty($all[$dep])) {
        $bucket = 'missing';
        $label  = $dep;
      }
      elseif (!empty($all[$dep]->status)) {
        $bucket = 'enabled';
        $label  = !empty($all[$dep]->info['name']) ? $all[$dep]->info['name'] : $dep;
      }
      else {
        $bucket = 'disabled';
        $label  = !empty($all[$dep]->info['name']) ? $all[$dep]->info['name'] : $dep;
      }
      if (!isset($buckets[$bucket][$dep])) {
        $buckets[$bucket][$dep] = array('label' => $label, 'needed_by' => array());
      }
      $buckets[$bucket][$dep]['needed_by'][] = $drop_label;
    }
  }
  foreach ($buckets as $key => $_) {
    ksort($buckets[$key]);
  }
  return $buckets;
}

// ---------------------------------------------------------------------------
// Step: Home page navigation
// ---------------------------------------------------------------------------
//
// Moved from drop_base in phase 5 of the wizard build (tid 566). The old
// /drop/settings/home-nav route now 301-redirects to this step.

/**
 * The Drop Suite navigation blocks that can be added to the home layout.
 * Keyed by plugin name (module:delta).
 */
function _drop_start_home_nav_blocks() {
  return array(
    'drop_base:drop_nav'         => t('Drop Navigation'),
    'drop_base:drop_section_nav' => t('Drop Section Navigation'),
  );
}

/**
 * Inspects the home layout and returns which Drop nav blocks are present.
 *
 * @return array
 *   - layout: Loaded home Layout object (or FALSE if missing).
 *   - present: plugin => uuid for blocks already in the layout.
 *   - missing: plugin => label for blocks not yet in the layout.
 */
function _drop_start_home_nav_state() {
  $state = array('layout' => FALSE, 'present' => array(), 'missing' => array());
  $layout = layout_load('home');
  if (!$layout) {
    return $state;
  }
  $state['layout'] = $layout;
  $wanted = _drop_start_home_nav_blocks();
  foreach ($layout->content as $uuid => $block) {
    if (isset($block->plugin) && isset($wanted[$block->plugin])) {
      $state['present'][$block->plugin] = $uuid;
    }
  }
  $state['missing'] = array_diff_key($wanted, $state['present']);
  return $state;
}

/**
 * Picks the best region on the home layout to place Drop Suite nav blocks.
 */
function _drop_start_home_nav_target_region($layout) {
  foreach (array('navigation', 'top', 'header') as $preferred) {
    if (isset($layout->positions[$preferred])) {
      return $preferred;
    }
  }
  $keys = array_keys($layout->positions);
  return reset($keys);
}

/**
 * Wizard step: Home page navigation.
 */
function drop_start_step_home_nav($form, &$form_state) {
  $subform = array();
  $state = _drop_start_home_nav_state();

  $subform['intro'] = array(
    '#markup' => '<h2>' . t('Home page navigation') . '</h2>'
      . '<p>' . t('The Backdrop home page uses its own layout, so the Drop Suite primary navigation that appears on the rest of your site is not shown there by default. This step can add the Drop Suite nav blocks to the top of your home layout so people arriving at the home page can jump straight into tickets, tasks, and other Drop modules.') . '</p>'
      . '<p>' . t('You can always fine-tune placement later at <em>Structure → Layouts → Home page</em>.') . '</p>',
  );

  if (!$state['layout']) {
    $subform['no_layout'] = array(
      '#markup' => '<p><strong>' . t('No home layout was found.') . '</strong> ' . t('This site does not appear to have a layout named "home", so there is nothing to update here. Click Continue to move on.') . '</p>',
    );
    return $subform;
  }

  $wanted = _drop_start_home_nav_blocks();
  $rows = array();
  foreach ($wanted as $plugin => $label) {
    $rows[] = array(
      check_plain($label),
      isset($state['present'][$plugin]) ? t('Present') : '<em>' . t('Not added') . '</em>',
    );
  }
  $subform['status'] = array(
    '#markup' => theme('table', array(
      'header' => array(t('Navigation block'), t('Status on home layout')),
      'rows'   => $rows,
    )),
  );

  if (empty($state['missing'])) {
    $subform['already'] = array(
      '#markup' => '<p><strong>' . t('✓ All Drop Suite navigation blocks are already on the home layout.') . '</strong></p>',
    );
    $subform['remove'] = array(
      '#type'        => 'checkbox',
      '#title'       => t('Remove Drop Suite navigation from home page'),
      '#default_value' => FALSE,
      '#description' => t('Check to revert. Leave unchecked and click Continue to keep the current layout.'),
    );
  }
  else {
    $subform['add'] = array(
      '#type'          => 'checkbox',
      '#title'         => t('Add Drop Suite navigation to home page'),
      '#default_value' => TRUE,
      '#description'   => t('Uncheck to skip. You can always return to this step later.'),
    );
  }

  return $subform;
}

/**
 * Status: 'configured' if all blocks present, 'not_started' otherwise.
 */
function drop_start_step_home_nav_status() {
  $state = _drop_start_home_nav_state();
  if (!$state['layout']) {
    // No home layout to configure — count as configured.
    return 'configured';
  }
  return empty($state['missing']) ? 'configured' : 'not_started';
}

/**
 * Custom submit: apply the add/remove choice, if any.
 */
function drop_start_step_home_nav_submit($form, &$form_state) {
  if (!empty($form_state['values']['add'])) {
    _drop_start_home_nav_apply('add');
  }
  elseif (!empty($form_state['values']['remove'])) {
    _drop_start_home_nav_apply('remove');
  }
}

/**
 * Applies the add or remove operation to the home layout.
 */
function _drop_start_home_nav_apply($op) {
  $state = _drop_start_home_nav_state();
  if (!$state['layout']) {
    backdrop_set_message(t('Home layout could not be loaded.'), 'error');
    return;
  }
  $layout = $state['layout'];

  if ($op === 'add') {
    $region = _drop_start_home_nav_target_region($layout);
    $added  = 0;
    foreach (array_keys($state['missing']) as $plugin) {
      list($module, $delta) = explode(':', $plugin, 2);
      $layout->addBlock($module, $delta, $region);
      $added++;
    }
    if ($added) {
      $layout->save();
      backdrop_set_message(t('Added @count Drop Suite navigation block(s) to the home layout.', array('@count' => $added)));
    }
  }
  elseif ($op === 'remove') {
    $removed = 0;
    foreach ($state['present'] as $plugin => $uuid) {
      $layout->removeBlock($uuid);
      $removed++;
    }
    if ($removed) {
      $layout->save();
      backdrop_set_message(t('Removed @count Drop Suite navigation block(s) from the home layout.', array('@count' => $removed)));
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
