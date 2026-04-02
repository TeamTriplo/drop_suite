<?php
/**
 * @file
 * Page callbacks for DropDocs.
 */

/**
 * Page callback: renders the docs index at drop/docs.
 *
 * Displays all docs in a hierarchical tree with an add-doc action.
 *
 * @return array  Backdrop render array.
 */
function dropdocs_index_page() {
  backdrop_set_title(t('Docs'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
  ));

  $output = array();

  // Page actions (add doc).
  if (node_access('create', 'doc')) {
    $output['actions'] = array(
      '#markup' => drop_base_page_actions(array(
        l(t('Add doc'), 'node/add/doc', array('attributes' => array('class' => array('button', 'button-primary')))),
      )),
    );
  }

  $docs = dropdocs_load_all();

  if (empty($docs)) {
    $output['empty'] = array(
      '#markup' => '<p class="dropdocs-empty">' . t('No documentation yet.') . '</p>',
    );
    return $output;
  }

  $tree = dropdocs_build_tree($docs);
  $output['tree'] = array(
    '#markup'   => '<div class="dropdocs-index-tree">' . dropdocs_render_tree($tree) . '</div>',
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropdocs') . '/dropdocs.css'),
      'js'  => array(backdrop_get_path('module', 'dropdocs') . '/dropdocs.js'),
    ),
  );

  return $output;
}

/**
 * Page callback: renders a single doc with sidebar tree navigation.
 *
 * @param object $node  The loaded doc node.
 *
 * @return array  Backdrop render array.
 */
function dropdocs_doc_view($node) {
  // Only show doc nodes here.
  if ($node->type !== 'doc') {
    return MENU_NOT_FOUND;
  }

  backdrop_set_title($node->title);

  // Build breadcrumb with ancestor chain.
  $crumb = array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Docs'), 'drop/docs'),
  );
  $ancestors = dropdocs_get_ancestors($node->nid);
  foreach ($ancestors as $ancestor) {
    $crumb[] = l(check_plain($ancestor->title), 'drop/doc/' . $ancestor->nid);
  }
  backdrop_set_breadcrumb($crumb);

  // Sidebar: full doc tree with active item highlighted.
  $docs    = dropdocs_load_all();
  $tree    = dropdocs_build_tree($docs);
  $sidebar = '<aside class="dropdocs-sidebar">'
    . '<h3 class="dropdocs-sidebar-title">' . t('Documentation') . '</h3>'
    . dropdocs_render_tree($tree, $node->nid)
    . '</aside>';

  // Main content: render node in full view mode.
  $node_view = node_view($node, 'full');
  $main = '<div class="dropdocs-body">' . backdrop_render($node_view) . '</div>';

  return array(
    '#markup'   => '<div class="dropdocs-view-layout">' . $sidebar . $main . '</div>',
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropdocs') . '/dropdocs.css'),
      'js'  => array(backdrop_get_path('module', 'dropdocs') . '/dropdocs.js'),
    ),
  );
}

/**
 * Page callback: redirects to node/add/doc with the parent nid pre-filled.
 *
 * @param object $node  The parent doc node.
 */
function dropdocs_add_child_redirect($node) {
  backdrop_goto('node/add/doc', array('query' => array('parent' => $node->nid)));
}

// ---------------------------------------------------------------------------
// Reorder form
// ---------------------------------------------------------------------------

/**
 * Form builder: drag-and-drop doc reorder page.
 *
 * Renders one draggable table per sibling group (top-level docs, plus one
 * section per parent that has children). Weights are stored in the
 * dropdocs_weight table.
 */
