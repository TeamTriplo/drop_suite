<?php
/**
 * @file
 * Page callbacks for DropFiles.
 */

/**
 * Page callback: main file library listing at drop/files.
 *
 * @return array
 *   A render array.
 */
function dropfiles_library_page() {
  backdrop_set_title(dropfiles_display_name());

  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
  ));

  $module_path = backdrop_get_path('module', 'dropfiles');

  $output = array(
    '#attached' => array(
      'css' => array($module_path . '/dropfiles.css'),
      'js' => array($module_path . '/dropfiles.js'),
    ),
  );

  // Page actions: upload toggle button (replaces the old "Upload file" link).
  if (user_access('upload dropfiles')) {
    $upload_toggle = '<button type="button" id="dropfiles-upload-toggle" class="dropfiles-upload-toggle">'
      . icon('upload-simple', array('attributes' => array('width' => 16, 'height' => 16)))
      . ' ' . t('Upload files')
      . '</button>';
    $actions = array($upload_toggle);
    $output['actions'] = array(
      '#markup' => drop_suite_page_actions($actions),
      '#weight' => -100,
    );

    // Drag-and-drop upload zone (hidden by default, toggled via JS).
    $config = config('dropfiles.settings');
    $extensions = $config->get('allowed_extensions');
    $max_size = format_size(file_upload_max_size());

    $output['#attached']['js'][] = array(
      'type' => 'setting',
      'data' => array(
        'dropfilesUpload' => array(
          'uploadUrl' => url('drop/files/ajax-upload'),
          'token' => backdrop_get_token('dropfiles_ajax_upload'),
        ),
      ),
    );

    $output['dropzone'] = array(
      '#markup' => '<div id="dropfiles-dropzone-wrapper" class="dropfiles-dropzone-wrapper" style="display:none">'
        . '<div id="dropfiles-dropzone" class="dropfiles-dropzone">'
        . '<div class="dropfiles-dropzone-inner">'
        . icon('upload-simple', array('attributes' => array('width' => 32, 'height' => 32)))
        . '<p>' . t('Drag and drop files here to upload') . '</p>'
        . '<p class="dropfiles-dropzone-hint">' . t('or click to select files') . '</p>'
        . '<p class="dropfiles-dropzone-hint">' . t('Allowed types: @ext. Max size: @size', array('@ext' => $extensions, '@size' => $max_size)) . '</p>'
        . '<input type="file" id="dropfiles-file-input" multiple style="display:none">'
        . '</div>'
        . '<div class="dropfiles-upload-progress" style="display:none">'
        . '<div class="dropfiles-progress-text"></div>'
        . '<div class="dropfiles-progress-bar"><div class="dropfiles-progress-bar-fill"></div></div>'
        . '<div class="dropfiles-progress-detail"></div>'
        . '</div>'
        . '</div>'
        . '</div>',
      '#weight' => -90,
    );
  }

  // Read filters from query string.
  $tag_filter = isset($_GET['tag']) ? (int) $_GET['tag'] : 0;
  $search = isset($_GET['search']) ? trim($_GET['search']) : '';

  // Build tag options for filter select.
  $vocabulary = taxonomy_vocabulary_load('file_tags');
  $tag_options = array();
  if ($vocabulary) {
    $terms = taxonomy_term_load_multiple(FALSE, array('vocabulary' => $vocabulary->machine_name));
    foreach ($terms as $term) {
      $tag_options[$term->tid] = check_plain($term->name);
    }
    asort($tag_options);
  }

  // Assemble filter form (GET-based, raw HTML) with view toggle.
  $filter_form = '<div class="dropfiles-toolbar">';
  $filter_form .= '<form method="get" action="' . url('drop/files') . '" class="dropfiles-filter-form">';
  $filter_form .= '<select name="tag" onchange="this.form.submit()">';
  $filter_form .= '<option value="">' . t('— All tags —') . '</option>';
  foreach ($tag_options as $tid => $name) {
    $selected = ($tag_filter === (int) $tid) ? ' selected' : '';
    $filter_form .= '<option value="' . (int) $tid . '"' . $selected . '>' . $name . '</option>';
  }
  $filter_form .= '</select>';
  $filter_form .= '<input type="search" name="search" placeholder="' . t('Search files...') . '" value="' . check_plain($search) . '">';
  $filter_form .= '<button type="submit">' . t('Filter') . '</button>';
  $filter_form .= '</form>';

  // View toggle buttons.
  $filter_form .= '<div class="dropfiles-view-toggle">';
  $filter_form .= '<button type="button" class="dropfiles-view-btn" data-view="list" title="' . t('List view') . '">'
    . icon('list', array('attributes' => array('width' => 18, 'height' => 18)))
    . '</button>';
  $filter_form .= '<button type="button" class="dropfiles-view-btn dropfiles-view-btn--active" data-view="grid" title="' . t('Grid view') . '">'
    . icon('squares-four', array('attributes' => array('width' => 18, 'height' => 18)))
    . '</button>';
  $filter_form .= '</div>';
  $filter_form .= '</div>';

  $output['filter'] = array(
    '#markup' => $filter_form,
    '#weight' => -50,
  );

  // Build file query.
  $query = db_select('file_managed', 'f')
    ->fields('f', array('fid', 'filename', 'uri', 'filesize', 'filemime', 'timestamp', 'uid'))
    ->condition('f.status', 1);
  $query->innerJoin('file_usage', 'fu', 'fu.fid = f.fid AND fu.module = :mod AND fu.type = :type', array(':mod' => 'dropfiles', ':type' => 'library'));

  if ($tag_filter) {
    $query->innerJoin('field_data_field_file_tags', 'ft', 'ft.entity_id = f.fid AND ft.entity_type = :etype', array(':etype' => 'file'));
    $query->condition('ft.field_file_tags_tid', $tag_filter);
  }

  if ($search !== '') {
    $query->condition('f.filename', '%' . db_like($search) . '%', 'LIKE');
  }

  $query->orderBy('f.timestamp', 'DESC');
  $query = $query->extend('PagerDefault')->limit(25);

  $results = $query->execute()->fetchAll();

  if (empty($results)) {
    $output['empty'] = array(
      '#markup' => '<p class="dropfiles-empty">' . t('No files found.') . '</p>',
    );
    return $output;
  }

  // Build table (list view).
  $header = array(
    array('data' => '', 'class' => array('dropfiles-icon-cell')),
    t('File'),
    t('Tags'),
    t('Size'),
    t('Uploaded'),
    t('Actions'),
  );

  $can_edit = user_access('upload dropfiles');
  $rows = array();
  $grid_cards = '';
  foreach ($results as $file) {
    $thumb = dropfiles_file_thumbnail($file, 32);
    $thumb_large = dropfiles_file_thumbnail($file, 120);

    // Tags.
    $tags = dropfiles_get_file_tags($file->fid);
    $tag_html = '';
    foreach ($tags as $tag) {
      $tag_html .= '<span class="dropfiles-tag">' . check_plain($tag->name) . '</span>';
    }

    // Actions.
    $file_url = file_create_url($file->uri);
    $actions = l(t('Details'), 'drop/files/' . $file->fid . '/view');
    if ($can_edit) {
      $actions .= ' ' . l(t('Edit'), 'drop/files/' . $file->fid . '/edit');
      $actions .= ' ' . l(t('Replace'), 'drop/files/' . $file->fid . '/replace');
      $actions .= ' ' . l(t('Delete'), 'drop/files/' . $file->fid . '/delete');
    }

    // Table row — file name links directly to the file (opens in browser).
    $rows[] = array(
      array('data' => $thumb, 'class' => array('dropfiles-icon-cell')),
      l($file->filename, $file_url, array('attributes' => array('target' => '_blank'))),
      $tag_html,
      format_size($file->filesize),
      format_date($file->timestamp, 'short'),
      array('data' => $actions, 'class' => array('dropfiles-actions-cell')),
    );

    // Grid card — links open the file directly in the browser.
    $grid_cards .= '<div class="dropfiles-grid-card">';
    $grid_cards .= '<a href="' . check_url($file_url) . '" target="_blank" class="dropfiles-grid-card-thumb">';
    $grid_cards .= $thumb_large;
    $grid_cards .= '</a>';
    $grid_cards .= '<div class="dropfiles-grid-card-info">';
    $grid_cards .= '<a href="' . check_url($file_url) . '" target="_blank" class="dropfiles-grid-card-name" title="' . check_plain($file->filename) . '">'
      . check_plain($file->filename) . '</a>';
    $grid_cards .= '<span class="dropfiles-grid-card-size">' . format_size($file->filesize) . '</span>';
    $grid_cards .= '</div>';
    $grid_cards .= '<div class="dropfiles-grid-card-actions">' . $actions . '</div>';
    $grid_cards .= '</div>';
  }

  // List view (table).
  $output['list_view'] = array(
    '#markup' => '<div class="dropfiles-list-view" style="display:none">'
      . theme('table', array(
        'header' => $header,
        'rows' => $rows,
        'attributes' => array('class' => array('dropfiles-table')),
      ))
      . '</div>',
  );

  // Grid view.
  $output['grid_view'] = array(
    '#markup' => '<div class="dropfiles-grid-view">'
      . '<div class="dropfiles-grid">' . $grid_cards . '</div>'
      . '</div>',
  );

  $output['pager'] = array(
    '#markup' => theme('pager'),
  );

  return $output;
}

