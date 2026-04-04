<?php
/**
 * @file
 * Drop Monitor agent — lightweight status endpoint for remote servers.
 *
 * Place this file on each monitored server (e.g. in the Backdrop docroot or a
 * known path). It collects local health data and returns JSON.
 *
 * Protected by a shared secret token passed as a query parameter.
 *
 * Usage:
 *   https://example.com/status.php?token=YOUR_SECRET_TOKEN
 *
 * Configuration:
 *   Set the TOKEN constant below to a random string. Use the same value in the
 *   Drop Monitor server settings on your support site.
 */

// ---- Configuration --------------------------------------------------------

define('DROPMONITOR_TOKEN', 'CHANGE_ME_TO_A_RANDOM_SECRET');

// Optional: path to the Backdrop docroot if this file is not in it.
// Leave empty to auto-detect relative to this file's location.
define('DROPMONITOR_BACKDROP_ROOT', '');

// ---------------------------------------------------------------------------

header('Content-Type: application/json');

// Token check.
if (!isset($_GET['token']) || $_GET['token'] !== DROPMONITOR_TOKEN) {
  http_response_code(403);
  echo json_encode(array('error' => 'Invalid token'));
  exit;
}

$data = array();

// ---- Disk space -----------------------------------------------------------

$root = '/';
$data['disk_free_gb'] = round(disk_free_space($root) / 1073741824, 2);
$data['disk_total_gb'] = round(disk_total_space($root) / 1073741824, 2);
$data['disk_used_percent'] = ($data['disk_total_gb'] > 0)
  ? round(100 - ($data['disk_free_gb'] / $data['disk_total_gb'] * 100), 1)
  : 0;

// ---- Pending OS updates ---------------------------------------------------

$data['pending_updates'] = 0;
$data['pending_security'] = 0;

$apt_output = @shell_exec('apt list --upgradable 2>/dev/null');
if ($apt_output) {
  $lines = array_filter(explode("\n", trim($apt_output)), function($line) {
    return $line && strpos($line, 'Listing...') === FALSE;
  });
  $data['pending_updates'] = count($lines);
  foreach ($lines as $line) {
    if (stripos($line, '-security') !== FALSE) {
      $data['pending_security']++;
    }
  }
}

// ---- PHP version ----------------------------------------------------------

$data['php_version'] = PHP_VERSION;

// ---- OS version -----------------------------------------------------------

$data['os_version'] = '';
if (is_readable('/etc/os-release')) {
  $os_release = parse_ini_file('/etc/os-release');
  if (!empty($os_release['PRETTY_NAME'])) {
    $data['os_version'] = $os_release['PRETTY_NAME'];
  }
}

// ---- Server uptime --------------------------------------------------------

$data['uptime_seconds'] = 0;
if (is_readable('/proc/uptime')) {
  $uptime_raw = file_get_contents('/proc/uptime');
  $parts = explode(' ', $uptime_raw);
  $data['uptime_seconds'] = (int) $parts[0];
  $data['uptime_days'] = round($parts[0] / 86400, 1);
}

// ---- Backdrop version -----------------------------------------------------

$backdrop_root = DROPMONITOR_BACKDROP_ROOT;
if (empty($backdrop_root)) {
  // Assume this file is in the Backdrop docroot.
  $backdrop_root = dirname(__FILE__);
}

$data['backdrop_version'] = '';
$bootstrap_file = $backdrop_root . '/core/includes/bootstrap.inc';
if (is_readable($bootstrap_file)) {
  $contents = file_get_contents($bootstrap_file);
  if (preg_match("/define\('BACKDROP_VERSION',\s*'([^']+)'\)/", $contents, $matches)) {
    $data['backdrop_version'] = $matches[1];
  }
}

// ---- Last cron run --------------------------------------------------------

$data['last_cron'] = 0;
$data['cron_age_hours'] = null;

// Try to read from Backdrop's state table.
$settings_file = $backdrop_root . '/settings.php';
if (is_readable($settings_file)) {
  $settings_contents = file_get_contents($settings_file);
  // Parse the $database connection string.
  if (preg_match("/\\\$database\s*=\s*'mysql:\/\/([^:]+):([^@]+)@([^\/]+)\/([^']+)'/", $settings_contents, $db_matches)) {
    $db_user = $db_matches[1];
    $db_pass = $db_matches[2];
    $db_host = $db_matches[3];
    $db_name = $db_matches[4];

    $conn = @new mysqli($db_host, $db_user, $db_pass, $db_name);
    if (!$conn->connect_error) {
      // Cron last run from state table.
      $result = $conn->query("SELECT value FROM state WHERE name = 'cron_last' LIMIT 1");
      if ($result && $row = $result->fetch_assoc()) {
        $cron_last = json_decode($row['value'], TRUE);
        if (is_numeric($cron_last)) {
          $data['last_cron'] = (int) $cron_last;
          $data['cron_age_hours'] = round((time() - $data['last_cron']) / 3600, 1);
        }
      }

      // Database size.
      $size_result = $conn->query("SELECT ROUND(SUM(data_length + index_length) / 1048576, 1) AS size_mb FROM information_schema.TABLES WHERE table_schema = '{$db_name}'");
      if ($size_result && $row = $size_result->fetch_assoc()) {
        $data['db_size_mb'] = (float) $row['size_mb'];
      }

      $conn->close();
    }
  }
}

// ---- Timestamp ------------------------------------------------------------

$data['collected_at'] = time();
$data['hostname'] = gethostname();

echo json_encode($data, JSON_PRETTY_PRINT);
