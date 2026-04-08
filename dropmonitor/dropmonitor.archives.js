(function () {
  'use strict';

  /**
   * Formats bytes as a human-readable string.
   */
  function formatSize(bytes) {
    if (bytes >= 1073741824) {
      return (bytes / 1073741824).toFixed(1) + ' GB';
    }
    if (bytes >= 1048576) {
      return (bytes / 1048576).toFixed(1) + ' MB';
    }
    if (bytes >= 1024) {
      return Math.round(bytes / 1024) + ' KB';
    }
    return bytes + ' B';
  }

  Backdrop.behaviors.dropmonitorArchives = {
    attach: function (context) {
      var dropzone = document.getElementById('dropmonitor-dropzone');
      var fileInput = document.getElementById('dropmonitor-file-input');
      if (!dropzone || !fileInput || dropzone.dataset.initialized) {
        return;
      }
      dropzone.dataset.initialized = '1';

      var settings = Backdrop.settings.dropmonitorArchives || {};
      var uploadUrl = settings.uploadUrl;
      var token = settings.token;
      var prefix = settings.prefix || '';

      // Click to select files.
      dropzone.addEventListener('click', function (e) {
        if (e.target === fileInput) return;
        fileInput.click();
      });

      fileInput.addEventListener('change', function () {
        if (fileInput.files.length > 0) {
          uploadFiles(fileInput.files);
        }
      });

      // Drag and drop events.
      dropzone.addEventListener('dragover', function (e) {
        e.preventDefault();
        dropzone.classList.add('dropmonitor-dropzone--active');
      });
      dropzone.addEventListener('dragleave', function () {
        dropzone.classList.remove('dropmonitor-dropzone--active');
      });
      dropzone.addEventListener('drop', function (e) {
        e.preventDefault();
        dropzone.classList.remove('dropmonitor-dropzone--active');
        if (e.dataTransfer.files.length > 0) {
          uploadFiles(e.dataTransfer.files);
        }
      });

      function showUploadError(msg) {
        var inner = dropzone.querySelector('.dropmonitor-dropzone-inner');
        var progress = dropzone.querySelector('.dropmonitor-upload-progress');
        var progressText = dropzone.querySelector('.dropmonitor-progress-text');
        var progressBarFill = dropzone.querySelector('.dropmonitor-progress-bar-fill');
        var progressDetail = dropzone.querySelector('.dropmonitor-progress-detail');

        progressText.textContent = Backdrop.t('Error: @msg', {'@msg': msg});
        progressBarFill.style.width = '100%';
        progressBarFill.classList.add('dropmonitor-progress-bar-fill--error');
        progressDetail.innerHTML = '<a href="#" class="dropmonitor-dismiss-error">'
          + Backdrop.t('Dismiss') + '</a>';
        progressDetail.querySelector('.dropmonitor-dismiss-error').addEventListener('click', function (e) {
          e.preventDefault();
          inner.style.display = '';
          progress.style.display = 'none';
          progressBarFill.classList.remove('dropmonitor-progress-bar-fill--error');
        });
      }

      function uploadFiles(files) {
        var inner = dropzone.querySelector('.dropmonitor-dropzone-inner');
        var progress = dropzone.querySelector('.dropmonitor-upload-progress');
        var progressText = dropzone.querySelector('.dropmonitor-progress-text');
        var progressBarFill = dropzone.querySelector('.dropmonitor-progress-bar-fill');
        var progressDetail = dropzone.querySelector('.dropmonitor-progress-detail');
        var queue = Array.prototype.slice.call(files);
        var total = queue.length;
        var completed = 0;

        inner.style.display = 'none';
        progress.style.display = 'block';
        progressBarFill.style.width = '0%';
        progressDetail.textContent = '';

        function uploadNext() {
          if (queue.length === 0) {
            progressBarFill.style.width = '100%';
            progressText.textContent = Backdrop.t('Done. Reloading...');
            progressDetail.textContent = '';
            setTimeout(function () { location.reload(); }, 500);
            return;
          }

          var file = queue.shift();
          completed++;
          progressText.textContent = Backdrop.t('Uploading @name (@n of @total)', {
            '@name': file.name,
            '@n': completed,
            '@total': total
          });
          progressBarFill.style.width = '0%';
          progressDetail.textContent = '0% \u2014 0 B / ' + formatSize(file.size);

          var formData = new FormData();
          formData.append('file', file);

          var xhr = new XMLHttpRequest();
          var url = uploadUrl + '?token=' + encodeURIComponent(token);
          if (prefix) {
            url += '&prefix=' + encodeURIComponent(prefix);
          }

          xhr.upload.addEventListener('progress', function (e) {
            if (e.lengthComputable) {
              var pct = Math.round((e.loaded / e.total) * 100);
              progressBarFill.style.width = pct + '%';
              progressDetail.textContent = pct + '% \u2014 '
                + formatSize(e.loaded) + ' / ' + formatSize(e.total);
            }
          });

          xhr.open('POST', url);
          xhr.onload = function () {
            var resp;
            try { resp = JSON.parse(xhr.responseText); } catch (e) { resp = {}; }
            if (xhr.status !== 200 || resp.error) {
              var msg = resp.error || Backdrop.t('Server returned status @status', {'@status': xhr.status});
              showUploadError(msg);
              return;
            }
            uploadNext();
          };
          xhr.onerror = function () {
            showUploadError(Backdrop.t('Upload failed. The server may have rejected the file (check size limits) or the connection was lost.'));
          };
          xhr.send(formData);
        }

        uploadNext();
      }
    }
  };
})();
