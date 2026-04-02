<?php
/**
 * @file
 * Admin settings for Drop Email.
 */

/**
 * Form: Drop Email settings.
 */
function dropemail_settings_form($form, &$form_state) {
  $config = config('dropemail.settings');

  $form['#config'] = 'dropemail.settings';

  $form['from_address'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Default From address'),
    '#default_value' => $config->get('from_address'),
    '#required'      => TRUE,
    '#description'   => t('The default sender email address (e.g. support@example.com). Senders may override this per email.'),
  );

  $form['from_name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Default From name'),
    '#default_value' => $config->get('from_name'),
    '#description'   => t('The sender name shown in email clients (e.g. "Acme Support").'),
  );

  return system_settings_form($form);
}

/**
 * Validation for dropemail_settings_form().
 */
function dropemail_settings_form_validate($form, &$form_state) {
  $address = trim($form_state['values']['from_address']);
  if (!valid_email_address($address)) {
    form_set_error('from_address', t('Please enter a valid email address.'));
  }
}
