<?php
/**
 * @file
 * Page callbacks, forms, and AJAX handlers for DropUpdates.
 */

// ---------------------------------------------------------------------------
// Feed page
// ---------------------------------------------------------------------------

/**
 * Page callback: team updates feed at drop/updates.
 */
function dropupdates_feed_page() {
  backdrop_set_title(t('Team Updates'));

  $build = array();
  $build['#attached']['css'][] = backdrop_get_path('module', 'dropupdates') . '/css/dropupdates.css';
  $build['#attached']['js'][]  = backdrop_get_path('module', 'dropupdates') . '/js/dropupdates.js';
  // Shared reaction chip CSS/JS.
  $reactions_assets = drop_base_reactions_attached();
  foreach ($reactions_assets['css'] as $css) {
    $build['#attached']['css'][] = $css;
  }
  foreach ($reactions_assets['js'] as $js) {
    $build['#attached']['js'][] = $js;
  }
  $build['#attached']['js'][] = array(
    'type' => 'setting',
    'data' => array(
      'dropupdates' => array(
        'seenBase' => url('drop/updates/seen'),
      ),
    ),
  );

  // Post form at the top if the user can post.
  if (user_access('post drop updates')) {
    $build['post_form'] = backdrop_get_form('dropupdates_post_form');
  }

  // Load the most recent updates (default 50).
  $ids = db_select('drop_update', 'u')
    ->fields('u', array('uid_post'))
    ->orderBy('u.created', 'DESC')
    ->range(0, 50)
    ->execute()
    ->fetchCol();

  if (empty($ids)) {
    $build['empty'] = array(
      '#type'   => 'markup',
      '#markup' => '<div class="drop-updates-empty">'
        . t('No updates yet. Post the first one!')
        . '</div>',
    );
    return $build;
  }

  $updates = entity_load('drop_update', $ids);
  // Preserve the order from the query above.
  $ordered = array();
  foreach ($ids as $id) {
    if (isset($updates[$id])) {
      $ordered[$id] = $updates[$id];
    }
  }

  // Group by local day (site timezone).
  $site_tz = new DateTimeZone(config_get('system.date', 'default_timezone') ?: date_default_timezone_get());
  $groups  = array();
  foreach ($ordered as $update) {
    $dt = new DateTime('@' . $update->created);
    $dt->setTimezone($site_tz);
    $day = $dt->format('Y-m-d');
    if (!isset($groups[$day])) {
      $groups[$day] = array();
    }
    $groups[$day][] = $update;
  }

  $build['feed'] = array(
    '#type'   => 'markup',
    '#markup' => _dropupdates_render_feed($groups),
  );

  return $build;
}

/**
 * Renders grouped updates as HTML.
 */
function _dropupdates_render_feed(array $groups) {
  global $user;
  $today = format_date(REQUEST_TIME, 'custom', 'Y-m-d');

  // Collect all update ids so we can bulk-load reactions in one query.
  $all_ids = array();
  foreach ($groups as $updates) {
    foreach ($updates as $u) {
      $all_ids[] = (int) $u->uid_post;
    }
  }
  $all_reactions = dropupdates_update_reactions_multiple($all_ids);

  // Bulk-load reactor accounts for tooltip names.
  $reaction_uids = array();
  foreach ($all_reactions as $by_code) {
    foreach ($by_code as $uids) {
      foreach ($uids as $uid) {
        $reaction_uids[(int) $uid] = TRUE;
      }
    }
  }
  $reaction_accounts = $reaction_uids ? user_load_multiple(array_keys($reaction_uids)) : array();

  $html = '<div class="drop-updates-feed">';
  foreach ($groups as $day => $updates) {
    $label = _dropupdates_day_label($day, $today);
    $html .= '<div class="drop-updates-day">';
    $html .= '<h3 class="drop-updates-day__label">' . check_plain($label) . '</h3>';

    foreach ($updates as $update) {
      $reactions = isset($all_reactions[$update->uid_post]) ? $all_reactions[$update->uid_post] : array();
      $html .= _dropupdates_render_card($update, $user, $reactions, $reaction_accounts);
    }
    $html .= '</div>';
  }
  $html .= '</div>';
  return $html;
}

/**
 * Renders a single update card.
 */
