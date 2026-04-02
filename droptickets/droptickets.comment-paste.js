/**
 * @file
 * Handles clipboard image paste in the comment textarea.
 *
 * Intercepts paste events containing image data, uploads to the server,
 * and inserts an <img> tag at the cursor position. @mention autocomplete
 * continues to work unchanged since the textarea is not modified.
 */
(function ($) {
  Backdrop.behaviors.dropticketCommentImagePaste = {
    attach: function (context, settings) {
      var uploadUrl   = settings.droptickets && settings.droptickets.uploadUrl;
      var uploadToken = settings.droptickets && settings.droptickets.uploadToken;
      if (!uploadUrl) { return; }

      $('textarea.droptickets-comment-body', context)
        .once('image-paste')
        .on('paste', function (e) {
          var clipboardData = e.originalEvent.clipboardData || window.clipboardData;
          if (!clipboardData || !clipboardData.items) { return; }

          var imageItem = null;
          for (var i = 0; i < clipboardData.items.length; i++) {
            if (clipboardData.items[i].type.indexOf('image') === 0) {
              imageItem = clipboardData.items[i];
              break;
            }
          }
          if (!imageItem) { return; }

          e.preventDefault();

          var file      = imageItem.getAsFile();
          var $textarea = $(this);
          // Use a unique placeholder so we can replace it after upload.
          var placeholder = '\n[image-uploading-' + Date.now() + ']\n';
          insertAtCursor($textarea[0], placeholder);

          var formData = new FormData();
          formData.append('file', file, 'image.png');
          formData.append('token', uploadToken);

          $.ajax({
            url:         uploadUrl,
            type:        'POST',
            data:        formData,
            processData: false,
            contentType: false,
            success: function (data) {
              var val = $textarea.val();
              if (data.url) {
                $textarea.val(val.replace(placeholder, '\n<img src="' + data.url + '">\n'));
                addImagePreview($textarea, data.url);
              }
              else {
                $textarea.val(val.replace(placeholder, ''));
                addImagePreview($textarea, null);
              }
            },
            error: function () {
              var val = $textarea.val();
              $textarea.val(val.replace(placeholder, ''));
              addImagePreview($textarea, null);
            }
          });
        });
    }
  };

  /**
   * Adds or refreshes the image preview panel below the textarea.
   * Scans the textarea value for all <img> tags and renders thumbnails.
   */
  function addImagePreview($textarea, newUrl) {
    var $preview = $textarea.siblings('.droptickets-comment-img-preview');
    if (!$preview.length) {
      $preview = $('<div class="droptickets-comment-img-preview"></div>').insertAfter($textarea);
    }

    // Parse all img srcs from the current textarea value.
    var val  = $textarea.val();
    var srcs = [];
    var re   = /<img[^>]+src="([^"]+)"/gi;
    var m;
    while ((m = re.exec(val)) !== null) {
      srcs.push(m[1]);
    }

    if (srcs.length === 0) {
      $preview.hide().empty();
      return;
    }

    $preview.empty().show();
    $.each(srcs, function (i, src) {
      $preview.append('<img src="' + src + '" alt="">');
    });
  }

  /**
   * Inserts text at the current cursor position in a textarea.
   */
  function insertAtCursor(el, text) {
    if (el.selectionStart !== undefined) {
      var start = el.selectionStart;
      var end   = el.selectionEnd;
      el.value  = el.value.substring(0, start) + text + el.value.substring(end);
      el.selectionStart = el.selectionEnd = start + text.length;
    }
    else {
      el.value += text;
    }
  }
}(jQuery));
