<?php
/**
 * @file
 * S3 archive browser for Drop Monitor.
 */

/**
 * Returns a configured DropS3 client, or NULL if not configured.
 */
function _dropmonitor_s3_client() {
  $config = config('dropmonitor.settings');
  $key = $config->get('s3_access_key');
  $secret = $config->get('s3_secret_key');
  $bucket = $config->get('s3_bucket');

  if (empty($key) || empty($secret) || empty($bucket)) {
    return NULL;
  }

  require_once dirname(__FILE__) . '/lib/DropS3.php';
  $region = $config->get('s3_region') ?: 'us-east-1';
  $endpoint = $config->get('s3_endpoint') ?: NULL;

  return new DropS3($key, $secret, $region, $endpoint);
}

/**
 * Formats a byte count as a human-readable string.
 */
function _dropmonitor_format_size($bytes) {
  if ($bytes >= 1073741824) {
    return number_format($bytes / 1073741824, 1) . ' GB';
  }
  elseif ($bytes >= 1048576) {
    return number_format($bytes / 1048576, 1) . ' MB';
  }
  elseif ($bytes >= 1024) {
    return number_format($bytes / 1024, 0) . ' KB';
  }
  return $bytes . ' B';
}

/**
 * Returns the maximum upload size in bytes based on PHP settings.
 */
function _dropmonitor_max_upload_size() {
  $upload_max = _dropmonitor_parse_size(ini_get('upload_max_filesize'));
  $post_max = _dropmonitor_parse_size(ini_get('post_max_size'));
  return min($upload_max, $post_max);
}

/**
 * Parses a PHP size string (e.g. '256M') into bytes.
 */
function _dropmonitor_parse_size($size_str) {
  $size_str = trim($size_str);
  $unit = strtolower(substr($size_str, -1));
  $value = (int) $size_str;
  switch ($unit) {
    case 'g':
      $value *= 1024 * 1024 * 1024;
      break;
    case 'm':
      $value *= 1024 * 1024;
      break;
    case 'k':
      $value *= 1024;
      break;
  }
  return $value;
}

/**
 * Shutdown function to catch fatal errors during S3 uploads.
 */
function _dropmonitor_upload_shutdown() {
  $error = error_get_last();
  if ($error && in_array($error['type'], array(E_ERROR, E_CORE_ERROR, E_COMPILE_ERROR))) {
    watchdog('dropmonitor', 'S3 upload killed by fatal error: @message (in @file line @line)', array(
      '@message' => $error['message'],
      '@file' => $error['file'],
      '@line' => $error['line'],
    ), WATCHDOG_ERROR);
  }
}

// -------------------------------------------------------------------------
// Archives page
// -------------------------------------------------------------------------

/**
 * Page callback: displays the S3 archive browser.
 */
