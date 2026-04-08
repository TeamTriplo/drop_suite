<?php
/**
 * @file
 * Minimal S3 client for Drop Monitor archives.
 *
 * Based on mnapoli/simple-s3 (MIT license), adapted for PHP 7.4+
 * and extended with listObjects support.
 *
 * @see https://github.com/mnapoli/simple-s3
 */

class DropS3 {

  private $accessKeyId;
  private $secretKey;
  private $region;
  private $endpoint;
  private $timeoutInSeconds = 30;

  /**
   * @param string $accessKeyId
   * @param string $secretKey
   * @param string $region
   * @param string|null $endpoint
   *   Custom endpoint for S3-compatible services (e.g. Linode Object Storage).
   */
  public function __construct($accessKeyId, $secretKey, $region, $endpoint = NULL) {
    $this->accessKeyId = $accessKeyId;
    $this->secretKey = $secretKey;
    $this->region = $region;
    $this->endpoint = $endpoint;
  }

  /**
   * Sets the timeout for HTTP requests.
   */
  public function setTimeout($timeoutInSeconds) {
    $this->timeoutInSeconds = $timeoutInSeconds;
    return $this;
  }

  /**
   * Lists objects in a bucket.
   *
   * @param string $bucket
   * @param string $prefix
   *   Optional prefix to filter results.
   * @param string $delimiter
   *   Optional delimiter for hierarchical listing. Use '/' to get folder-like
   *   behavior where common prefixes are returned separately.
   *
   * @return array
   *   Associative array with keys:
   *   - 'objects': Array of objects, each with 'key', 'size', 'last_modified'.
   *   - 'prefixes': Array of common prefix strings (only when delimiter is set).
   */
  public function listObjects($bucket, $prefix = '', $delimiter = '') {
    $query = 'list-type=2';
    if ($prefix !== '') {
      $query .= '&prefix=' . rawurlencode($prefix);
    }
    if ($delimiter !== '') {
      $query .= '&delimiter=' . rawurlencode($delimiter);
    }

    list($status, $body) = $this->s3Request('GET', $bucket, '/', array(), '', TRUE, NULL, $query);

    $objects = array();
    $prefixes = array();
    if ($body) {
      $dom = new DOMDocument();
      $prior = libxml_use_internal_errors(TRUE);
      $dom->loadXML($body);
      libxml_clear_errors();
      libxml_use_internal_errors($prior);

      $contents = $dom->getElementsByTagName('Contents');
      foreach ($contents as $item) {
        $key = '';
        $size = 0;
        $modified = '';
        foreach ($item->childNodes as $child) {
          if ($child->nodeName === 'Key') {
            $key = $child->textContent;
          }
          elseif ($child->nodeName === 'Size') {
            $size = (int) $child->textContent;
          }
          elseif ($child->nodeName === 'LastModified') {
            $modified = $child->textContent;
          }
        }
        // Skip the prefix itself (S3 returns the "directory" object).
        if ($key !== '' && $key !== $prefix) {
          $objects[] = array(
            'key' => $key,
            'size' => $size,
            'last_modified' => $modified,
          );
        }
      }

      // Extract CommonPrefixes (folders).
      $commonPrefixes = $dom->getElementsByTagName('CommonPrefixes');
      foreach ($commonPrefixes as $cp) {
        foreach ($cp->childNodes as $child) {
          if ($child->nodeName === 'Prefix') {
            $prefixes[] = $child->textContent;
          }
        }
      }
    }

    return array(
      'objects' => $objects,
      'prefixes' => $prefixes,
    );
  }

  /**
   * Uploads content to S3.
   *
   * @param string $bucket
   * @param string $key
   * @param string|resource $content
   * @param array $headers
   *
   * @return array
   *   [status, body, headers].
   */
  public function put($bucket, $key, $content, $headers = array()) {
    return $this->s3Request('PUT', $bucket, $key, $headers, $content);
  }

