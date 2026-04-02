<?php
/**
 * @file
 * Page callbacks for Drop Newsletter.
 */

/**
 * Form: manage newsletter subscriptions for a CRM contact.
 */
function dropnewsletter_contact_form($form, &$form_state, $contact) {
  $form['#contact'] = $contact;
  $form['#attached']['css'][] = backdrop_get_path('module', 'dropnewsletter') . '/dropnewsletter.css';

  $email = dropnewsletter_get_primary_email($contact);

  if (!$email) {
    $form['no_email'] = array(
      '#markup' => '<p class="messages warning">'
        . t('No primary email address on file. <a href="@url">Add one</a> to manage newsletter subscriptions.',
            array('@url' => url('drop/crm/contact-methods/add/email', array('query' => array('cid' => $contact->cid)))))
        . '</p>',
    );
    return $form;
  }

  $categories = simplenews_category_get_visible();
  if (empty($categories)) {
    $form['no_newsletters'] = array(
      '#markup' => '<p>' . t('No newsletters have been created yet.') . '</p>',
    );
    return $form;
  }

  $subscriptions = dropnewsletter_get_subscriptions($email);

  $form['newsletters'] = array(
    '#type'       => 'container',
    '#attributes' => array('class' => array('dropnewsletter-list')),
  );

  foreach ($categories as $category) {
    $tid    = $category->tid;
    $status = isset($subscriptions[$tid]) ? $subscriptions[$tid] : 'none';

    if ($status === 'unsubscribed') {
      $form['newsletters']['newsletter_' . $tid . '_locked'] = array(
        '#markup' => '<div class="dropnewsletter-row dropnewsletter-row--unsubscribed">'
          . '<span class="dropnewsletter-name">' . check_plain($category->name) . '</span>'
          . '<span class="dropnewsletter-badge">' . t('Unsubscribed') . '</span>'
          . '</div>',
      );
    }
    else {
      $desc_html = !empty($category->description)
        ? '<span class="dropnewsletter-desc">' . check_plain($category->description) . '</span>'
        : '';
      $form['newsletters']['newsletter_' . $tid] = array(
        '#type'          => 'checkbox',
        '#title'         => check_plain($category->name),
        '#default_value' => $status === 'subscribed' ? 1 : 0,
        '#prefix'        => '<div class="dropnewsletter-row">',
        '#suffix'        => $desc_html . '</div>',
      );
    }
  }

  $form['actions'] = array(
    '#type'       => 'actions',
    '#attributes' => array('class' => array('dropnewsletter-actions')),
  );
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save subscriptions'),
  );
  $form['actions']['meta'] = array(
    '#markup' => '<span class="dropnewsletter-meta">'
      . t('Managing: %email', array('%email' => $email))
      . '</span>',
  );

  return $form;
}

/**
 * Submit handler for dropnewsletter_contact_form().
 */
function dropnewsletter_contact_form_submit($form, &$form_state) {
  $contact = $form['#contact'];
  $email   = dropnewsletter_get_primary_email($contact);

  if (!$email) {
    return;
  }

  $categories    = simplenews_category_get_visible();
  $subscriptions = dropnewsletter_get_subscriptions($email);
  $account       = simplenews_load_user_by_mail($email);

  $changed = 0;
  foreach ($categories as $category) {
    $tid    = $category->tid;
    $status = isset($subscriptions[$tid]) ? $subscriptions[$tid] : 'none';

    // Skip unsubscribed newsletters — they cannot be changed via this UI.
    if ($status === 'unsubscribed') {
      continue;
    }

    $was = $status === 'subscribed';
    $now = !empty($form_state['values']['newsletter_' . $tid]);

    if ($now && !$was) {
      simplenews_subscribe_user($email, $tid, FALSE, 'dropnewsletter', NULL, TRUE);
      $changed++;
    }
    elseif (!$now && $was) {
      simplenews_unsubscribe_user($email, $tid, FALSE, 'dropnewsletter');
      $changed++;
    }
  }

  if ($changed) {
    backdrop_set_message(t('Newsletter subscriptions updated for %email.', array('%email' => $email)));
  }
  else {
    backdrop_set_message(t('No changes were made.'));
  }
}
