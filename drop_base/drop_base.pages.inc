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

  // Entity types section.
  $entity_types = _drop_base_fieldable_entity_types();
  if (!empty($entity_types) && user_access('administer drop entities')) {
    $output .= '<div class="drop-settings-footer">';
    $output .= '<h3>' . t('Entity Types') . '</h3>';
    $output .= '<p>' . t('Manage custom fields and display settings for Drop Suite entities.') . '</p>';
    $output .= '<div class="drop-settings-dashboard">';
    foreach ($entity_types as $type_info) {
      $output .= '<div class="drop-settings-card">';
      $link_path = $type_info['path'] . '/fields';
      if (menu_get_item($type_info['path'])) {
        $link_path = $type_info['path'];
      }
      $output .= '<h3>' . l(check_plain($type_info['label']), $link_path) . '</h3>';
      if (!empty($type_info['description'])) {
        $output .= '<p>' . check_plain($type_info['description']) . '</p>';
      }
      $output .= '</div>';
    }
    $output .= '</div>';
    $output .= '</div>';
  }

  // Also link to permissions.
  if (user_access('administer permissions')) {
    $output .= '<div class="drop-settings-footer">';
    $output .= '<p>' . l(t('Drop Suite permissions'), 'admin/drop/permissions') . '</p>';
    $output .= '</div>';
  }

  return $output;
}

/**
 * Discovers all fieldable Drop Suite entity types with bundle admin paths.
 *
 * @return array
 *   Array of entity type info, each with 'label', 'path', and 'description'.
 */
function _drop_base_fieldable_entity_types() {
  $all_info = entity_get_info();
  $drop_modules = array('drop_base', 'dropcrm', 'droptickets', 'droptasks', 'dropgoals', 'droptime', 'dropdocs');

  $types = array();
  foreach ($all_info as $entity_type => $info) {
    if (empty($info['fieldable'])) {
      continue;
    }
    // Only include entities owned by Drop Suite modules.
    if (empty($info['module']) || !in_array($info['module'], $drop_modules)) {
      continue;
    }

    // Collect unique bundle admin paths.
    if (!empty($info['bundles'])) {
      foreach ($info['bundles'] as $bundle_name => $bundle_info) {
        if (empty($bundle_info['admin']['path'])) {
          continue;
        }
        $path = $bundle_info['admin']['path'];
        // For entities with multiple bundles (like crm_contact), list each bundle.
        $label = $info['label'];
        if (count($info['bundles']) > 1) {
          $label = $bundle_info['label'];
        }
        $types[$entity_type . '_' . $bundle_name] = array(
          'label' => $label,
          'path' => $path,
          'description' => isset($info['description']) ? $info['description'] : '',
        );
      }
    }
  }

  // Sort alphabetically by label.
  uasort($types, function($a, $b) {
    return strcmp($a['label'], $b['label']);
  });

  return $types;
}