/**
 * Form constructor: upload a file to the library.
 */
function dropfiles_upload_form($form, &$form_state) {
  backdrop_set_title(t('Upload file'));

  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(dropfiles_display_name(), 'drop/files'),
  ));

  $module_path = backdrop_get_path('module', 'dropfiles');
  $form['#attached']['css'][] = $module_path . '/dropfiles.css';

  $config = config('dropfiles.settings');
  $extensions = $config->get('allowed_extensions');
  $upload_dir = 'public://' . $config->get('upload_directory');

  $form['file'] = array(
    '#type' => 'managed_file',
    '#title' => t('File'),
    '#required' => TRUE,
    '#upload_location' => $upload_dir,
    '#upload_validators' => array(
      'file_validate_extensions' => array($extensions),
      'file_validate_size' => array(file_upload_max_size()),
    ),
    '#weight' => -20,
  );

  $form['tags'] = array(
    '#type' => 'textfield',
    '#title' => t('Tags'),
    '#description' => t('Optional tags. Separate multiple tags with commas.'),
    '#autocomplete_path' => 'taxonomy/autocomplete/field_file_tags',
    '#weight' => -10,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Upload'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'drop/files'),
  );

  return $form;
}

/**
 * Submit handler: save uploaded file to the library.
 */
function dropfiles_upload_form_submit($form, &$form_state) {
  $file = file_load($form_state['values']['file']);
  if (!$file) {
    backdrop_set_message(t('File upload failed.'), 'error');
    return;
  }

  // Make the file permanent.
  $file->status = FILE_STATUS_PERMANENT;
  file_save($file);

  // Register with the dropfiles library.
  file_usage_add($file, 'dropfiles', 'library', 1);

  // Generate PDF thumbnail if applicable.
  dropfiles_generate_pdf_thumbnail($file);

  // Save tags if provided.
  $tags_string = trim($form_state['values']['tags']);
  if ($tags_string !== '') {
    $tag_names = array_map('trim', explode(',', $tags_string));
    $tag_names = array_filter($tag_names, 'strlen');
    dropfiles_save_file_tags($file->fid, $tag_names);
  }

  backdrop_set_message(t('File !link uploaded.', array('!link' => l($file->filename, 'drop/files/' . $file->fid . '/view'))));
  $form_state['redirect'] = 'drop/files';
}