  /**
   * Downloads an object from S3.
   *
   * @param string $bucket
   * @param string $key
   *
   * @return array
   *   [status, body, headers].
   */
  public function get($bucket, $key) {
    return $this->s3Request('GET', $bucket, $key);
  }

  /**
   * Generates a pre-signed URL for downloading an object.
   *
   * @param string $bucket
   * @param string $key
   * @param int $expires
   *   URL validity in seconds (default 300 = 5 minutes).
   * @param string|null $response_content_disposition
   *   Optional Content-Disposition override (e.g. 'attachment; filename="f"').
   *
   * @return string
   *   The pre-signed URL.
   */
  public function getPresignedUrl($bucket, $key, $expires = 300, $response_content_disposition = NULL) {
    $dateAsText = gmdate('Ymd');
    $timeAsText = gmdate('Ymd\THis\Z');
    $scope = $dateAsText . '/' . $this->region . '/s3/aws4_request';

    $uriPath = str_replace('%2F', '/', rawurlencode($key));
    $uriPath = '/' . ltrim($uriPath, '/');

    if ($this->endpoint) {
      $endpointHost = $this->getEndpointHost($this->endpoint);
      $hostname = $endpointHost ? $endpointHost : $this->getHostname($bucket);
      $uriPath = '/' . ltrim($bucket . $uriPath, '/');
    }
    else {
      $hostname = $this->getHostname($bucket);
    }

    $queryParams = array(
      'X-Amz-Algorithm' => 'AWS4-HMAC-SHA256',
      'X-Amz-Credential' => $this->accessKeyId . '/' . $scope,
      'X-Amz-Date' => $timeAsText,
      'X-Amz-Expires' => (string) $expires,
      'X-Amz-SignedHeaders' => 'host',
    );
    if ($response_content_disposition !== NULL) {
      $queryParams['response-content-disposition'] = $response_content_disposition;
    }

    ksort($queryParams);
    $queryParts = array();
    foreach ($queryParams as $k => $v) {
      $queryParts[] = rawurlencode($k) . '=' . rawurlencode($v);
    }
    $queryString = implode('&', $queryParts);

    $canonicalRequest = implode("\n", array(
      'GET',
      $uriPath,
      $queryString,
      'host:' . $hostname . "\n",
      'host',
      'UNSIGNED-PAYLOAD',
    ));

    $stringToSign = "AWS4-HMAC-SHA256\n" . $timeAsText . "\n" . $scope . "\n" . hash('sha256', $canonicalRequest);

    $dateKey = hash_hmac('sha256', $dateAsText, 'AWS4' . $this->secretKey, TRUE);
    $regionKey = hash_hmac('sha256', $this->region, $dateKey, TRUE);
    $serviceKey = hash_hmac('sha256', 's3', $regionKey, TRUE);
    $signingKey = hash_hmac('sha256', 'aws4_request', $serviceKey, TRUE);
    $signature = hash_hmac('sha256', $stringToSign, $signingKey);

    $url = ($this->endpoint ? $this->endpoint : 'https://' . $hostname) . $uriPath;
    $url .= '?' . $queryString . '&X-Amz-Signature=' . $signature;

    return $url;
  }

  /**
   * Copies an object within the same bucket.
   *
   * @param string $bucket
   * @param string $source_key
   * @param string $dest_key
   *
   * @return array
   *   [status, body, headers].
   */
  public function copy($bucket, $source_key, $dest_key) {
    $copy_source = '/' . $bucket . '/' . ltrim($source_key, '/');
    return $this->s3Request('PUT', $bucket, $dest_key, array(
      'x-amz-copy-source' => $copy_source,
    ));
  }

  /**
   * Deletes an object from S3.
   *
   * @param string $bucket
   * @param string $key
   *
   * @return array
   *   [status, body, headers].
   */
  public function delete($bucket, $key) {
    return $this->s3Request('DELETE', $bucket, $key);
  }