function dropmonitor_archives_page() {
  backdrop_set_title(t('Archives'));

  $config = config('dropmonitor.settings');
  $bucket = $config->get('s3_bucket');

  if (empty($config->get('s3_access_key')) || empty($config->get('s3_secret_key')) || empty($bucket)) {
    return array(
      '#markup' => '<p>' . t('S3 is not configured. <a href="@url">Configure S3 settings</a> to use the archive browser.', array(
        '@url' => url('drop/monitor/settings'),
      )) . '</p>',
    );
  }

  // Current folder prefix from query string.
  $prefix = isset($_GET['prefix']) ? $_GET['prefix'] : '';

  $s3 = _dropmonitor_s3_client();
  $error = NULL;
  $result = array('objects' => array(), 'prefixes' => array());

  try {
    $result = $s3->listObjects($bucket, $prefix, '/');
  }
  catch (Exception $e) {
    $error = $e->getMessage();
  }

  $objects = $result['objects'];
  $prefixes = $result['prefixes'];

  $output = array(
    '#attached' => array(
      'css' => array(backdrop_get_path('module', 'dropmonitor') . '/dropmonitor.css'),
      'js' => array(
        array(
          'data' => backdrop_get_path('module', 'dropmonitor') . '/dropmonitor.archives.js',
          'type' => 'file',
        ),
        array(
          'data' => array(
            'dropmonitorArchives' => array(
              'uploadUrl' => url('drop/monitor/archives/upload'),
              'token' => backdrop_get_token('dropmonitor_archive_upload'),
              'prefix' => $prefix,
            ),
          ),
          'type' => 'setting',
        ),
      ),
    ),
  );

  // Breadcrumb navigation.
  $output['breadcrumb'] = array(
    '#markup' => _dropmonitor_archives_breadcrumb($prefix, $bucket),
    '#weight' => -15,
  );

  // New folder form.
  $new_folder_url = url('drop/monitor/archives/new-folder');
  $new_folder_token = backdrop_get_token('dropmonitor_new_folder');
  $output['new_folder'] = array(
    '#markup' => '<div class="dropmonitor-new-folder">'
      . '<form class="dropmonitor-new-folder-form" method="post" action="' . $new_folder_url . '">'
      . '<input type="hidden" name="token" value="' . $new_folder_token . '">'
      . '<input type="hidden" name="prefix" value="' . check_plain($prefix) . '">'
      . '<input type="text" name="folder_name" placeholder="' . t('New folder name') . '" class="dropmonitor-new-folder-input" required>'
      . '<button type="submit" class="dropmonitor-new-folder-btn">'
      . icon('folder-plus', array('attributes' => array('width' => 16, 'height' => 16)))
      . ' ' . t('Create folder') . '</button>'
      . '</form>'
      . '</div>',
    '#weight' => -12,
  );

  // Drop zone.
  $upload_label = $prefix !== ''
    ? t('Drag and drop files here to upload to %folder', array('%folder' => rtrim($prefix, '/')))
    : t('Drag and drop files here to upload to S3');
  $output['dropzone'] = array(
    '#markup' => '<div id="dropmonitor-dropzone" class="dropmonitor-dropzone">'
      . '<div class="dropmonitor-dropzone-inner">'
      . icon('upload-simple', array('attributes' => array('width' => 32, 'height' => 32)))
      . '<p>' . $upload_label . '</p>'
      . '<p class="dropmonitor-dropzone-hint">' . t('or click to select files') . '</p>'
      . '<p class="dropmonitor-dropzone-hint">' . t('Maximum file size: @size', array('@size' => format_size(_dropmonitor_max_upload_size()))) . '</p>'
      . '<input type="file" id="dropmonitor-file-input" multiple style="display:none">'
      . '</div>'
      . '<div class="dropmonitor-upload-progress" style="display:none">'
      . '<div class="dropmonitor-progress-text"></div>'
      . '<div class="dropmonitor-progress-bar"><div class="dropmonitor-progress-bar-fill"></div></div>'
      . '<div class="dropmonitor-progress-detail"></div>'
      . '</div>'
      . '</div>',
    '#weight' => -10,
  );

  if ($error) {
    $output['error'] = array(
      '#markup' => '<div class="messages error">' . check_plain($error) . '</div>',
      '#weight' => -5,
    );
    return $output;
  }

  if (empty($prefixes) && empty($objects)) {
    $output['empty'] = array(
      '#markup' => '<p class="dropmonitor-empty">' . t('No files in this location.') . '</p>',
    );
    return $output;
  }

  $rows = array();

  // Folders first.
  foreach ($prefixes as $folder_prefix) {
    $folder_name = rtrim($folder_prefix, '/');
    // Strip the current prefix to show only the folder name.
    if ($prefix !== '') {
      $folder_name = substr($folder_name, strlen($prefix));
    }

    // Count items inside the folder.
    $item_count = '';
    try {
      $folder_result = $s3->listObjects($bucket, $folder_prefix, '/');
      $count = count($folder_result['objects']) + count($folder_result['prefixes']);
      $item_count = format_plural($count, '1 item', '@count items');
    }
    catch (Exception $e) {
      // Silently skip count on error.
    }

    $rows[] = array(
      array(
        'data' => icon('folder', array('attributes' => array('width' => 16, 'height' => 16)))
          . ' ' . l(check_plain($folder_name), 'drop/monitor/archives', array(
            'html' => TRUE,
            'query' => array('prefix' => $folder_prefix),
          )),
        'class' => array('dropmonitor-folder-cell'),
      ),
      '<span class="dropmonitor-folder-count">' . $item_count . '</span>',
      '',
      l(t('Delete'), 'drop/monitor/archives/delete', array(
        'query' => array('key' => $folder_prefix),
        'attributes' => array('class' => array('dropmonitor-delete-link')),
      )),
    );
  }

  // Sort files by last modified descending (newest first).
  usort($objects, function ($a, $b) {
    return strcmp($b['last_modified'], $a['last_modified']);
  });

  foreach ($objects as $obj) {
    $key = $obj['key'];
    // Show only the filename, not the full path.
    $display_name = $prefix !== '' ? substr($key, strlen($prefix)) : $key;
    $modified = !empty($obj['last_modified'])
      ? format_date(strtotime($obj['last_modified']), 'short')
      : '';

    $rows[] = array(
      icon('file', array('attributes' => array('width' => 16, 'height' => 16)))
        . ' ' . check_plain($display_name),
      _dropmonitor_format_size($obj['size']),
      $modified,
      l(t('Download'), 'drop/monitor/archives/download', array(
        'query' => array('key' => $key),
      )) . ' &nbsp; '
        . l(t('Move'), 'drop/monitor/archives/move', array(
          'query' => array('key' => $key),
        )) . ' &nbsp; '
        . l(t('Delete'), 'drop/monitor/archives/delete', array(
          'query' => array('key' => $key),
          'attributes' => array('class' => array('dropmonitor-delete-link')),
        )),
    );
  }

  $output['table'] = array(
    '#theme' => 'table',
    '#header' => array(t('File'), t('Size'), t('Uploaded'), t('Actions')),
    '#rows' => $rows,
    '#attributes' => array('class' => array('dropmonitor-archives-table')),
  );

  $total = count($objects) + count($prefixes);
  $output['count'] = array(
    '#markup' => '<p class="dropmonitor-archive-count">'
      . format_plural($total, '1 item', '@count items')
      . '</p>',
    '#weight' => 10,
  );

  return $output;
}