function _dropupdates_render_card($update, $viewer, array $reactions = array(), array $reaction_accounts = array()) {
  $author = user_load($update->uid);
  $author_name = $author ? format_username($author) : t('Unknown');
  $author_picture = '';
  if ($author) {
    $author_picture = theme('user_picture', array('account' => $author));
  }

  $is_own = ($update->uid == $viewer->uid);
  $classes = array('drop-update', 'drop-update--card', 'drop-reactions-host');
  if ($is_own) {
    $classes[] = 'drop-update--own';
  }

  $body = check_markup($update->body, $update->body_format, '', TRUE);
  $time = format_date($update->created, 'custom', 'g:i a');

  // Seen-by list (team members only, excluding author).
  $viewers = dropupdates_update_viewers($update->uid_post);
  unset($viewers[$update->uid]);
  $seen_html = _dropupdates_render_seen_by($viewers);

  // Reactions — delegated to shared drop_base helper.
  $reactions_html = drop_base_render_reactions($reactions, $reaction_accounts, array(
    'react_url' => url('drop/updates/react/' . (int) $update->uid_post),
    'data'      => array('update-id' => (int) $update->uid_post),
  ));

  // Edit/delete links for own posts (or admin).
  $actions = '';
  if (dropupdates_update_access('update', $update, $viewer)) {
    $actions .= ' ' . l(t('Edit'), 'drop/updates/' . $update->uid_post . '/edit', array(
      'attributes' => array('class' => array('drop-update__action')),
    ));
  }
  if (dropupdates_update_access('delete', $update, $viewer)) {
    $actions .= ' ' . l(t('Delete'), 'drop/updates/' . $update->uid_post . '/delete', array(
      'attributes' => array('class' => array('drop-update__action')),
    ));
  }

  $html = '<article class="' . implode(' ', $classes) . '" data-update-id="' . (int) $update->uid_post . '" id="u-' . (int) $update->uid_post . '">';
  $html .= '<header class="drop-update__header">';
  if ($author_picture) {
    $html .= '<div class="drop-update__avatar">' . $author_picture . '</div>';
  }
  $html .= '<div class="drop-update__meta">';
  $html .= '<span class="drop-update__author">' . check_plain($author_name) . '</span>';
  $html .= ' <span class="drop-update__time">' . check_plain($time) . '</span>';
  if ($actions) {
    $html .= '<span class="drop-update__actions">' . $actions . '</span>';
  }
  $html .= '</div>';
  $html .= '</header>';
  $html .= '<div class="drop-update__body">' . $body . '</div>';
  $html .= '<footer class="drop-update__footer">';
  $html .= $reactions_html;
  $html .= $seen_html;
  $html .= '</footer>';
  $html .= '</article>';

  return $html;
}

/**
 * Returns a friendly day label: "Today", "Yesterday", or a date.
 */
function _dropupdates_day_label($day, $today) {
  if ($day === $today) {
    return t('Today');
  }
  $yesterday = format_date(REQUEST_TIME - 86400, 'custom', 'Y-m-d');
  if ($day === $yesterday) {
    return t('Yesterday');
  }
  $ts = strtotime($day);
  return format_date($ts, 'custom', 'l, F j, Y');
}

/**
 * Renders the seen-by list for an update card as comma-separated names.
 *
 * Shows up to 5 names inline, then "+N more" for the remainder.
 *
 * @param array $viewers
 *   Array of uid => viewed_at timestamps.
 */
function _dropupdates_render_seen_by(array $viewers) {
  if (empty($viewers)) {
    return '<div class="drop-update__seen drop-update__seen--empty">'
      . t('Not yet seen by anyone.')
      . '</div>';
  }

  $max_inline  = 5;
  $uids        = array_keys($viewers);
  $shown_uids  = array_slice($uids, 0, $max_inline);
  $extra_count = count($uids) - count($shown_uids);
  $users       = user_load_multiple($shown_uids);

  $names = array();
  foreach ($shown_uids as $uid) {
    if (empty($users[$uid])) {
      continue;
    }
    $tip = format_date($viewers[$uid], 'small');
    $names[] = '<span class="drop-update__seen-name" title="' . check_plain($tip) . '">'
      . check_plain(format_username($users[$uid]))
      . '</span>';
  }

  $list = implode(', ', $names);
  if ($extra_count > 0) {
    $list .= ' <span class="drop-update__seen-more">+' . (int) $extra_count . ' ' . t('more') . '</span>';
  }

  return '<div class="drop-update__seen">'
    . '<span class="drop-update__seen-label">' . t('Seen by') . '</span> '
    . $list
    . '</div>';
}

// ---------------------------------------------------------------------------
// Post / edit form
// ---------------------------------------------------------------------------

/**
 * Form builder: post or edit an update.
 *
 * @param array $form
 * @param array $form_state
 * @param DropUpdate|null $update
 */
