<?php
/**
 * @file
 * Page callbacks for Drop Docs.
 */

/**
 * Page callback: renders the docs index at drop/docs.
 *
 * Lists every book whose top-level node is of type 'doc', rendering each
 * book's outline as a nested link tree. Ordering and nesting are managed
 * entirely by the core Book module.
 *
 * @return array  Backdrop render array.
 */
function drop_docs_index_page() {
  backdrop_set_title(drop_docs_display_name());
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
  ));

  $output = array();

  // Page actions (add doc).
  if (node_access('create', 'doc')) {
    $output['actions'] = array(
      '#markup' => drop_suite_page_actions(array(
        l(t('Add @name', array('@name' => strtolower(drop_docs_display_name('singular')))), 'node/add/doc', array('attributes' => array('class' => array('button', 'button-primary')))),
      )),
    );
  }

  $doc_books = _drop_docs_get_doc_books();

  if (empty($doc_books)) {
    $output['empty'] = array(
      '#markup' => '<p class="drop-docs-empty">' . t('No @name yet.', array('@name' => strtolower(drop_docs_display_name()))) . '</p>',
    );
    return $output;
  }

  $markup = '';
  foreach ($doc_books as $bid => $book) {
    $tree = menu_tree_all_data($book['menu_name']);
    $markup .= _drop_docs_render_book_tree($tree);
  }

  $output['tree'] = array(
    '#markup'   => '<div class="drop-docs-index-tree">' . $markup . '</div>',
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'drop_docs') . '/drop_docs.css'),
    ),
  );

  return $output;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Renders a book menu tree as a nested <ul> of node/{nid} links.
 *
 * Backdrop's path aliases apply to node/{nid} automatically, so users see
 * friendly URLs in the tree without us hard-coding any custom route.
 *
 * @param array $tree
 *   Output of menu_tree_all_data() for a book menu.
 *
 * @return string  HTML markup (empty string if $tree is empty).
 */
function _drop_docs_render_book_tree(array $tree) {
  if (empty($tree)) {
    return '';
  }
  $output = '<ul class="drop-docs-tree">';
  foreach ($tree as $data) {
    if (empty($data['link']) || !empty($data['link']['hidden'])) {
      continue;
    }
    $link = $data['link'];
    if (!preg_match('#^node/(\d+)$#', $link['link_path'], $m)) {
      continue;
    }
    $nid          = (int) $m[1];
    $has_children = !empty($data['below']);
    $class_attr   = $has_children ? ' class="drop-docs-has-children"' : '';
    $output .= '<li' . $class_attr . ' data-nid="' . $nid . '">';
    $output .= l($link['link_title'], 'node/' . $nid);
    if ($has_children) {
      $output .= _drop_docs_render_book_tree($data['below']);
    }
    $output .= '</li>';
  }
  $output .= '</ul>';
  return $output;
}

/**
 * Returns all books whose top-level node is a doc node, keyed by bid.
 *
 * @return array
 */
function _drop_docs_get_doc_books() {
  $doc_books = array();
  foreach (book_get_books() as $bid => $book) {
    $root = node_load($book['nid']);
    if ($root && $root->type === 'doc' && node_access('view', $root)) {
      $doc_books[$bid] = $book;
    }
  }
  return $doc_books;
}

// ---------------------------------------------------------------------------
// Settings form
// ---------------------------------------------------------------------------

/**
 * Settings form for Drop Docs.
 */
function drop_docs_settings_form($form, &$form_state) {
  backdrop_set_title(t('Settings'));
  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(drop_docs_display_name(), 'drop/docs'),
  ));

  $config = config('drop_docs.settings');

  $form['display'] = array(
    '#type' => 'fieldset',
    '#title' => t('Display'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['display']['display_name'] = array(
    '#type' => 'select',
    '#title' => t('Display name'),
    '#description' => t('Choose what this section is called in navigation, breadcrumbs, and page titles. The internal module name and URLs are unchanged.'),
    '#options' => array(
      'Docs' => t('Docs'),
      'Notes' => t('Notes'),
      'Wiki' => t('Wiki'),
      'Knowledge Base' => t('Knowledge Base'),
      'Resources' => t('Resources'),
    ),
    '#default_value' => $config->get('display_name'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save settings'),
  );

  return $form;
}

/**
 * Settings form submit.
 */
function drop_docs_settings_form_submit($form, &$form_state) {
  $config = config('drop_docs.settings');
  $config->set('display_name', $form_state['values']['display_name']);
  $config->save();
  backdrop_set_message(t('Settings saved.'));
  $form_state['redirect'] = 'drop/docs';
}