/**
 * Builds breadcrumb HTML for the archive folder path.
 */
function _dropmonitor_archives_breadcrumb($prefix, $bucket) {
  $parts = array();
  $parts[] = l(check_plain($bucket), 'drop/monitor/archives', array('html' => TRUE));

  if ($prefix !== '') {
    $segments = explode('/', rtrim($prefix, '/'));
    $cumulative = '';
    foreach ($segments as $segment) {
      $cumulative .= $segment . '/';
      $parts[] = l(check_plain($segment), 'drop/monitor/archives', array(
        'query' => array('prefix' => $cumulative),
        'html' => TRUE,
      ));
    }
  }

  return '<nav class="dropmonitor-breadcrumb">'
    . icon('folder-open', array('attributes' => array('width' => 16, 'height' => 16)))
    . ' ' . implode(' / ', $parts) . '</nav>';
}

// -------------------------------------------------------------------------
// Upload handler (AJAX)
// -------------------------------------------------------------------------

/**
 * AJAX callback: receives an uploaded file and pushes it to S3.
 */
function dropmonitor_archives_upload() {
  // Allow long-running uploads to S3 without PHP killing the process.
  if (function_exists('set_time_limit')) {
    @set_time_limit(600);
  }
  // Disable output buffering so the connection stays alive.
  if (function_exists('apache_setenv')) {
    @apache_setenv('no-gzip', '1');
  }

  // Catch fatal errors (e.g. timeout) that would otherwise die silently.
  register_shutdown_function('_dropmonitor_upload_shutdown');

  // Verify token.
  if (empty($_GET['token']) || !backdrop_valid_token($_GET['token'], 'dropmonitor_archive_upload')) {
    watchdog('dropmonitor', 'S3 upload rejected: invalid CSRF token.', array(), WATCHDOG_WARNING);
    backdrop_json_output(array('error' => t('Invalid token.')));
    backdrop_exit();
  }

  if (empty($_FILES['file'])) {
    watchdog('dropmonitor', 'S3 upload failed: no file in request.', array(), WATCHDOG_WARNING);
    backdrop_json_output(array('error' => t('No file received.')));
    backdrop_exit();
  }

  $file = $_FILES['file'];
  if ($file['error'] !== UPLOAD_ERR_OK) {
    $error_map = array(
      UPLOAD_ERR_INI_SIZE => 'exceeds php.ini upload_max_filesize',
      UPLOAD_ERR_FORM_SIZE => 'exceeds form MAX_FILE_SIZE',
      UPLOAD_ERR_PARTIAL => 'partially uploaded',
      UPLOAD_ERR_NO_FILE => 'no file sent',
      UPLOAD_ERR_NO_TMP_DIR => 'missing temp directory',
      UPLOAD_ERR_CANT_WRITE => 'failed to write to disk',
      UPLOAD_ERR_EXTENSION => 'blocked by PHP extension',
    );
    $reason = isset($error_map[$file['error']]) ? $error_map[$file['error']] : 'code ' . $file['error'];
    watchdog('dropmonitor', 'S3 upload failed for %name: @reason', array(
      '%name' => $file['name'],
      '@reason' => $reason,
    ), WATCHDOG_ERROR);
    backdrop_json_output(array('error' => t('Upload error: @reason', array('@reason' => $reason))));
    backdrop_exit();
  }

  $s3 = _dropmonitor_s3_client();
  if (!$s3) {
    watchdog('dropmonitor', 'S3 upload failed: S3 not configured.', array(), WATCHDOG_ERROR);
    backdrop_json_output(array('error' => t('S3 is not configured.')));
    backdrop_exit();
  }

  $bucket = config_get('dropmonitor.settings', 's3_bucket');
  $prefix = isset($_GET['prefix']) ? $_GET['prefix'] : '';
  $key = $prefix . basename($file['name']);

  // Determine content type.
  $content_type = 'application/octet-stream';
  $ext = strtolower(pathinfo($key, PATHINFO_EXTENSION));
  $type_map = array(
    'tar' => 'application/x-tar',
    'gz' => 'application/gzip',
    'tgz' => 'application/gzip',
    'zip' => 'application/zip',
    'sql' => 'application/sql',
    'bz2' => 'application/x-bzip2',
  );
  if (isset($type_map[$ext])) {
    $content_type = $type_map[$ext];
  }

  try {
    $fh = fopen($file['tmp_name'], 'rb');
    if (!$fh) {
      throw new RuntimeException('Could not open uploaded file.');
    }
    $s3->setTimeout(300);
    $s3->put($bucket, $key, $fh, array('content-type' => $content_type));
    fclose($fh);

    backdrop_json_output(array(
      'success' => TRUE,
      'message' => t('Uploaded %file to S3.', array('%file' => $key)),
    ));
  }
  catch (Exception $e) {
    watchdog('dropmonitor', 'S3 upload failed for %file: @error', array(
      '%file' => $key,
      '@error' => $e->getMessage(),
    ), WATCHDOG_ERROR);
    backdrop_json_output(array('error' => $e->getMessage()));
  }

  backdrop_exit();
}