/**
 * Form constructor: edit file title and tags.
 *
 * @param object $file
 *   The file object to edit.
 */
function dropfiles_edit_form($form, &$form_state, $file) {
  backdrop_set_title(t('Edit @name', array('@name' => $file->filename)), PASS_THROUGH);

  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(dropfiles_display_name(), 'drop/files'),
    l($file->filename, 'drop/files/' . $file->fid . '/view'),
  ));

  $module_path = backdrop_get_path('module', 'dropfiles');
  $form['#attached']['css'][] = $module_path . '/dropfiles.css';

  $form['#file'] = $file;

  $form['filename'] = array(
    '#type' => 'textfield',
    '#title' => t('Filename'),
    '#default_value' => $file->filename,
    '#required' => TRUE,
    '#weight' => -20,
  );

  // Load existing tags as comma-separated string.
  $existing_tags = dropfiles_get_file_tags($file->fid);
  $tag_names = array();
  foreach ($existing_tags as $tag) {
    $tag_names[] = $tag->name;
  }

  $form['tags'] = array(
    '#type' => 'textfield',
    '#title' => t('Tags'),
    '#description' => t('Separate multiple tags with commas.'),
    '#default_value' => implode(', ', $tag_names),
    '#autocomplete_path' => 'taxonomy/autocomplete/field_file_tags',
    '#weight' => -10,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'drop/files/' . $file->fid . '/view'),
  );

  return $form;
}

