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

  /**
   * Behavior: collapsible upload dropzone toggle.
   */
  Backdrop.behaviors.dropfilesUploadToggle = {
    attach: function (context) {
      var toggle = document.getElementById('dropfiles-upload-toggle');
      var wrapper = document.getElementById('dropfiles-dropzone-wrapper');
      if (!toggle || !wrapper || toggle.dataset.initialized) {
        return;
      }
      toggle.dataset.initialized = '1';

      toggle.addEventListener('click', function () {
        var isVisible = wrapper.classList.contains('dropfiles-dropzone-wrapper--visible');
        if (isVisible) {
          wrapper.classList.remove('dropfiles-dropzone-wrapper--visible');
          toggle.classList.remove('dropfiles-upload-toggle--active');
          // After transition, hide fully.
          setTimeout(function () {
            if (!wrapper.classList.contains('dropfiles-dropzone-wrapper--visible')) {
              wrapper.style.display = 'none';
            }
          }, 300);
        }
        else {
          wrapper.style.display = 'block';
          // Force reflow before adding class for transition.
          wrapper.offsetHeight;
          wrapper.classList.add('dropfiles-dropzone-wrapper--visible');
          toggle.classList.add('dropfiles-upload-toggle--active');
        }
      });
    }
  };

  /**
   * Behavior: grid/list view toggle with localStorage persistence.
   */
  Backdrop.behaviors.dropfilesViewToggle = {
    attach: function (context) {
      var btns = document.querySelectorAll('.dropfiles-view-btn');
      var listView = document.querySelector('.dropfiles-list-view');
      var gridView = document.querySelector('.dropfiles-grid-view');
      if (!btns.length || !listView || !gridView) {
        return;
      }
      // Only initialize once.
      if (btns[0].dataset.initialized) {
        return;
      }
      btns[0].dataset.initialized = '1';

      function setView(mode) {
        if (mode === 'grid') {
          listView.style.display = 'none';
          gridView.style.display = '';
        }
        else {
          listView.style.display = '';
          gridView.style.display = 'none';
        }
        // Update active button.
        for (var i = 0; i < btns.length; i++) {
          if (btns[i].getAttribute('data-view') === mode) {
            btns[i].classList.add('dropfiles-view-btn--active');
          }
          else {
            btns[i].classList.remove('dropfiles-view-btn--active');
          }
        }
        try {
          localStorage.setItem('dropfiles_view', mode);
        }
        catch (e) {
          // localStorage unavailable; silently ignore.
        }
      }

      // Restore preference from localStorage.
      var saved = null;
      try {
        saved = localStorage.getItem('dropfiles_view');
      }
      catch (e) {
        // Ignore.
      }
      if (saved === 'grid' || saved === 'list') {
        setView(saved);
      }
      else {
        setView('grid');
      }

      // Bind click handlers.
      for (var i = 0; i < btns.length; i++) {
        btns[i].addEventListener('click', function () {
          setView(this.getAttribute('data-view'));
        });
      }
    }
  };

  Backdrop.behaviors.dropfilesUpload = {
    attach: function (context) {
      var dropzone = document.getElementById('dropfiles-dropzone');
      var fileInput = document.getElementById('dropfiles-file-input');
      if (!dropzone || !fileInput || dropzone.dataset.initialized) {
        return;
      }
      dropzone.dataset.initialized = '1';

      var settings = Backdrop.settings.dropfilesUpload || {};
      var uploadUrl = settings.uploadUrl;
      var token = settings.token;

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
        dropzone.classList.add('dropfiles-dropzone--active');
      });
      dropzone.addEventListener('dragleave', function () {
        dropzone.classList.remove('dropfiles-dropzone--active');
      });
      dropzone.addEventListener('drop', function (e) {
        e.preventDefault();
        dropzone.classList.remove('dropfiles-dropzone--active');
        if (e.dataTransfer.files.length > 0) {
          uploadFiles(e.dataTransfer.files);
        }
      });

      function showUploadError(msg) {
        var inner = dropzone.querySelector('.dropfiles-dropzone-inner');
        var progress = dropzone.querySelector('.dropfiles-upload-progress');
        var progressText = dropzone.querySelector('.dropfiles-progress-text');
        var progressBarFill = dropzone.querySelector('.dropfiles-progress-bar-fill');
        var progressDetail = dropzone.querySelector('.dropfiles-progress-detail');

        progressText.textContent = Backdrop.t('Error: @msg', {'@msg': msg});
        progressBarFill.style.width = '100%';
        progressBarFill.classList.add('dropfiles-progress-bar-fill--error');
        progressDetail.innerHTML = '<a href="#" class="dropfiles-dismiss-error">'
          + Backdrop.t('Dismiss') + '</a>';
        progressDetail.querySelector('.dropfiles-dismiss-error').addEventListener('click', function (e) {
          e.preventDefault();
          inner.style.display = '';
          progress.style.display = 'none';
          progressBarFill.classList.remove('dropfiles-progress-bar-fill--error');
        });
      }

      function uploadFiles(files) {
        var inner = dropzone.querySelector('.dropfiles-dropzone-inner');
        var progress = dropzone.querySelector('.dropfiles-upload-progress');
        var progressText = dropzone.querySelector('.dropfiles-progress-text');
        var progressBarFill = dropzone.querySelector('.dropfiles-progress-bar-fill');
        var progressDetail = dropzone.querySelector('.dropfiles-progress-detail');
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
            showUploadError(Backdrop.t('Upload failed. The server may have rejected the file or the connection was lost.'));
          };
          xhr.send(formData);
        }

        uploadNext();
      }
    }
  };
})();