function dropdocs_reorder_form($form, &$form_state) {
  backdrop_set_title(t('Reorder docs'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(t('Docs'), 'drop/docs'),
  ));

  $docs = dropdocs_load_all();

  if (empty($docs)) {
    $form['empty'] = array('#markup' => '<p>' . t('No docs to reorder.') . '</p>');
    return $form;
  }

  // Group docs by parent_nid. Key 0 = root level.
  $groups = array();
  foreach ($docs as $nid => $doc) {
    $parent_key = $doc->parent_nid ? (int) $doc->parent_nid : 0;
    $groups[$parent_key][$nid] = $doc;
  }

  // Root group (key 0) first, then child groups sorted by parent nid.
  ksort($groups);

  // Sort each group by current weight then title.
  foreach ($groups as $parent_key => &$group) {
    uasort($group, function($a, $b) {
      $wa = ($a->weight !== NULL) ? (int) $a->weight : 0;
      $wb = ($b->weight !== NULL) ? (int) $b->weight : 0;
      return ($wa !== $wb) ? ($wa - $wb) : strcmp($a->title, $b->title);
    });
  }
  unset($group);

  // Build per-group form elements. The theme function renders these into tables.
  $form['groups'] = array('#tree' => TRUE);

  foreach ($groups as $parent_nid => $group_docs) {
    $form['groups'][$parent_nid] = array(
      '#parent_nid'   => $parent_nid,
      '#parent_label' => _dropdocs_group_label($parent_nid, $docs),
      '#tree'         => TRUE,
    );
    foreach ($group_docs as $nid => $doc) {
      $form['groups'][$parent_nid][$nid] = array(
        'title'  => array('#markup' => check_plain($doc->title)),
        'weight' => array(
          '#type'              => 'weight',
          '#title'             => t('Weight'),
          '#title_display'     => 'invisible',
          '#default_value'     => ($doc->weight !== NULL) ? (int) $doc->weight : 0,
          '#delta'             => 50,
          '#attributes'        => array('class' => array('dropdocs-w-' . $parent_nid)),
        ),
      );
    }
  }

  $form['#theme']    = 'dropdocs_reorder_form';
  $form['#attached'] = array(
    'css' => array(backdrop_get_path('module', 'dropdocs') . '/dropdocs.css'),
  );

  $form['actions'] = array(
    '#type'   => 'actions',
    '#weight' => 100,
    'submit'  => array(
      '#type'       => 'submit',
      '#value'      => t('Save order'),
      '#attributes' => array('class' => array('button-primary')),
    ),
  );

  return $form;
}

/**
 * Returns a human-readable label for a sibling group.
 *
 * @param int   $parent_nid  0 for root, otherwise the parent doc nid.
 * @param array $docs        Flat doc list from dropdocs_load_all().
 *
 * @return string
 */
function _dropdocs_group_label($parent_nid, array $docs) {
  if ($parent_nid === 0) {
    return t('Top-level pages');
  }
  $parent = isset($docs[$parent_nid]) ? $docs[$parent_nid] : NULL;
  return $parent
    ? t('Children of %title', array('%title' => $parent->title))
    : t('Orphaned pages');
}

/**
 * Theme function: renders dropdocs_reorder_form as draggable group tables.
 *
 * Uses array references so that backdrop_render() correctly marks each element
 * as #printed, preventing backdrop_render_children() from emitting them again.
 */
function theme_dropdocs_reorder_form($variables) {
  $form   = &$variables['form'];
  $output = '';

  if (empty($form['groups'])) {
    return backdrop_render_children($form);
  }

  foreach (element_children($form['groups']) as $parent_nid) {
    $group    = &$form['groups'][$parent_nid];
    $label    = $group['#parent_label'];
    $table_id = 'dropdocs-reorder-' . $parent_nid;
    $wclass   = 'dropdocs-w-' . $parent_nid;

    $rows = array();
    foreach (element_children($group) as $nid) {
      $rows[] = array(
        'data'  => array(
          array('class' => array('tabledrag-handle')),
          backdrop_render($group[$nid]['title']),
          backdrop_render($group[$nid]['weight']),
        ),
        'class' => array('draggable'),
      );
    }
    unset($group);

    $output .= '<h3 class="dropdocs-reorder-label">' . $label . '</h3>';
    $output .= theme('table', array(
      'header' => array(
        array('data' => '', 'class' => array('tabledrag-hide')),
        t('Page'),
        array('data' => t('Weight'), 'class' => array('tabledrag-hide')),
      ),
      'rows'       => $rows,
      'attributes' => array('id' => $table_id),
      'sticky'     => FALSE,
    ));

    drupal_add_tabledrag($table_id, 'order', 'sibling', $wclass);
  }

  $output .= backdrop_render_children($form);
  return $output;
}

/**
 * Submit handler: persist new weights to the dropdocs_weight table.
 */
function dropdocs_reorder_form_submit($form, &$form_state) {
  foreach ($form_state['values']['groups'] as $parent_nid => $group) {
    foreach ($group as $nid => $values) {
      if (!is_array($values) || !isset($values['weight'])) {
        continue;
      }
      db_merge('dropdocs_weight')
        ->key(array('nid' => (int) $nid))
        ->fields(array('weight' => (int) $values['weight']))
        ->execute();
    }
  }
  backdrop_set_message(t('Doc order saved.'));
  $form_state['redirect'] = 'drop/docs';
}