/**
 * Submit handler: save file title and tags.
 */
function dropfiles_edit_form_submit($form, &$form_state) {
  $file = $form['#file'];

  // Update filename if changed.
  $new_name = trim($form_state['values']['filename']);
  if ($new_name !== $file->filename) {
    $file->filename = $new_name;
    file_save($file);
  }

  // Save tags.
  $tags_string = trim($form_state['values']['tags']);
  if ($tags_string !== '') {
    $tag_names = array_map('trim', explode(',', $tags_string));
    $tag_names = array_filter($tag_names, 'strlen');
  }
  else {
    $tag_names = array();
  }
  dropfiles_save_file_tags($file->fid, $tag_names);

  backdrop_set_message(t('File !link updated.', array('!link' => l($file->filename, 'drop/files/' . $file->fid . '/view'))));
  $form_state['redirect'] = 'drop/files/' . $file->fid . '/view';
}

/**
 * Page callback: view file details.
 *
 * @param object $file
 *   A file object from file_load().
 *
 * @return array
 *   A render array.
 */
function dropfiles_file_view_page($file) {
  backdrop_set_title(check_plain($file->filename));

  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(dropfiles_display_name(), 'drop/files'),
  ));

  $module_path = backdrop_get_path('module', 'dropfiles');

  $output = array(
    '#attached' => array(
      'css' => array($module_path . '/dropfiles.css'),
    ),
  );

  // Build detail display.
  $thumb = dropfiles_file_thumbnail($file, 300, 'medium');

  $tags = dropfiles_get_file_tags($file->fid);
  $tag_html = '';
  foreach ($tags as $tag) {
    $tag_html .= '<span class="dropfiles-tag">' . check_plain($tag->name) . '</span>';
  }

  $account = user_load($file->uid);
  $uploader = $account ? check_plain(format_username($account)) : t('Unknown');

  $download_url = file_create_url($file->uri);
  $file_type = dropfiles_human_type($file->filemime, $file->filename);
  $ext = strtoupper(pathinfo($file->filename, PATHINFO_EXTENSION));

  // Two-column layout: preview left, metadata right.
  $html = '<div class="dropfiles-detail-layout">';

  // Left: preview thumbnail (clickable — opens the file in the browser).
  $html .= '<div class="dropfiles-detail-preview">';
  $html .= '<a href="' . check_url($download_url) . '" target="_blank">' . $thumb . '</a>';
  $html .= '</div>';

  // Right: metadata.
  $html .= '<div class="dropfiles-detail-meta">';

  $html .= '<div class="dropfiles-meta-row">';
  $html .= '<span class="dropfiles-meta-label">' . t('Type') . '</span>';
  $html .= '<span class="dropfiles-meta-value">' . check_plain($file_type) . '</span>';
  $html .= '</div>';

  $html .= '<div class="dropfiles-meta-row">';
  $html .= '<span class="dropfiles-meta-label">' . t('Size') . '</span>';
  $html .= '<span class="dropfiles-meta-value">' . format_size($file->filesize) . '</span>';
  $html .= '</div>';

  $html .= '<div class="dropfiles-meta-row">';
  $html .= '<span class="dropfiles-meta-label">' . t('Uploaded') . '</span>';
  $html .= '<span class="dropfiles-meta-value">' . format_date($file->timestamp, 'short') . '</span>';
  $html .= '</div>';

  $html .= '<div class="dropfiles-meta-row">';
  $html .= '<span class="dropfiles-meta-label">' . t('Uploaded by') . '</span>';
  $html .= '<span class="dropfiles-meta-value">' . $uploader . '</span>';
  $html .= '</div>';

  if (!empty($tag_html)) {
    $html .= '<div class="dropfiles-meta-row">';
    $html .= '<span class="dropfiles-meta-label">' . t('Tags') . '</span>';
    $html .= '<span class="dropfiles-meta-value">' . $tag_html . '</span>';
    $html .= '</div>';
  }

  $html .= '<div class="dropfiles-detail-actions">';
  $html .= '<a href="' . check_url($download_url) . '" target="_blank" class="dropfiles-open-link">'
    . icon('eye', array('attributes' => array('width' => 16, 'height' => 16)))
    . ' ' . t('Open') . '</a>';
  $html .= '<a href="' . check_url($download_url) . '" download class="dropfiles-download-link">'
    . icon('download', array('attributes' => array('width' => 16, 'height' => 16)))
    . ' ' . t('Download') . '</a>';
  if (user_access('upload dropfiles')) {
    $html .= l(t('Edit'), 'drop/files/' . $file->fid . '/edit', array('attributes' => array('class' => array('dropfiles-action-link'))));
    $html .= l(t('Replace'), 'drop/files/' . $file->fid . '/replace', array('attributes' => array('class' => array('dropfiles-action-link'))));
    $html .= l(t('Delete'), 'drop/files/' . $file->fid . '/delete', array('attributes' => array('class' => array('dropfiles-action-link', 'dropfiles-action-delete'))));
  }
  $html .= '</div>';

  $html .= '</div>'; // .dropfiles-detail-meta
  $html .= '</div>'; // .dropfiles-detail-layout

  $output['detail'] = array(
    '#markup' => $html,
  );

  return $output;
}