// -------------------------------------------------------------------------
// New folder
// -------------------------------------------------------------------------

/**
 * Page callback: creates a new "folder" in S3.
 */
function dropmonitor_archives_new_folder() {
  watchdog('dropmonitor', 'New folder handler called. Method: @method, POST keys: @keys', array(
    '@method' => $_SERVER['REQUEST_METHOD'],
    '@keys' => implode(', ', array_keys($_POST)),
  ), WATCHDOG_DEBUG);

  if (empty($_POST['token']) || !backdrop_valid_token($_POST['token'], 'dropmonitor_new_folder')) {
    watchdog('dropmonitor', 'New folder rejected: invalid token.', array(), WATCHDOG_WARNING);
    backdrop_set_message(t('Invalid token.'), 'error');
    backdrop_goto('drop/monitor/archives');
    return;
  }

  $folder_name = isset($_POST['folder_name']) ? trim($_POST['folder_name']) : '';
  $prefix = isset($_POST['prefix']) ? $_POST['prefix'] : '';

  if (empty($folder_name)) {
    watchdog('dropmonitor', 'New folder rejected: empty name.', array(), WATCHDOG_WARNING);
    backdrop_set_message(t('Folder name is required.'), 'error');
    backdrop_goto('drop/monitor/archives', array('query' => array('prefix' => $prefix)));
    return;
  }

  // Sanitize: strip slashes and dangerous characters.
  $folder_name = str_replace(array('/', '\\', "\0"), '', $folder_name);

  $s3 = _dropmonitor_s3_client();
  if (!$s3) {
    watchdog('dropmonitor', 'New folder failed: S3 not configured.', array(), WATCHDOG_ERROR);
    backdrop_set_message(t('S3 is not configured.'), 'error');
    backdrop_goto('drop/monitor/archives');
    return;
  }

  $bucket = config_get('dropmonitor.settings', 's3_bucket');
  $key = $prefix . $folder_name . '/';

  try {
    $s3->put($bucket, $key, '', array('content-type' => 'application/x-directory'));
    watchdog('dropmonitor', 'Created S3 folder: %key', array('%key' => $key), WATCHDOG_INFO);
    backdrop_set_message(t('Created folder %name.', array('%name' => $folder_name)));
  }
  catch (Exception $e) {
    watchdog('dropmonitor', 'S3 folder creation failed for %key: @error', array(
      '%key' => $key,
      '@error' => $e->getMessage(),
    ), WATCHDOG_ERROR);
    backdrop_set_message(t('Failed to create folder: @error', array('@error' => $e->getMessage())), 'error');
  }

  $redirect_query = !empty($prefix) ? array('query' => array('prefix' => $prefix)) : array();
  backdrop_goto('drop/monitor/archives', $redirect_query);
}

