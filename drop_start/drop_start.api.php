<?php
/**
 * @file
 * Hooks provided by Drop Start.
 */

/**
 * Declare setup-wizard steps contributed by this module.
 *
 * Drop Start exposes a multi-step setup wizard at drop/start/step/{key}.
 * Each module in the Drop Suite family can contribute steps by implementing
 * this hook. Steps appear in the wizard sequence in ascending weight order.
 *
 * Return an array keyed by step key (unique within the declaring module —
 * the orchestrator namespaces the key internally so two modules can declare
 * different steps with the same key if they want).
 *
 * Each entry supports the following fields:
 *   - title: (string) Short human-readable label shown in the wizard nav
 *     and the overview page.
 *   - weight: (int) Sort order. Lower = earlier in the sequence. Welcome
 *     uses -100, Done uses 100.
 *   - module: (string, optional) Which module "owns" the step. Defaults to
 *     the declaring module. Used for grouping and disable detection.
 *   - callback: (string) Function that returns a render array (or form
 *     builder function name) for the step page. Signature:
 *     function my_step_callback(&$form_state = NULL) — optional form_state
 *     for steps that are forms.
 *   - status_callback: (string, optional) Function returning one of
 *     'configured', 'partial', 'not_started'. Drives the overview page's
 *     summary rendering and enables skip-already-done behavior. Defaults
 *     to 'not_started' if omitted.
 *   - required: (bool, optional) If TRUE, the step cannot be skipped.
 *     Defaults to FALSE.
 *   - description: (string, optional) One-paragraph explanation shown on
 *     the overview card for the step.
 *
 * @return array
 *   Array of step definitions.
 *
 * @see drop_start_get_steps()
 */
function hook_drop_start_steps() {
  return array(
    'welcome' => array(
      'title' => t('Welcome'),
      'weight' => -100,
      'callback' => 'drop_start_step_welcome',
      'status_callback' => 'drop_start_step_welcome_status',
      'required' => TRUE,
      'description' => t('What Drop Suite is and what this wizard will set up.'),
    ),
    'sample_content' => array(
      'title' => t('Sample content'),
      'weight' => -50,
      'callback' => 'drop_start_step_sample_content',
      'status_callback' => 'drop_start_step_sample_content_status',
      'description' => t('Populate the site with demo data you can explore.'),
    ),
  );
}