/**
 * Form constructor: replace an existing file.
 *
 * @param object $file
 *   The file object to replace.
 */
function dropfiles_replace_form($form, &$form_state, $file) {
  backdrop_set_title(t('Replace @name', array('@name' => $file->filename)), PASS_THROUGH);

  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(dropfiles_display_name(), 'drop/files'),
    l($file->filename, 'drop/files/' . $file->fid . '/view'),
  ));

  $module_path = backdrop_get_path('module', 'dropfiles');
  $form['#attached']['css'][] = $module_path . '/dropfiles.css';

  $form['#file'] = $file;

  // Current file info.
  $form['current'] = array(
    '#markup' => '<div class="dropfiles-current-file">'
      . '<strong>' . t('Current file') . '</strong>'
      . check_plain($file->filename) . ' &mdash; '
      . format_size($file->filesize) . ' &mdash; '
      . format_date($file->timestamp, 'short')
      . '</div>',
    '#weight' => -30,
  );

  $config = config('dropfiles.settings');
  $extensions = $config->get('allowed_extensions');
  $upload_dir = 'public://' . $config->get('upload_directory');

  $form['new_file'] = array(
    '#type' => 'managed_file',
    '#title' => t('New file'),
    '#required' => TRUE,
    '#upload_location' => $upload_dir,
    '#upload_validators' => array(
      'file_validate_extensions' => array($extensions),
      'file_validate_size' => array(file_upload_max_size()),
    ),
    '#weight' => -20,
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Replace'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => ' ' . l(t('Cancel'), 'drop/files'),
  );

  return $form;
}

