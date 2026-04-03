<?php
/**
 * @file
 * Page callbacks for drop_base module.
 */

/**
 * Settings dashboard — shows links to all Drop module settings pages.
 *
 * Invokes hook_drop_settings_info() to collect settings page definitions
 * from all enabled modules.
 */
function drop_base_settings_dashboard() {
  backdrop_set_title(t('Drop Suite Settings'));

  $items = module_invoke_all('drop_settings_info');

  // Sort by weight.
  uasort($items, function($a, $b) {
    $wa = isset($a['weight']) ? $a['weight'] : 0;
    $wb = isset($b['weight']) ? $b['weight'] : 0;
    return $wa - $wb;
  });

  // Filter by permission.
  global $user;
  $items = array_filter($items, function($item) use ($user) {
    if (empty($item['permission'])) {
      return TRUE;
    }
    return user_access($item['permission'], $user);
  });

  if (empty($items)) {
    return '<p>' . t('No settings pages are available.') . '</p>';
  }

  $output = '<div class="drop-settings-dashboard">';

  foreach ($items as $key => $item) {
    $output .= '<div class="drop-settings-card">';
    $output .= '<h3>' . l(check_plain($item['title']), $item['path']) . '</h3>';
    if (!empty($item['description'])) {
      $output .= '<p>' . check_plain($item['description']) . '</p>';
    }
    $output .= '</div>';
  }

  $output .= '</div>';

  // Also link to permissions.
  if (user_access('administer permissions')) {
    $output .= '<div class="drop-settings-footer">';
    $output .= '<p>' . l(t('Drop Suite permissions'), 'admin/drop/permissions') . '</p>';
    $output .= '</div>';
  }

  return $output;
}