function dropupdates_post_form($form, &$form_state, $update = NULL) {
  $is_edit = !empty($update);
  if (!$is_edit) {
    $update = entity_create('drop_update', array());
  }
  $form_state['update'] = $update;

  $form['#attributes']['class'][] = 'drop-updates-post-form';

  $form['body'] = array(
    '#type'          => 'text_format',
    '#title'         => $is_edit ? t('Edit your update') : t('Post an update'),
    '#default_value' => $is_edit ? $update->body : '',
    '#format'        => $is_edit ? $update->body_format : NULL,
    '#rows'          => 4,
    '#required'      => TRUE,
    '#description'   => $is_edit ? NULL : t('What did you work on? Share it with the team.'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Save') : t('Post update'),
  );
  if ($is_edit) {
    $form['actions']['cancel'] = array(
      '#type'   => 'link',
      '#title'  => t('Cancel'),
      '#href'   => 'drop/updates',
      '#options' => array('attributes' => array('class' => array('button'))),
    );
  }

  return $form;
}

/**
 * Submit handler: save the update.
 */
function dropupdates_post_form_submit($form, &$form_state) {
  $update = $form_state['update'];
  $update->body        = $form_state['values']['body']['value'];
  $update->body_format = $form_state['values']['body']['format'];
  entity_get_controller('drop_update')->save($update);

  backdrop_set_message(empty($form_state['update']->changed)
    ? t('Update posted.')
    : t('Update saved.'));
  $form_state['redirect'] = 'drop/updates';
}

// ---------------------------------------------------------------------------
// Delete confirm form
// ---------------------------------------------------------------------------

/**
 * Form builder: confirm delete.
 */
function dropupdates_delete_confirm($form, &$form_state, $update) {
  $form_state['update'] = $update;
  return confirm_form(
    $form,
    t('Delete this update?'),
    'drop/updates',
    t('This cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler: delete the update and its child rows.
 */
function dropupdates_delete_confirm_submit($form, &$form_state) {
  $update = $form_state['update'];
  $uid_post = $update->uid_post;

  entity_delete('drop_update', $uid_post);
  db_delete('drop_update_view')->condition('uid_post', $uid_post)->execute();
  db_delete('drop_update_reaction')->condition('uid_post', $uid_post)->execute();

  backdrop_set_message(t('Update deleted.'));
  $form_state['redirect'] = 'drop/updates';
}

// ---------------------------------------------------------------------------
// AJAX: mark seen
// ---------------------------------------------------------------------------

/**
 * AJAX callback: record that the current user has seen the given update.
 */
function dropupdates_seen_ajax($update) {
  global $user;
  if (!$update || !$user->uid) {
    return array('ok' => FALSE);
  }
  // Authors don't count as readers of their own posts.
  if ($update->uid == $user->uid) {
    return array('ok' => TRUE, 'skipped' => 'author');
  }
  // Only team members are tracked.
  $team = dropupdates_team_members();
  if (!isset($team[$user->uid])) {
    return array('ok' => TRUE, 'skipped' => 'not_team');
  }

  db_merge('drop_update_view')
    ->key(array('uid_post' => $update->uid_post, 'uid' => $user->uid))
    ->fields(array('viewed_at' => REQUEST_TIME))
    ->execute();

  return array('ok' => TRUE);
}

// ---------------------------------------------------------------------------
// AJAX: toggle reaction
// ---------------------------------------------------------------------------

/**
 * AJAX callback: toggle a reaction on an update.
 *
 * Emoji code is read from the POST body.
 */
function dropupdates_react_ajax($update) {
  global $user;
  if (!$update || !$user->uid) {
    return array('error' => 'forbidden');
  }
  $emoji = isset($_POST['emoji']) ? trim($_POST['emoji']) : '';
  if (!array_key_exists($emoji, drop_base_reaction_emojis())) {
    return array('error' => 'invalid emoji');
  }

  $exists = (bool) db_select('drop_update_reaction', 'r')
    ->fields('r', array('uid'))
    ->condition('uid_post', $update->uid_post)
    ->condition('uid', $user->uid)
    ->condition('emoji', $emoji)
    ->range(0, 1)
    ->execute()
    ->fetchField();

  if ($exists) {
    db_delete('drop_update_reaction')
      ->condition('uid_post', $update->uid_post)
      ->condition('uid', $user->uid)
      ->condition('emoji', $emoji)
      ->execute();
  }
  else {
    db_insert('drop_update_reaction')
      ->fields(array(
        'uid_post' => $update->uid_post,
        'uid'      => $user->uid,
        'emoji'    => $emoji,
        'created'  => REQUEST_TIME,
      ))
      ->execute();
  }

  $reactions = dropupdates_update_reactions($update->uid_post);
  return array('reactions' => drop_base_format_reactions_response($reactions, $user->uid));
}