  /**
   * Sends a signed request to S3.
   */
  private function s3Request($httpVerb, $bucket, $key, $headers = array(), $body = '', $throwOn404 = TRUE, $responseBodyStream = NULL, $queryString = '') {
    // Sort query string parameters alphabetically (required by SigV4).
    if ($queryString !== '') {
      $params = explode('&', $queryString);
      sort($params);
      $queryString = implode('&', $params);
    }

    $headers = array_change_key_case($headers, CASE_LOWER);
    $uriPath = str_replace('%2F', '/', rawurlencode($key));
    $uriPath = '/' . ltrim($uriPath, '/');

    if ($this->endpoint) {
      $endpointHost = $this->getEndpointHost($this->endpoint);
      $hostname = $endpointHost ? $endpointHost : $this->getHostname($bucket);
      $headers['host'] = $hostname;
      $uriPath = '/' . ltrim($bucket . $uriPath, '/');
    }
    else {
      $hostname = $this->getHostname($bucket);
      $headers['host'] = $hostname;
    }

    $isStream = is_resource($body);
    $bodyHash = $isStream ? 'UNSIGNED-PAYLOAD' : hash('sha256', $body);

    if ($isStream && !isset($headers['content-length'])) {
      $stat = fstat($body);
      if (is_array($stat) && isset($stat['size']) && $stat['size'] >= 0) {
        $headers['content-length'] = (string) $stat['size'];
      }
    }

    $headers = $this->signRequest($httpVerb, $uriPath, $queryString, $headers, $bodyHash);

    if ($this->endpoint) {
      $url = $this->endpoint;
    }
    else {
      $url = 'https://' . $hostname;
    }
    $url = $url . $uriPath;
    if ($queryString !== '') {
      $url .= '?' . $queryString;
    }

    list($status, $responseBody, $responseHeaders) = $this->curlRequest($httpVerb, $url, $headers, $body, $isStream, $responseBodyStream);

    $shouldThrow404 = $throwOn404 && ($status === 404);
    if ($shouldThrow404 || $status < 200 || ($status >= 400 && $status !== 404)) {
      $errorMessage = '';
      if ($responseBody) {
        $dom = new DOMDocument();
        $prior = libxml_use_internal_errors(TRUE);
        $loaded = $dom->loadXML($responseBody);
        libxml_clear_errors();
        libxml_use_internal_errors($prior);
        if ($loaded && $dom->childNodes->item(0) && $dom->childNodes->item(0)->nodeName === 'Error') {
          $errorMessage = $dom->childNodes->item(0)->textContent;
        }
      }
      throw new RuntimeException('AWS S3 request failed: ' . $status . ' ' . $errorMessage);
    }

    return array($status, $responseBody, $responseHeaders);
  }