/**
 * Submit handler: replace the file content.
 */
function dropfiles_replace_form_submit($form, &$form_state) {
  $old_file = $form['#file'];
  $new_file = file_load($form_state['values']['new_file']);
  if (!$new_file) {
    backdrop_set_message(t('File upload failed.'), 'error');
    return;
  }

  // Copy new file content over the old file's URI.
  $old_uri = $old_file->uri;
  file_unmanaged_copy($new_file->uri, $old_uri, FILE_EXISTS_REPLACE);

  // Update old file's metadata.
  $old_file->filesize = filesize(backdrop_realpath($old_uri));
  $old_file->filemime = $new_file->filemime;
  $old_file->timestamp = REQUEST_TIME;
  file_save($old_file);

  // Remove the temporary new file.
  file_delete($new_file->fid);

  // Regenerate PDF thumbnail if the replacement is a PDF.
  $thumb_uri = dropfiles_pdf_thumbnail_uri($old_file->fid);
  if (file_exists($thumb_uri)) {
    file_unmanaged_delete($thumb_uri);
  }
  dropfiles_generate_pdf_thumbnail($old_file);

  backdrop_set_message(t('File !link has been replaced.', array('!link' => l($old_file->filename, 'drop/files/' . $old_file->fid . '/view'))));
  $form_state['redirect'] = 'drop/files';
}

/**
 * Form constructor: confirm file deletion.
 *
 * @param object $file
 *   The file object to delete.
 */