// -------------------------------------------------------------------------
// Download proxy
// -------------------------------------------------------------------------

/**
 * Page callback: proxies an S3 download to the browser.
 */
function dropmonitor_archives_download() {
  $key = isset($_GET['key']) ? $_GET['key'] : '';
  if (empty($key)) {
    backdrop_not_found();
    return;
  }

  $s3 = _dropmonitor_s3_client();
  if (!$s3) {
    backdrop_set_message(t('S3 is not configured.'), 'error');
    backdrop_goto('drop/monitor/archives');
    return;
  }

  $bucket = config_get('dropmonitor.settings', 's3_bucket');
  $filename = basename($key);
  $disposition = 'attachment; filename="' . $filename . '"';

  try {
    $url = $s3->getPresignedUrl($bucket, $key, 300, $disposition);
    header('Location: ' . $url, TRUE, 302);
    backdrop_exit();
  }
  catch (Exception $e) {
    watchdog('dropmonitor', 'S3 download failed for %key: @error', array(
      '%key' => $key,
      '@error' => $e->getMessage(),
    ), WATCHDOG_ERROR);
    backdrop_set_message(t('Download failed: @error', array('@error' => $e->getMessage())), 'error');
    backdrop_goto('drop/monitor/archives');
  }
}

// -------------------------------------------------------------------------
// Delete confirmation form
// -------------------------------------------------------------------------

/**
 * Confirm form for deleting an archive from S3.
 */