  /**
   * Executes a cURL request.
   */
  private function curlRequest($httpVerb, $url, $headers, $body, $isStream = FALSE, $responseBodyStream = NULL) {
    $curlHeaders = array();
    foreach ($headers as $name => $value) {
      $curlHeaders[] = $name . ': ' . $value;
    }

    $ch = curl_init($url);
    if (!$ch) {
      throw new RuntimeException('Could not create a CURL request');
    }

    $responseHeadersAsString = '';
    $curlOptions = array(
      CURLOPT_CUSTOMREQUEST => $httpVerb,
      CURLOPT_HTTPHEADER => $curlHeaders,
      CURLOPT_FOLLOWLOCATION => TRUE,
      CURLOPT_TIMEOUT => $this->timeoutInSeconds,
      CURLOPT_HEADERFUNCTION => function ($c, $data) use (&$responseHeadersAsString) {
        $responseHeadersAsString .= $data;
        return strlen($data);
      },
    );

    if ($responseBodyStream) {
      $curlOptions[CURLOPT_FILE] = $responseBodyStream;
      $curlOptions[CURLOPT_RETURNTRANSFER] = FALSE;
    }
    else {
      $curlOptions[CURLOPT_RETURNTRANSFER] = TRUE;
    }

    if ($isStream) {
      $curlOptions[CURLOPT_UPLOAD] = TRUE;
      $curlOptions[CURLOPT_INFILE] = $body;
      if (isset($headers['content-length'])) {
        $curlOptions[CURLOPT_INFILESIZE] = (int) $headers['content-length'];
      }
    }
    else {
      $curlOptions[CURLOPT_POSTFIELDS] = $body;
    }

    curl_setopt_array($ch, $curlOptions);
    $responseBody = curl_exec($ch);
    $status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $errno = curl_errno($ch);
    $error = curl_error($ch);
    curl_close($ch);

    if ($responseBody === FALSE || $errno > 0) {
      throw new RuntimeException('AWS S3 request failed: ' . $status . ' ' . $error);
    }

    $responseHeaders = array();
    foreach (explode("\r\n", $responseHeadersAsString) as $line) {
      $parts = explode(':', $line, 2);
      if (count($parts) === 2) {
        $responseHeaders[strtolower(trim($parts[0]))] = trim($parts[1]);
      }
    }

    if ($responseBodyStream) {
      return array($status, '', $responseHeaders);
    }

    return array($status, (string) $responseBody, $responseHeaders);
  }

  /**
   * Signs an S3 request using AWS Signature V4.
   */
  private function signRequest($httpVerb, $uriPath, $queryString, $headers, $bodyHash) {
    $dateAsText = gmdate('Ymd');
    $timeAsText = gmdate('Ymd\THis\Z');
    $scope = $dateAsText . '/' . $this->region . '/s3/aws4_request';

    $headers['x-amz-date'] = $timeAsText;
    $headers['x-amz-content-sha256'] = $bodyHash;

    ksort($headers, SORT_STRING | SORT_FLAG_CASE);

    $headerNamesAsString = implode(';', array_map('strtolower', array_keys($headers)));
    $headerString = '';
    foreach ($headers as $key => $value) {
      $headerString .= strtolower($key) . ':' . trim($value) . "\n";
    }

    $canonicalRequest = implode("\n", array(
      $httpVerb,
      $uriPath,
      $queryString,
      $headerString,
      $headerNamesAsString,
      $bodyHash,
    ));

    $stringToSign = "AWS4-HMAC-SHA256\n" . $timeAsText . "\n" . $scope . "\n" . hash('sha256', $canonicalRequest);

    $dateKey = hash_hmac('sha256', $dateAsText, 'AWS4' . $this->secretKey, TRUE);
    $regionKey = hash_hmac('sha256', $this->region, $dateKey, TRUE);
    $serviceKey = hash_hmac('sha256', 's3', $regionKey, TRUE);
    $signingKey = hash_hmac('sha256', 'aws4_request', $serviceKey, TRUE);
    $signature = hash_hmac('sha256', $stringToSign, $signingKey);

    $headers['authorization'] = 'AWS4-HMAC-SHA256 Credential=' . $this->accessKeyId . '/' . $scope
      . ',SignedHeaders=' . $headerNamesAsString
      . ',Signature=' . $signature;

    return $headers;
  }

  /**
   * Returns the S3 hostname for a bucket.
   */
  private function getHostname($bucketName) {
    if ($this->region === 'us-east-1') {
      return $bucketName . '.s3.amazonaws.com';
    }
    return $bucketName . '.s3-' . $this->region . '.amazonaws.com';
  }

  /**
   * Extracts the host from a custom endpoint URL.
   */
  private function getEndpointHost($endpoint) {
    $parts = parse_url($endpoint);
    if ($parts === FALSE) {
      return NULL;
    }
    return isset($parts['host']) ? $parts['host'] : NULL;
  }

}