function dropfiles_delete_form($form, &$form_state, $file) {
  $form['#file'] = $file;
  return confirm_form(
    $form,
    t('Delete %name?', array('%name' => $file->filename)),
    'drop/files',
    t('This will permanently delete the file. This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler: delete a file from the library.
 */
function dropfiles_delete_form_submit($form, &$form_state) {
  $file = $form['#file'];

  // Remove file usage tracking.
  file_usage_delete($file, 'dropfiles', 'library', 1);

  // Remove PDF thumbnail if it exists.
  $thumb_uri = dropfiles_pdf_thumbnail_uri($file->fid);
  if (file_exists($thumb_uri)) {
    file_unmanaged_delete($thumb_uri);
  }

  // Delete the file.
  file_delete($file->fid);

  backdrop_set_message(t('File %name has been deleted.', array('%name' => $file->filename)));
  $form_state['redirect'] = 'drop/files';
}

/**
 * Form constructor: DropFiles settings.
 */
function dropfiles_settings_form($form, &$form_state) {
  backdrop_set_title(t('Settings'));

  backdrop_set_breadcrumb(array(
    l(t('Home'), '<front>'),
    l(t('Drop Suite'), 'drop'),
    l(dropfiles_display_name(), 'drop/files'),
  ));

  $module_path = backdrop_get_path('module', 'dropfiles');
  $form['#attached']['css'][] = $module_path . '/dropfiles.css';

  $config = config('dropfiles.settings');
  $form['#config'] = 'dropfiles.settings';

  // Display settings.
  $form['display'] = array(
    '#type' => 'fieldset',
    '#title' => t('Display'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['display']['display_name'] = array(
    '#type' => 'select',
    '#title' => t('Display name'),
    '#description' => t('Choose how this module appears in navigation and headings.'),
    '#options' => array(
      'Files' => t('Files'),
      'Documents' => t('Documents'),
      'Resources' => t('Resources'),
      'Library' => t('Library'),
    ),
    '#default_value' => $config->get('display_name'),
  );

  // Upload settings.
  $form['uploads'] = array(
    '#type' => 'fieldset',
    '#title' => t('Uploads'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['uploads']['allowed_extensions'] = array(
    '#type' => 'textfield',
    '#title' => t('Allowed file extensions'),
    '#description' => t('Separate extensions with a space, e.g. "pdf doc docx xls xlsx png jpg".'),
    '#default_value' => $config->get('allowed_extensions'),
  );
  $form['uploads']['upload_directory'] = array(
    '#type' => 'textfield',
    '#title' => t('Upload directory'),
    '#description' => t('Subdirectory within the public files directory. Do not include a leading slash.'),
    '#default_value' => $config->get('upload_directory'),
  );

  $form['actions'] = array('#type' => 'actions', '#weight' => 99);
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save settings'),
  );

  return $form;
}

/**
 * Submit handler: save DropFiles settings.
 */
function dropfiles_settings_form_submit($form, &$form_state) {
  $config = config('dropfiles.settings');
  $config->set('display_name', $form_state['values']['display_name']);
  $config->set('allowed_extensions', $form_state['values']['allowed_extensions']);
  $config->set('upload_directory', $form_state['values']['upload_directory']);
  $config->save();

  backdrop_set_message(t('Settings saved.'));
  $form_state['redirect'] = 'drop/files';
}

// ---------------------------------------------------------------------------
// AJAX upload handler
// ---------------------------------------------------------------------------

/**
 * AJAX callback: handles drag-and-drop file uploads.
 *
 * Expects a POST with a 'file' field and a 'token' query parameter.
 * Returns JSON: {ok: true, filename: '...'} or {error: '...'}.
 */
function dropfiles_ajax_upload() {
  // Verify CSRF token.
  if (empty($_GET['token']) || !backdrop_valid_token($_GET['token'], 'dropfiles_ajax_upload')) {
    watchdog('dropfiles', 'AJAX upload rejected: invalid CSRF token.', array(), WATCHDOG_WARNING);
    backdrop_json_output(array('error' => t('Invalid token.')));
    backdrop_exit();
  }

  if (empty($_FILES['file'])) {
    watchdog('dropfiles', 'AJAX upload failed: no file in request.', array(), WATCHDOG_WARNING);
    backdrop_json_output(array('error' => t('No file received.')));
    backdrop_exit();
  }

  $upload = $_FILES['file'];
  if ($upload['error'] !== UPLOAD_ERR_OK) {
    $error_map = array(
      UPLOAD_ERR_INI_SIZE   => 'exceeds upload_max_filesize',
      UPLOAD_ERR_FORM_SIZE  => 'exceeds MAX_FILE_SIZE',
      UPLOAD_ERR_PARTIAL    => 'partially uploaded',
      UPLOAD_ERR_NO_FILE    => 'no file sent',
      UPLOAD_ERR_NO_TMP_DIR => 'missing temp directory',
      UPLOAD_ERR_CANT_WRITE => 'failed to write to disk',
      UPLOAD_ERR_EXTENSION  => 'blocked by PHP extension',
    );
    $reason = isset($error_map[$upload['error']]) ? $error_map[$upload['error']] : 'code ' . $upload['error'];
    watchdog('dropfiles', 'AJAX upload failed for %name: @reason', array(
      '%name' => $upload['name'],
      '@reason' => $reason,
    ), WATCHDOG_ERROR);
    backdrop_json_output(array('error' => t('Upload error: @reason', array('@reason' => $reason))));
    backdrop_exit();
  }

  // Validate file extension.
  $config = config('dropfiles.settings');
  $allowed = $config->get('allowed_extensions');
  $ext = strtolower(pathinfo($upload['name'], PATHINFO_EXTENSION));
  $allowed_list = array_filter(array_map('trim', explode(' ', $allowed)));
  if (!empty($allowed_list) && !in_array($ext, $allowed_list)) {
    watchdog('dropfiles', 'AJAX upload rejected %name: extension .@ext not allowed.', array(
      '%name' => $upload['name'],
      '@ext' => $ext,
    ), WATCHDOG_WARNING);
    backdrop_json_output(array('error' => t('File type .@ext is not allowed.', array('@ext' => $ext))));
    backdrop_exit();
  }

  // Save the file via Backdrop's managed file API.
  // Backdrop's file_save_upload() expects $_FILES['files']['name'][$key], not
  // the flat $_FILES['file'] structure that XHR FormData produces. Restructure.
  if (!isset($_FILES['files'])) {
    $_FILES['files'] = array(
      'name' => array(),
      'type' => array(),
      'tmp_name' => array(),
      'error' => array(),
      'size' => array(),
    );
  }
  $_FILES['files']['name']['file'] = $upload['name'];
  $_FILES['files']['type']['file'] = $upload['type'];
  $_FILES['files']['tmp_name']['file'] = $upload['tmp_name'];
  $_FILES['files']['error']['file'] = $upload['error'];
  $_FILES['files']['size']['file'] = $upload['size'];

  $upload_dir = 'public://' . $config->get('upload_directory');
  file_prepare_directory($upload_dir, FILE_CREATE_DIRECTORY);

  $file = file_save_upload('file', array(
    'file_validate_extensions' => array($allowed),
    'file_validate_size' => array(file_upload_max_size()),
  ), $upload_dir, FILE_EXISTS_RENAME);

  if (!$file) {
    watchdog('dropfiles', 'AJAX upload failed: file_save_upload returned FALSE for %name.', array(
      '%name' => $upload['name'],
    ), WATCHDOG_ERROR);
    backdrop_json_output(array('error' => t('File could not be saved.')));
    backdrop_exit();
  }

  // Make permanent and register with the library.
  $file->status = FILE_STATUS_PERMANENT;
  file_save($file);
  file_usage_add($file, 'dropfiles', 'library', 1);

  // Generate PDF thumbnail if applicable.
  dropfiles_generate_pdf_thumbnail($file);

  watchdog('dropfiles', 'File %name uploaded via drag-and-drop (@size).', array(
    '%name' => $file->filename,
    '@size' => format_size($file->filesize),
  ), WATCHDOG_INFO);

  backdrop_json_output(array('ok' => TRUE, 'filename' => $file->filename));
  backdrop_exit();
}

/**
 * Autocomplete callback: search library files by filename.
 *
 * Returns JSON keyed by "filename [fid:N]" for autocomplete matching.
 *
 * @param string $string
 *   The partial filename to search for.
 */
function dropfiles_autocomplete($string = '') {
  $matches = array();

  if (strlen($string) < 2) {
    backdrop_json_output($matches);
    return;
  }

  $query = db_select('file_managed', 'f')
    ->fields('f', array('fid', 'filename', 'filesize', 'filemime'))
    ->condition('f.status', 1)
    ->condition('f.filename', '%' . db_like($string) . '%', 'LIKE');
  $query->innerJoin('file_usage', 'fu', 'fu.fid = f.fid AND fu.module = :mod AND fu.type = :type', array(':mod' => 'dropfiles', ':type' => 'library'));
  $query->orderBy('f.filename');
  $query->range(0, 10);

  $results = $query->execute()->fetchAll();
  foreach ($results as $file) {
    $key = check_plain($file->filename) . ' [fid:' . $file->fid . ']';
    $matches[$key] = check_plain($file->filename) . ' (' . format_size($file->filesize) . ')';
  }

  backdrop_json_output($matches);
}