function dropmonitor_archive_delete_form($form, &$form_state) {
  $key = isset($_GET['key']) ? $_GET['key'] : '';
  if (empty($key)) {
    backdrop_not_found();
    return;
  }

  $is_folder = (substr($key, -1) === '/');

  // For folders, check if empty before allowing delete.
  if ($is_folder) {
    $s3 = _dropmonitor_s3_client();
    if ($s3) {
      $bucket = config_get('dropmonitor.settings', 's3_bucket');
      try {
        $result = $s3->listObjects($bucket, $key, '/');
        if (!empty($result['objects']) || !empty($result['prefixes'])) {
          backdrop_set_message(t('Cannot delete folder %name because it is not empty. Delete its contents first.', array(
            '%name' => rtrim($key, '/'),
          )), 'error');
          // Redirect to parent folder.
          $parent = dirname(rtrim($key, '/'));
          $parent_prefix = ($parent === '.') ? '' : $parent . '/';
          $query = !empty($parent_prefix) ? array('query' => array('prefix' => $parent_prefix)) : array();
          backdrop_goto('drop/monitor/archives', $query);
          return;
        }
      }
      catch (Exception $e) {
        // If we can't check, let the user proceed.
      }
    }
  }

  $form['key'] = array(
    '#type' => 'value',
    '#value' => $key,
  );

  // Build cancel path back to the parent folder.
  if ($is_folder) {
    $parent = dirname(rtrim($key, '/'));
    $cancel_prefix = ($parent === '.') ? '' : $parent . '/';
    $display_name = rtrim($key, '/');
    if ($cancel_prefix !== '') {
      $display_name = substr($display_name, strlen($cancel_prefix));
    }
    $description = t('This will delete the empty folder from the S3 bucket.');
  }
  else {
    $cancel_prefix = '';
    $last_slash = strrpos($key, '/');
    if ($last_slash !== FALSE) {
      $cancel_prefix = substr($key, 0, $last_slash + 1);
    }
    $display_name = basename($key);
    $description = t('This will permanently delete the file from the S3 bucket. This action cannot be undone.');
  }

  $cancel_path = !empty($cancel_prefix)
    ? url('drop/monitor/archives', array('query' => array('prefix' => $cancel_prefix)))
    : 'drop/monitor/archives';

  return confirm_form(
    $form,
    t('Delete %file from the archive?', array('%file' => $display_name)),
    $cancel_path,
    $description,
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for archive delete confirmation.
 */
function dropmonitor_archive_delete_form_submit($form, &$form_state) {
  $key = $form_state['values']['key'];

  $s3 = _dropmonitor_s3_client();
  if (!$s3) {
    backdrop_set_message(t('S3 is not configured.'), 'error');
    $form_state['redirect'] = 'drop/monitor/archives';
    return;
  }

  $bucket = config_get('dropmonitor.settings', 's3_bucket');

  try {
    $s3->delete($bucket, $key);
    watchdog('dropmonitor', 'Deleted S3 object: %key', array('%key' => $key), WATCHDOG_INFO);
    backdrop_set_message(t('Deleted %file from the archive.', array('%file' => basename($key))));
  }
  catch (Exception $e) {
    watchdog('dropmonitor', 'S3 delete failed for %key: @error', array(
      '%key' => $key,
      '@error' => $e->getMessage(),
    ), WATCHDOG_ERROR);
    backdrop_set_message(t('Delete failed: @error', array('@error' => $e->getMessage())), 'error');
  }

  // Redirect back to the folder the file was in.
  $prefix = '';
  $last_slash = strrpos($key, '/');
  if ($last_slash !== FALSE) {
    $prefix = substr($key, 0, $last_slash + 1);
  }
  $redirect_query = !empty($prefix) ? array('query' => array('prefix' => $prefix)) : array();
  $form_state['redirect'] = array('drop/monitor/archives', $redirect_query);
}

// -------------------------------------------------------------------------
// Move file to folder
// -------------------------------------------------------------------------

/**
 * Helper: recursively collects all folder prefixes in a bucket.
 */
function _dropmonitor_list_all_folders($s3, $bucket, $prefix = '') {
  $folders = array();
  try {
    $result = $s3->listObjects($bucket, $prefix, '/');
    foreach ($result['prefixes'] as $folder_prefix) {
      $folders[] = $folder_prefix;
      // Recurse into subfolders.
      $subfolders = _dropmonitor_list_all_folders($s3, $bucket, $folder_prefix);
      $folders = array_merge($folders, $subfolders);
    }
  }
  catch (Exception $e) {
    // Skip on error.
  }
  return $folders;
}

/**
 * Form: select a destination folder to move a file into.
 */
function dropmonitor_archive_move_form($form, &$form_state) {
  $key = isset($_GET['key']) ? $_GET['key'] : '';
  if (empty($key)) {
    backdrop_not_found();
    return;
  }

  $s3 = _dropmonitor_s3_client();
  if (!$s3) {
    backdrop_set_message(t('S3 is not configured.'), 'error');
    backdrop_goto('drop/monitor/archives');
    return;
  }

  $bucket = config_get('dropmonitor.settings', 's3_bucket');
  $filename = basename($key);

  // Current folder of the file.
  $current_prefix = '';
  $last_slash = strrpos($key, '/');
  if ($last_slash !== FALSE) {
    $current_prefix = substr($key, 0, $last_slash + 1);
  }

  // Get all folders in the bucket.
  $all_folders = _dropmonitor_list_all_folders($s3, $bucket);

  $options = array('__root__' => t('/ (bucket root)'));
  foreach ($all_folders as $folder) {
    // Skip the current folder.
    if ($folder === $current_prefix) {
      continue;
    }
    $options[$folder] = rtrim($folder, '/');
  }

  backdrop_set_title(t('Move @file', array('@file' => $filename)), PASS_THROUGH);

  $form['key'] = array(
    '#type' => 'value',
    '#value' => $key,
  );

  $form['info'] = array(
    '#markup' => '<p>' . t('Moving <strong>@file</strong> from <strong>@from</strong>', array(
      '@file' => $filename,
      '@from' => $current_prefix !== '' ? rtrim($current_prefix, '/') : t('bucket root'),
    )) . '</p>',
  );

  $form['destination'] = array(
    '#type' => 'select',
    '#title' => t('Move to'),
    '#options' => $options,
    '#default_value' => ($current_prefix === '') ? '__root__' : $current_prefix,
  );

  $cancel_path = !empty($current_prefix)
    ? url('drop/monitor/archives', array('query' => array('prefix' => $current_prefix)))
    : 'drop/monitor/archives';

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Move'),
  );
  $form['actions']['cancel'] = array(
    '#markup' => l(t('Cancel'), $cancel_path),
  );

  return $form;
}

/**
 * Submit handler for the move form.
 */
function dropmonitor_archive_move_form_submit($form, &$form_state) {
  $key = $form_state['values']['key'];
  $destination = $form_state['values']['destination'];
  if ($destination === '__root__') {
    $destination = '';
  }
  $filename = basename($key);
  $new_key = $destination . $filename;

  if ($new_key === $key) {
    backdrop_set_message(t('File is already in that location.'), 'warning');
    $form_state['redirect'] = 'drop/monitor/archives';
    return;
  }

  $s3 = _dropmonitor_s3_client();
  if (!$s3) {
    backdrop_set_message(t('S3 is not configured.'), 'error');
    $form_state['redirect'] = 'drop/monitor/archives';
    return;
  }

  $bucket = config_get('dropmonitor.settings', 's3_bucket');

  try {
    $s3->copy($bucket, $key, $new_key);
    $s3->delete($bucket, $key);
    watchdog('dropmonitor', 'Moved S3 object from %old to %new', array(
      '%old' => $key,
      '%new' => $new_key,
    ), WATCHDOG_INFO);
    backdrop_set_message(t('Moved %file to %dest.', array(
      '%file' => $filename,
      '%dest' => $destination !== '' ? rtrim($destination, '/') : t('bucket root'),
    )));
  }
  catch (Exception $e) {
    watchdog('dropmonitor', 'S3 move failed from %old to %new: @error', array(
      '%old' => $key,
      '%new' => $new_key,
      '@error' => $e->getMessage(),
    ), WATCHDOG_ERROR);
    backdrop_set_message(t('Move failed: @error', array('@error' => $e->getMessage())), 'error');
  }

  $redirect_query = !empty($destination) ? array('query' => array('prefix' => $destination)) : array();
  $form_state['redirect'] = array('drop/monitor/archives', $redirect_query);
}
