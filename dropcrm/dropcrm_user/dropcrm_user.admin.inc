<?php
/**
 * @file
 * Admin forms for DropCRM User: create-contact and change-link forms.
 */

// ---------------------------------------------------------------------------
// Create CRM contact from user account
// ---------------------------------------------------------------------------

/**
 * Form: create a new CRM contact linked to a user account.
 *
 * Pre-fills first/last name from the username (best-guess split) and email
 * from the account. The admin can adjust before saving.
 *
 * @param array $form
 * @param array $form_state
 * @param object $account
 *   The Backdrop user account to link.
 *
 * @return array
 */
function dropcrm_user_create_contact_form($form, &$form_state, $account) {
  $form_state['dropcrm_user_account'] = $account;

  backdrop_set_title(t('Create CRM contact for %name', array('%name' => $account->name)));

  // Best-guess first/last name split from username.
  $parts = explode(' ', trim($account->name), 2);
  $default_first = $parts[0];
  $default_last  = isset($parts[1]) ? $parts[1] : '';

  $form['first_name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('First name'),
    '#default_value' => $default_first,
    '#required'      => TRUE,
    '#maxlength'     => 128,
  );

  $form['last_name'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Last name'),
    '#default_value' => $default_last,
    '#maxlength'     => 128,
  );

  $form['email'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Email address'),
    '#default_value' => $account->mail,
    '#description'   => t('Will be saved as the primary email contact method.'),
    '#maxlength'     => 254,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Create contact'),
  );
  $form['actions']['cancel'] = array(
    '#type'       => 'link',
    '#title'      => t('Cancel'),
    '#href'       => 'user/' . $account->uid,
  );

  return $form;
}

/**
 * Submit handler: create CRM contact and link to the user account.
 */
function dropcrm_user_create_contact_form_submit($form, &$form_state) {
  $account    = $form_state['dropcrm_user_account'];
  $first_name = trim($form_state['values']['first_name']);
  $last_name  = trim($form_state['values']['last_name']);
  $email      = trim($form_state['values']['email']);

  $contact = _dropcrm_user_create_contact_for_user($account, $first_name, $last_name, $email);

  if ($contact) {
    backdrop_set_message(t('CRM contact %name has been created and linked to %user.',
      array(
        '%name' => entity_label('crm_contact', $contact),
        '%user' => $account->name,
      )
    ));
  }
  else {
    backdrop_set_message(t('There was a problem creating the CRM contact.'), 'error');
  }

  $form_state['redirect'] = 'user/' . $account->uid;
}

// ---------------------------------------------------------------------------
// Change (or remove) the linked CRM contact
// ---------------------------------------------------------------------------

/**
 * Form: link a user account to an existing CRM contact, or remove the link.
 *
 * @param array $form
 * @param array $form_state
 * @param object $account
 *   The Backdrop user account.
 *
 * @return array
 */
function dropcrm_user_link_contact_form($form, &$form_state, $account) {
  $form_state['dropcrm_user_account'] = $account;

  backdrop_set_title(t('Change CRM contact link for %name', array('%name' => $account->name)));

  $current_contact = dropcrm_user_get_contact($account->uid);

  if ($current_contact) {
    $form['current'] = array(
      '#type'   => 'item',
      '#title'  => t('Currently linked contact'),
      '#markup' => l(entity_label('crm_contact', $current_contact), 'drop/crm/contact/' . $current_contact->cid),
    );
  }

  $form['contact_autocomplete'] = array(
    '#type'              => 'textfield',
    '#title'             => $current_contact ? t('Link to a different contact') : t('Link to an existing contact'),
    '#autocomplete_path' => 'drop/crm/autocomplete/contacts',
    '#description'       => t('Type a contact name to search. Leave blank to only remove the current link.'),
    '#maxlength'         => 512,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save link'),
  );

  if ($current_contact) {
    $form['actions']['remove'] = array(
      '#type'                    => 'submit',
      '#value'                   => t('Remove link'),
      '#submit'                  => array('dropcrm_user_link_contact_form_remove'),
      '#limit_validation_errors' => array(),
    );
  }

  $form['actions']['cancel'] = array(
    '#type'  => 'link',
    '#title' => t('Cancel'),
    '#href'  => 'user/' . $account->uid,
  );

  return $form;
}

/**
 * Validate handler: ensure the autocomplete value resolves to a valid contact.
 */
function dropcrm_user_link_contact_form_validate($form, &$form_state) {
  $value = trim($form_state['values']['contact_autocomplete']);
  if ($value === '') {
    // Blank is allowed (nothing will change).
    return;
  }

  $cid = _dropcrm_user_parse_autocomplete_cid($value);
  if (!$cid) {
    form_set_error('contact_autocomplete', t('Could not find a matching contact. Please select from the autocomplete suggestions.'));
    return;
  }

  $contact = crm_contact_load($cid);
  if (!$contact) {
    form_set_error('contact_autocomplete', t('The selected contact could not be loaded.'));
    return;
  }

  // Check the target contact isn't already linked to a different user.
  $existing_uid = db_select('crm_user_contact', 'uc')
    ->fields('uc', array('uid'))
    ->condition('cid', $cid)
    ->execute()
    ->fetchField();

  $account = $form_state['dropcrm_user_account'];
  if ($existing_uid && $existing_uid != $account->uid) {
    $linked_user = user_load($existing_uid);
    form_set_error('contact_autocomplete', t('That contact is already linked to user %name.',
      array('%name' => $linked_user ? $linked_user->name : '#' . $existing_uid)
    ));
  }
}

/**
 * Submit handler: save the new contact link.
 */
function dropcrm_user_link_contact_form_submit($form, &$form_state) {
  $account = $form_state['dropcrm_user_account'];
  $value   = trim($form_state['values']['contact_autocomplete']);

  if ($value === '') {
    backdrop_set_message(t('No changes made.'));
    $form_state['redirect'] = 'user/' . $account->uid;
    return;
  }

  $cid = _dropcrm_user_parse_autocomplete_cid($value);
  dropcrm_user_link($account->uid, $cid);

  $contact = crm_contact_load($cid);
  backdrop_set_message(t('CRM contact %name has been linked to %user.',
    array(
      '%name' => entity_label('crm_contact', $contact),
      '%user' => $account->name,
    )
  ));

  $form_state['redirect'] = 'user/' . $account->uid;
}

/**
 * Submit handler: remove the contact link entirely.
 */
function dropcrm_user_link_contact_form_remove($form, &$form_state) {
  $account = $form_state['dropcrm_user_account'];
  dropcrm_user_unlink($account->uid);
  backdrop_set_message(t('The CRM contact link for %user has been removed.', array('%user' => $account->name)));
  $form_state['redirect'] = 'user/' . $account->uid;
}

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

/**
 * Parses a CID from an autocomplete value in "Name [cid:N]" format.
 *
 * @param string $value
 *   The autocomplete field value.
 *
 * @return int|FALSE
 *   The CID, or FALSE if not found.
 */
function _dropcrm_user_parse_autocomplete_cid($value) {
  if (preg_match('/\[cid:(\d+)\]/', $value, $matches)) {
    return (int) $matches[1];
  }
  return FALSE;
}
