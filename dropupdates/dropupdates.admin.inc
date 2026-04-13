<?php
/**
 * @file
 * Admin settings form for DropUpdates.
 */

/**
 * Form builder: DropUpdates settings.
 */
function dropupdates_settings_form($form, &$form_state) {
  $config = config('dropupdates.settings');

  $form['#config'] = 'dropupdates.settings';

  $form['slack'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Slack cross-posting'),
    '#description' => t('When a team member posts an update, a preview can be cross-posted to a Slack channel. Requires the Slack bot token configured in DropBridge settings.'),
  );
  $form['slack']['slack_channel'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Slack channel'),
    '#default_value' => $config->get('slack_channel'),
    '#description'   => t('Channel name (e.g. <code>#team-updates</code>) or channel ID. Leave blank to disable cross-posting.'),
    '#size'          => 40,
  );

  return system_settings_form($form);
}
