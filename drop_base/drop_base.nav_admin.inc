<?php
/**
 * @file
 * Admin UI for the Drop Suite primary navigation.
 *
 * Site Architects can reorder, rename, hide, or icon-ize nav items declared
 * by modules via hook_drop_nav_links(), and add custom shortcut links that
 * live only in the Site Architect's config. Module-declared items remain the
 * source of truth for their existence — customizations layer on top.
 */

/**
 * Page callback: drop/settings/navigation.
 */
function drop_base_navigation_page() {
  backdrop_set_title(t('Primary navigation'));
  return backdrop_get_form('drop_base_navigation_form');
}

/**
 * Form builder: primary navigation customization.
 *
 * Combines hook-declared nav items with Site Architect overrides and custom
 * shortcut links into one draggable table.
 */
function drop_base_navigation_form($form, &$form_state) {
  $form['#theme'] = 'drop_base_navigation_form';
  $form['#tree'] = TRUE;

  // Load module-declared items and current config layer.
  $hook_items = _drop_base_hook_nav_items();
  $config = config('drop_base.navigation');
  $overrides = $config->get('overrides');
  $shortcuts = $config->get('shortcuts');
  if (!is_array($overrides)) {
    $overrides = array();
  }
  if (!is_array($shortcuts)) {
    $shortcuts = array();
  }

  $show_icons = $config->get('show_icons');
  if ($show_icons === NULL) {
    $show_icons = TRUE;
  }

  // Build a single merged list, sorted by effective weight, for display.
  $rows = array();
  foreach ($hook_items as $path => $item) {
    $override = isset($overrides[$path]) ? $overrides[$path] : array();
    $default_icon = isset($item['icon']) ? $item['icon'] : '';
    $rows[] = array(
      'kind'      => 'module',
      'row_id'    => 'mod:' . $path,
      'path'      => $path,
      'title'     => isset($override['title']) && $override['title'] !== '' ? $override['title'] : $item['title'],
      'default_title' => $item['title'],
      'icon'      => isset($override['icon']) ? $override['icon'] : $default_icon,
      'default_icon' => $default_icon,
      'weight'    => isset($override['weight']) ? (int) $override['weight'] : (int) $item['weight'],
      'hidden'    => !empty($override['hidden']),
      'source_module' => isset($item['module']) ? $item['module'] : '',
    );
  }
  foreach ($shortcuts as $index => $shortcut) {
    if (!is_array($shortcut) || empty($shortcut['path'])) {
      continue;
    }
    $rows[] = array(
      'kind'      => 'shortcut',
      'row_id'    => 'sc:' . $index,
      'path'      => $shortcut['path'],
      'title'     => isset($shortcut['title']) ? $shortcut['title'] : $shortcut['path'],
      'icon'      => isset($shortcut['icon']) ? $shortcut['icon'] : '',
      'weight'    => isset($shortcut['weight']) ? (int) $shortcut['weight'] : 50,
      'hidden'    => !empty($shortcut['hidden']),
    );
  }
  usort($rows, function($a, $b) {
    if ($a['weight'] === $b['weight']) {
      return strcmp($a['title'], $b['title']);
    }
    return $a['weight'] - $b['weight'];
  });

  $form['intro'] = array(
    '#markup' =>
      '<ul>' .
        '<li>' . t('Reorder by dragging.') . '</li>' .
        '<li>' . t('Rename, hide, or add a Phosphor icon to any item.') . '</li>' .
        '<li>' . t('Module-supplied items cannot be deleted — untick them if you want them hidden.') . '</li>' .
        '<li>' . t('Use the shortcuts section below to add custom links to any path (e.g. <code>drop/tickets/kanban</code> labelled "Boards").') . '</li>' .
      '</ul>' .
      '<p>' . t('Icon names are the lowercase <a href="@url">Phosphor</a> name, for example <code>kanban</code>, <code>house</code>, <code>ticket</code>. Clear the field to fall back to the module default.', array(
        '@url' => 'https://phosphoricons.com/',
      )) . '</p>',
  );

  $form['display_options'] = array(
    '#type' => 'fieldset',
    '#title' => t('Display options'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['display_options']['show_icons'] = array(
    '#type' => 'checkbox',
    '#title' => t('Show icons in the primary navigation'),
    '#description' => t('Turn off to hide all icons site-wide without clearing the per-item icon names.'),
    '#default_value' => (bool) $show_icons,
  );

  $form['items'] = array(
    '#type' => 'container',
  );

  $weight_delta = 50;
  foreach ($rows as $row) {
    $row_id = $row['row_id'];
    $form['items'][$row_id] = array(
      '#type' => 'container',
      '#kind' => $row['kind'],
      '#path' => $row['path'],
    );
    $form['items'][$row_id]['title'] = array(
      '#type' => 'textfield',
      '#default_value' => $row['title'],
      '#size' => 20,
      '#maxlength' => 128,
    );
    $form['items'][$row_id]['icon'] = array(
      '#type' => 'textfield',
      '#default_value' => $row['icon'],
      '#size' => 14,
      '#maxlength' => 64,
      '#attributes' => array('placeholder' => t('e.g. kanban')),
    );
    if ($row['kind'] === 'module') {
      $form['items'][$row_id]['path_display'] = array(
        '#markup' => '<code>' . check_plain($row['path']) . '</code>' .
          (!empty($row['source_module']) ? ' <small>(' . check_plain($row['source_module']) . ')</small>' : ''),
      );
    }
    else {
      $form['items'][$row_id]['path_edit'] = array(
        '#type' => 'textfield',
        '#default_value' => $row['path'],
        '#size' => 24,
        '#maxlength' => 255,
      );
    }
    $form['items'][$row_id]['hidden'] = array(
      '#type' => 'checkbox',
      '#default_value' => $row['hidden'],
    );
    $form['items'][$row_id]['weight'] = array(
      '#type' => 'number',
      '#default_value' => $row['weight'],
      '#size' => 4,
      '#delta' => $weight_delta,
      '#attributes' => array('class' => array('drop-nav-weight')),
    );
    if ($row['kind'] === 'shortcut') {
      $form['items'][$row_id]['delete'] = array(
        '#type' => 'checkbox',
        '#default_value' => FALSE,
      );
    }
    else {
      $form['items'][$row_id]['delete'] = array(
        '#markup' => '<span class="drop-nav-nodelete" title="' . t('Module-supplied item — hide it instead of deleting') . '">—</span>',
      );
    }
  }

  // Add-shortcut fieldset.
  $form['new_shortcut'] = array(
    '#type' => 'fieldset',
    '#title' => t('Add shortcut'),
    '#description' => t('Custom link visible to anyone who can see the nav. Add one shortcut per save.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['new_shortcut']['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Label'),
    '#size' => 20,
    '#maxlength' => 128,
  );
  $form['new_shortcut']['path'] = array(
    '#type' => 'textfield',
    '#title' => t('Path'),
    '#size' => 30,
    '#maxlength' => 255,
    '#description' => t('Internal path (e.g. <code>drop/tickets/kanban</code>) or full URL.'),
  );
  $form['new_shortcut']['icon'] = array(
    '#type' => 'textfield',
    '#title' => t('Icon'),
    '#size' => 14,
    '#maxlength' => 64,
    '#attributes' => array('placeholder' => t('e.g. kanban')),
  );
  $form['new_shortcut']['weight'] = array(
    '#type' => 'number',
    '#title' => t('Weight'),
    '#default_value' => 50,
    '#size' => 4,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save navigation'),
  );
  $form['actions']['reset'] = array(
    '#type'   => 'submit',
    '#value'  => t('Reset to defaults'),
    '#submit' => array('drop_base_navigation_form_reset_submit'),
  );

  return $form;
}

/**
 * Theme function for the navigation form — renders a tabledrag table.
 */
function theme_drop_base_navigation_form($variables) {
  $form = $variables['form'];

  backdrop_add_css('
    .drop-nav-admin-table td,
    .drop-nav-admin-table th { padding: 4px 8px; vertical-align: middle; }
    .drop-nav-admin-table input[type="text"],
    .drop-nav-admin-table input[type="number"] { margin: 0; padding: 4px 6px; }
    .drop-nav-admin-table .form-item { margin: 0; }
    .drop-nav-admin-table code { font-size: 0.85em; }
    .drop-nav-admin-table small { color: #666; font-size: 0.8em; }
    .drop-nav-nodelete { color: #aaa; }
  ', 'inline');

  $header = array(
    t('Label'),
    t('Icon'),
    t('Path'),
    t('Hide'),
    t('Weight'),
    t('Delete'),
  );

  $rows = array();
  if (isset($form['items'])) {
    foreach (element_children($form['items']) as $row_id) {
      $form['items'][$row_id]['weight']['#attributes']['class'][] = 'drop-nav-weight';
      $path_cell = isset($form['items'][$row_id]['path_display'])
        ? backdrop_render($form['items'][$row_id]['path_display'])
        : backdrop_render($form['items'][$row_id]['path_edit']);
      $rows[] = array(
        'data' => array(
          backdrop_render($form['items'][$row_id]['title']),
          backdrop_render($form['items'][$row_id]['icon']),
          $path_cell,
          backdrop_render($form['items'][$row_id]['hidden']),
          backdrop_render($form['items'][$row_id]['weight']),
          backdrop_render($form['items'][$row_id]['delete']),
        ),
        'class' => array('draggable'),
      );
    }
  }

  $output = '';
  if (isset($form['intro'])) {
    $output .= backdrop_render($form['intro']);
  }
  if (isset($form['display_options'])) {
    $output .= backdrop_render($form['display_options']);
  }

  $output .= theme('table', array(
    'header' => $header,
    'rows'   => $rows,
    'empty'  => t('No navigation items.'),
    'attributes' => array(
      'id'    => 'drop-nav-admin-table',
      'class' => array('drop-nav-admin-table'),
    ),
  ));

  backdrop_add_tabledrag('drop-nav-admin-table', 'order', 'sibling', 'drop-nav-weight');

  $output .= backdrop_render_children($form);

  return $output;
}

/**
 * Submit handler: save navigation form.
 */
function drop_base_navigation_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $config = config('drop_base.navigation');

  $config->set('show_icons', !empty($values['display_options']['show_icons']));

  $hook_items = _drop_base_hook_nav_items();
  $new_overrides = array();
  $new_shortcuts = array();

  if (!empty($values['items']) && is_array($values['items'])) {
    // Read module item metadata from the form tree (kind + path).
    foreach ($values['items'] as $row_id => $fields) {
      if (strpos($row_id, 'mod:') === 0) {
        $path = substr($row_id, 4);
        $hook_item = isset($hook_items[$path]) ? $hook_items[$path] : array();
        $default_title  = isset($hook_item['title']) ? $hook_item['title'] : '';
        $default_icon   = isset($hook_item['icon']) ? strtolower($hook_item['icon']) : '';
        $default_weight = isset($hook_item['weight']) ? (int) $hook_item['weight'] : 0;

        $override = array();
        if (isset($fields['title']) && $fields['title'] !== '' && $fields['title'] !== $default_title) {
          $override['title'] = $fields['title'];
        }
        if (isset($fields['icon']) && $fields['icon'] !== '' && strtolower($fields['icon']) !== $default_icon) {
          $override['icon'] = strtolower($fields['icon']);
        }
        if (isset($fields['weight']) && (int) $fields['weight'] !== $default_weight) {
          $override['weight'] = (int) $fields['weight'];
        }
        if (!empty($fields['hidden'])) {
          $override['hidden'] = TRUE;
        }
        if (!empty($override)) {
          $new_overrides[$path] = $override;
        }
      }
      elseif (strpos($row_id, 'sc:') === 0) {
        if (!empty($fields['delete'])) {
          continue;
        }
        $path = isset($fields['path_edit']) ? trim($fields['path_edit']) : '';
        if ($path === '') {
          continue;
        }
        $shortcut = array(
          'title'  => isset($fields['title']) ? $fields['title'] : $path,
          'path'   => $path,
          'icon'   => isset($fields['icon']) ? strtolower($fields['icon']) : '',
          'weight' => isset($fields['weight']) ? (int) $fields['weight'] : 50,
          'hidden' => !empty($fields['hidden']),
        );
        $new_shortcuts[] = $shortcut;
      }
    }
  }

  // Append a newly-added shortcut if the Site Architect filled the fieldset.
  if (!empty($values['new_shortcut']['path'])) {
    $new_shortcuts[] = array(
      'title'  => !empty($values['new_shortcut']['title']) ? $values['new_shortcut']['title'] : $values['new_shortcut']['path'],
      'path'   => trim($values['new_shortcut']['path']),
      'icon'   => !empty($values['new_shortcut']['icon']) ? strtolower($values['new_shortcut']['icon']) : '',
      'weight' => isset($values['new_shortcut']['weight']) ? (int) $values['new_shortcut']['weight'] : 50,
      'hidden' => FALSE,
    );
  }

  $config->set('overrides', $new_overrides);
  $config->set('shortcuts', $new_shortcuts);
  $config->save();

  backdrop_set_message(t('Navigation saved.'));
  $form_state['redirect'] = 'drop/settings/navigation';
}

/**
 * Submit handler: reset navigation overrides + shortcuts to defaults (empty).
 */
function drop_base_navigation_form_reset_submit($form, &$form_state) {
  $config = config('drop_base.navigation');
  $config->set('show_icons', TRUE);
  $config->set('overrides', array());
  $config->set('shortcuts', array());
  $config->save();
  backdrop_set_message(t('Navigation reset to module defaults. All customizations and shortcuts cleared.'));
  $form_state['redirect'] = 'drop/settings/navigation';
}
